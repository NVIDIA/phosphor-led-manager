#include "config.hpp"
#include "gpio-access.hpp"
#include "gpio-physical.hpp"

#include <unistd.h>

#include <CLI/CLI.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/system/error_code.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/server/manager.hpp>

#include <cerrno>
#include <exception>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace
{
constexpr const char* busName = "xyz.openbmc_project.LED.NvidiaGpioLed";
constexpr const char* rootPath = "/xyz/openbmc_project/led/physical";
} // namespace

namespace phosphor::led::gpio
{

// Watches one LED's input GPIO fd and updates reported State on edges, sharing
// the io_context with D-Bus (phosphor-buttons stream_descriptor pattern).
class InputMonitor
{
  public:
    InputMonitor(const InputMonitor&) = delete;
    InputMonitor& operator=(const InputMonitor&) = delete;
    InputMonitor(InputMonitor&&) = delete;
    InputMonitor& operator=(InputMonitor&&) = delete;
    ~InputMonitor() = default;

    InputMonitor(boost::asio::io_context& io, GpioPhysical& ledArg) :
        led(ledArg), streamDesc(io)
    {
        int fd = led.gpio().inputEventFd();
        if (fd < 0)
        {
            lg2::error("No input event fd; state readback disabled for an LED");
            return;
        }

        // asio closes the descriptor it is handed; the gpiod line still owns
        // the original, so give asio a duplicate.
        int owned = ::dup(fd);
        if (owned < 0)
        {
            lg2::error("Unable to dup input event fd: {ERRNO}", "ERRNO", errno);
            return;
        }
        streamDesc.assign(owned);
        wait();
    }

  private:
    void wait()
    {
        streamDesc.async_wait(
            boost::asio::posix::stream_descriptor::wait_read,
            [this](const boost::system::error_code& ec) {
                if (ec == boost::asio::error::operation_aborted)
                {
                    return;
                }
                if (ec)
                {
                    throw std::system_error(ec, "input GPIO wait failed");
                }
                // Let failures escape to main: a partially working daemon is
                // worse than exiting and letting systemd restart it.
                led.gpio().readInputEvent();
                led.updateStateFromInput();
                wait();
            });
    }

    GpioPhysical& led;
    boost::asio::posix::stream_descriptor streamDesc;
};

} // namespace phosphor::led::gpio

int main(int argc, char** argv)
{
    using namespace phosphor::led::gpio;

    CLI::App app("nvidia-gpio-led");
    std::string configFile;
    app.add_option("-c,--config", configFile,
                   "Path to nvidia-gpio-led JSON config")
        ->required();
    CLI11_PARSE(app, argc, argv);

    std::string raw;
    {
        std::ifstream ifs(configFile);
        if (!ifs)
        {
            lg2::error("Unable to open config file {FILE}", "FILE", configFile);
            return 1;
        }
        std::stringstream ss;
        ss << ifs.rdbuf();
        raw = ss.str();
    }

    GpioLedConfig cfg;
    try
    {
        cfg = parseAndValidate(raw);
    }
    catch (const ConfigError& e)
    {
        lg2::error("Invalid nvidia-gpio-led config: {ERR}", "ERR", e.what());
        return 1;
    }

    boost::asio::io_context io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::server::manager_t objManager(*conn, rootPath);

    std::vector<std::unique_ptr<GpioPhysical>> leds;
    std::vector<std::unique_ptr<InputMonitor>> monitors;
    leds.reserve(cfg.leds.size());
    monitors.reserve(cfg.leds.size());

    for (const auto& entry : cfg.leds)
    {
        const std::string objPath = std::string(rootPath) + "/" + entry.name;
        try
        {
            auto access = std::make_unique<LibgpiodAccess>(entry);
            auto led = std::make_unique<GpioPhysical>(*conn, objPath,
                                                      std::move(access));
            monitors.push_back(std::make_unique<InputMonitor>(io, *led));
            leds.push_back(std::move(led));
            lg2::info("Registered GPIO LED {NAME} at {PATH}", "NAME",
                      entry.name, "PATH", objPath);
        }
        catch (const std::exception& e)
        {
            lg2::error("Failed to set up LED {NAME}: {ERR}", "NAME", entry.name,
                       "ERR", e.what());
            return 1;
        }
    }

    conn->request_name(busName);

    try
    {
        io.run();
    }
    catch (const std::exception& e)
    {
        lg2::error("GPIO LED monitoring failed, exiting: {ERR}", "ERR",
                   e.what());
        return 1;
    }
    return 0;
}
