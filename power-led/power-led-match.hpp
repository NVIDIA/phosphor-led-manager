#include <sdbusplus/bus.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>

using primarycode_t = uint64_t;
using secondarycode_t = std::vector<uint8_t>;
using postcode_t = std::tuple<primarycode_t, secondarycode_t>;
namespace StateServer = sdbusplus::xyz::openbmc_project::State::server;

// Singleton holder to store host/node and other path information
class PostCodeDataHolder
{
    PostCodeDataHolder()
    {}

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
};

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

    PostCodeDataHolder postcodeDataHolderObj =
        PostCodeDataHolder::getInstance();

  public:
    PowerLEDMatch(sdbusplus::bus::bus& bus, postcode_handler_t postcode_handler,
                  host_state_handler_t host_state_handler,
                  EventPtr& /*event*/) :
        bus(bus),
        propertiesChangedSignalRaw(
            bus,
            sdbusplus::bus::match::rules::type::signal() +
                sdbusplus::bus::match::rules::member("PropertiesChanged") +
                sdbusplus::bus::match::rules::path(
                    postcodeDataHolderObj.PostCodePath +
                    std::to_string(postcodeDataHolderObj.node)) +
                sdbusplus::bus::match::rules::interface(
                    postcodeDataHolderObj.PropertiesIntf),
            [this, postcode_handler](sdbusplus::message::message& msg) {
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
                    postcodeDataHolderObj.HostStatePathPrefix +
                    std::to_string(postcodeDataHolderObj.node)) +
                sdbusplus::bus::match::rules::interface(
                    postcodeDataHolderObj.PropertiesIntf),
            [this, host_state_handler](sdbusplus::message::message& msg) {
                std::string objectName;
                std::map<std::string, std::variant<std::string>> msgData;
                msg.read(objectName, msgData);
                // Check if it was the Value property that changed.
                auto valPropMap = msgData.find("CurrentHostState");
                {
                    if (valPropMap != msgData.end())
                    {
                        StateServer::Host::HostState currentHostState =
                            StateServer::Host::convertHostStateFromString(
                                std::get<std::string>(valPropMap->second));
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
    {}

    PowerLEDMatch() = delete; // no default constructor
    ~PowerLEDMatch() = default;
    PowerLEDMatch(const PowerLEDMatch&) = delete;
    PowerLEDMatch& operator=(const PowerLEDMatch&) = delete;
    PowerLEDMatch(PowerLEDMatch&&) = default;
    PowerLEDMatch& operator=(PowerLEDMatch&&) = default;

  protected:
    sdbusplus::bus::bus& bus;
    sdbusplus::bus::match_t propertiesChangedSignalRaw;
    sdbusplus::bus::match_t propertiesChangedSignalCurrentHostState;
};
