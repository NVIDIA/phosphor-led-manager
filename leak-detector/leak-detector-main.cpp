#include "leak-detector-config.hpp"
#include "leak-detector-monitor.hpp"

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    CLI::App app("leak-detector-led-controller");
    std::string configPath;
    app.add_option("-c,--config", configPath,
                   "Path to leak detector JSON config (optional). "
                   "When omitted, built-in defaults are used.");
    CLI11_PARSE(app, argc, argv);

    auto config = phosphor::led::leakdetector::config::loadConfig(configPath);

    /** @brief D-Bus connection used by the leak detector monitor. */
    sdbusplus::bus_t bus = sdbusplus::bus::new_default();

    phosphor::led::leakdetector::monitor::Monitor monitor(
        bus, std::move(config));

    /** @brief Wait for and dispatch D-Bus signals. */
    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
