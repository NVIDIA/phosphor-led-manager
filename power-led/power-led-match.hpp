#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

using primarycode_t = std::vector<uint8_t>;
using secondarycode_t = std::vector<uint8_t>;
using postcode_t = std::tuple<primarycode_t, secondarycode_t>;
namespace StateServer = sdbusplus::xyz::openbmc_project::State::server;

// Singleton holder to store host/node and other path information
class PostCodeDataHolder
{
    PostCodeDataHolder() = default;

  public:
    static PostCodeDataHolder& getInstance()
    {
        static PostCodeDataHolder instance;
        return instance;
    }

    int node;

    const static constexpr char* PostCodePath =
        "/xyz/openbmc_project/state/boot/raw";
    const static constexpr char* PropertiesIntf =
        "org.freedesktop.DBus.Properties";
    const static constexpr char* PostCodeListPathPrefix =
        "/var/lib/phosphor-post-code-manager/host";
    const static constexpr char* HostStatePathPrefix =
        "/xyz/openbmc_project/state/host";
    const static constexpr char* OsStatusIntf =
        "xyz.openbmc_project.State.OperatingSystem.Status";
    const static constexpr char* OsStatusStandby =
        "xyz.openbmc_project.State.OperatingSystem.Status.OSStatus.Standby";
};

inline bool osStateIsStandby(const std::string& state)
{
    return state == PostCodeDataHolder::OsStatusStandby;
}

struct EventDeleter
{
    void operator()(sd_event* event) const
    {
        sd_event_unref(event);
    }
};
using EventPtr = std::unique_ptr<sd_event, EventDeleter>;

/*
 * Class to check for changes to Chassis Power Status
 * Adapted from phosphor-post-cod-manager
 */
class PowerLEDMatch
{
    using postcode_handler_t = std::function<void(std::vector<postcode_t>)>;
    using host_state_handler_t = std::function<void(bool)>;
    using os_state_handler_t = std::function<void(bool)>;

    PostCodeDataHolder postcodeDataHolderObj =
        PostCodeDataHolder::getInstance();

  public:
    PowerLEDMatch(sdbusplus::bus::bus& bus, postcode_handler_t postcode_handler,
                  host_state_handler_t host_state_handler,
                  EventPtr& /*event*/,
                  os_state_handler_t os_state_handler = nullptr) :
        bus(bus),
        propertiesChangedSignalRaw(
            bus,
            sdbusplus::bus::match::rules::type::signal() +
                sdbusplus::bus::match::rules::member("PropertiesChanged") +
                sdbusplus::bus::match::rules::path(
                    PostCodeDataHolder::PostCodePath +
                    std::to_string(postcodeDataHolderObj.node)) +
                sdbusplus::bus::match::rules::interface(
                    PostCodeDataHolder::PropertiesIntf),
            [postcode_handler](sdbusplus::message::message& msg) {
                std::string objectName;
                std::map<std::string, std::variant<postcode_t>> msgData;
                msg.read(objectName, msgData);
                // Check if it was the Value property that changed.
                auto valPropMap = msgData.find("Value");
                {
                    if (valPropMap != msgData.end())
                    {
                        std::vector<postcode_t> code = {
                            std::get<postcode_t>(valPropMap->second)};
                        postcode_handler(code);
                    }
                }
            }),
        propertiesChangedSignalCurrentHostState(
            bus,
            sdbusplus::bus::match::rules::type::signal() +
                sdbusplus::bus::match::rules::member("PropertiesChanged") +
                sdbusplus::bus::match::rules::path(
                    PostCodeDataHolder::HostStatePathPrefix +
                    std::to_string(postcodeDataHolderObj.node)) +
                sdbusplus::bus::match::rules::interface(
                    PostCodeDataHolder::PropertiesIntf),
            [host_state_handler](sdbusplus::message::message& msg) {
                std::string objectName;
                std::map<std::string, std::variant<std::string>> msgData;
                msg.read(objectName, msgData);
                // Check if it was the Value property that changed.
                auto valPropMap = msgData.find("CurrentHostState");
                {
                    if (valPropMap != msgData.end())
                    {
                        const std::string& hostStateStr =
                            std::get<std::string>(valPropMap->second);
                        StateServer::Host::HostState currentHostState;
                        try
                        {
                            currentHostState =
                                StateServer::Host::convertHostStateFromString(
                                    hostStateStr);
                        }
                        catch (const sdbusplus::exception::InvalidEnumString& e)
                        {
                            lg2::warning(
                                "Ignoring CurrentHostState change: invalid or empty value. WHAT={WHAT}",
                                "WHAT", e.what());
                            return;
                        }
                        if (currentHostState ==
                            StateServer::Host::HostState::Off)
                        {
                            host_state_handler(false);
                        }
                        else
                        {
                            host_state_handler(true);
                        }
                    }
                }
            })
    {
        // Optional: report when the OS status reaches Standby (POST complete)
        if (os_state_handler)
        {
            propertiesChangedSignalOsState =
                std::make_unique<sdbusplus::bus::match_t>(
                    bus,
                    sdbusplus::bus::match::rules::type::signal() +
                        sdbusplus::bus::match::rules::member(
                            "PropertiesChanged") +
                        sdbusplus::bus::match::rules::path(
                            PostCodeDataHolder::HostStatePathPrefix +
                            std::to_string(postcodeDataHolderObj.node)) +
                        sdbusplus::bus::match::rules::interface(
                            PostCodeDataHolder::PropertiesIntf) +
                        sdbusplus::bus::match::rules::argN(
                            0, PostCodeDataHolder::OsStatusIntf),
                    [os_state_handler](sdbusplus::message::message& msg) {
                        std::string objectName;
                        std::map<std::string, std::variant<std::string>>
                            msgData;
                        msg.read(objectName, msgData);
                        auto it = msgData.find("OperatingSystemState");
                        if (it != msgData.end())
                        {
                            os_state_handler(osStateIsStandby(
                                std::get<std::string>(it->second)));
                        }
                    });
        }
    }

    PowerLEDMatch() = delete; // no default constructor
    ~PowerLEDMatch() = default;
    PowerLEDMatch(const PowerLEDMatch&) = delete;
    PowerLEDMatch& operator=(const PowerLEDMatch&) = delete;
    PowerLEDMatch(PowerLEDMatch&&) = delete;
    PowerLEDMatch& operator=(PowerLEDMatch&&) = delete;

  protected:
    sdbusplus::bus::bus& bus;
    sdbusplus::bus::match_t propertiesChangedSignalRaw;
    sdbusplus::bus::match_t propertiesChangedSignalCurrentHostState;
    std::unique_ptr<sdbusplus::bus::match_t> propertiesChangedSignalOsState;
};
