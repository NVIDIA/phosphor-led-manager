#include "power-led-match.hpp"

#include <CLI/CLI.hpp>
#include <boost/asio/io_service.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/State/Boot/PostCode/server.hpp>
#include <xyz/openbmc_project/State/Chassis/server.hpp>
#include <xyz/openbmc_project/State/Host/server.hpp>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <sstream>

#define HOST_OBJECT_NAME "/xyz/openbmc_project/state/host0"
#define HOST_SERVICE_NAME "xyz.openbmc_project.State.Host"
#define HOST_INTERFACE_NAME "xyz.openbmc_project.State.Host"
#define PROPERTY_INTERFACE_NAME "org.freedesktop.DBus.Properties"

std::string BMC_booted_group = "";
std::string post_active_group = "";
std::string fully_powered_on_group = "";

namespace properties
{
constexpr const char* interface = "org.freedesktop.DBus.Properties";
constexpr const char* get = "Get";
constexpr const char* set = "Set";
} // namespace properties

using namespace sdbusplus::xyz::openbmc_project::State::server;

// Get bytes 6-14 of postcode (these are the relevant bytes)
auto POST_start = std::vector<uint8_t>{};
auto POST_end = std::vector<uint8_t>{};
// Vars to keep track of whether system entered post.
std::atomic<bool> started_post = false;
std::atomic<bool> ended_post = false;
std::atomic<bool> host_power_on = false;

boost::asio::io_service io;
std::shared_ptr<sdbusplus::asio::connection> conn;

static void updatePowerLed();
void setLedGroup(const std::shared_ptr<sdbusplus::asio::connection>& conn,
                 const std::string& name, bool on);

// Will throw an exception if number is not formatted as a list of uint8_t
static std::vector<uint8_t>
    convertStringVectToHexVect(std::vector<std::string> input)
{
    std::vector<uint8_t> result;
    for (std::string elem : input)
    {
        // Needs to be a 8 bit number or exception
        result.push_back(std::stoul(elem, nullptr, 16));
    }

    return result;
}

static bool readConfig(std::string configFile)
{
    if (configFile == "")
    {
        lg2::error("Power LED controller config argument not provided.");
        return false;
    }
    nlohmann::json config_json;
    try
    {
        std::ifstream config_ifstream{configFile};
        config_json = nlohmann::json::parse(config_ifstream, nullptr, true);
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Error parsing power-led-controller config file: unable to open the file or read the json.");
        return false;
    }
    try
    {
        POST_start = convertStringVectToHexVect(
            std::vector<std::string>(config_json["POST_start"]));
        POST_end = convertStringVectToHexVect(
            std::vector<std::string>(config_json["POST_end"]));
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Error parsing power-led-controller config file: Couldn't parse the provided POST codes. Be sure the code is a valid list of strings (one byte in hex per string).");
        return false;
    }
    try
    {
        post_active_group = config_json["POST_active_group"];
        fully_powered_on_group = config_json["fully_powered_on_group"];
        BMC_booted_group = config_json["BMC_booted_group"];
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Error parsing power-led-controller config file: couldn't parse LED group names. Be sure to use the correct keys.");
        return false;
    }
    if (POST_start.empty() || POST_end.empty() || post_active_group == "" ||
        fully_powered_on_group == "" || BMC_booted_group == "")
    {
        lg2::error(
            "Error parsing power-led-controller config file. Something might be missing/empty.");
        return false;
    }
    return true;
}

/* Compares 2 postcodes in the form of uint8 vectors, ignoring the instance
   (last) byte If one code is longer, then skip bytes until they "line up"
    Returns 0 if equivalent, 1 if not, -1 if error.
    Codes must be 9 or fewer bytes
*/
static int checkSameCode(std::vector<uint8_t> a, std::vector<uint8_t> b)
{
    // Make sure both codes are not too long
    if (a.size() > 9 || b.size() > 9 || a.size() == 0 || b.size() == 0)
    {
        lg2::error("POST code of invalid length provided for comparison.");
        return -1;
    }
    // Figure out which code is longer and which is shorter
    auto& shorter_code = (a.size() >= b.size()) ? b : a;
    auto& longer_code = (a.size() >= b.size()) ? a : b;
    auto longer_it = longer_code.begin();
    auto shorter_it = shorter_code.begin();

    // Advance the longer iterator so both end at the same place
    // In other words, skip the bytes that the shorter doesn't have
    advance(longer_it, longer_code.size() - shorter_code.size() - 1);
    // Compare the elements until one doesn't match
    for (; longer_it != longer_code.end(), shorter_it != shorter_code.end();
         longer_it++, shorter_it++)
    {
        if (*longer_it != *shorter_it)
        {
            return 1;
        }
    }
    return 0;
}

/*
 * Handler for when new POST codes are received.
 * If POST_start or POST_end are seen, then
 * those booleans are updated and LED update function called
 * so that the LEDs can update accordingly.
 */
static void updatePostcodeStatus(std::vector<postcode_t> postcodes)
{
    // Loop through all provided postcodes
    bool change = false;
    for (auto& postcode : postcodes)
    {
        // Get the postcode
        std::vector<uint8_t> code = std::get<1>(postcode);
        // If the code is POST_start, then post has started
        if (!started_post && checkSameCode(code, POST_start) == 0)
        {
            started_post = true;
            ended_post =
                false; // Reset ended_post until it's found AFTER POST_start
            change = true;
        }
        // If the code is POST_end, then post has ended
        else if (!ended_post && checkSameCode(code, POST_end) == 0)
        {
            ended_post = true;
            change = true;
        }
        // No relevant post code or power state change. Don't change LED
    }
    if (change)
    {
        updatePowerLed();
    }
}

/*
 * Handler for when a power status change is detected.
 * If the power state changes, then the LED should be updated
 * accordingly.
 */
static void updatePowerStatus(bool isPowerOn)
{
    // If power state changes, any existing postcodes are irrelevant
    if (host_power_on != isPowerOn)
    {
        if (!isPowerOn)
        {
            lg2::info("Power LED: Host powering off. Resetting LED.");
            started_post = false;
            ended_post = false;
        }

        // Update booleans and LEDs if necessary
        host_power_on = isPowerOn;
        updatePowerLed();
    }
}

/*
 * This function updates the LED groups based on the
 * power state and which POST codes have been seen.
 */
static void updatePowerLed()
{
    // Update LED based on power status and what stage has been reached
    // Host is off or POST hasn't started: standby mode
    lg2::info("Updating power LED");
    if (!host_power_on || !started_post)
    {
        // LED = flash 1hz
        lg2::info("Power LED in standby mode (BMC booted)");
        setLedGroup(conn, post_active_group, false);
        setLedGroup(conn, fully_powered_on_group, false);
        setLedGroup(conn, BMC_booted_group, true);
    }
    // System in POST (POST_start code has been received)
    else if (host_power_on && started_post && !ended_post)
    {
        // LED = flash 4hz
        lg2::info("Power LED in POST mode");
        setLedGroup(conn, BMC_booted_group, false);
        setLedGroup(conn, post_active_group, true);
        setLedGroup(conn, fully_powered_on_group, false);
    }
    // Power on, POST completed (POST_end code has been received)
    else if (host_power_on && started_post && ended_post)
    {
        // LED = on;
        lg2::info("Power LED solid on (POST completed)");
        setLedGroup(conn, post_active_group, false);
        setLedGroup(conn, fully_powered_on_group, true);
    }
}

/*
Taken from Phosphor Button Handler.
Returns true if chassis is on, false if off.
*/
bool poweredOn(sdbusplus::bus::bus& bus)
{
    auto method = bus.new_method_call(HOST_SERVICE_NAME, HOST_OBJECT_NAME,
                                      PROPERTY_INTERFACE_NAME, "Get");
    method.append(HOST_INTERFACE_NAME, "CurrentHostState");
    auto result = bus.call(method);

    std::variant<std::string> state;
    result.read(state);

    return !(StateServer::Host::HostState::Off ==
             StateServer::Host::convertHostStateFromString(
                 std::get<std::string>(state)));
}

/*
 * Gets a vector of all POST codes that are current to this boot cycle.
 */
std::vector<postcode_t> getPostCodesFromDBUS(sdbusplus::bus::bus& bus)
{
    std::vector<postcode_t> codeArray;
    auto method = bus.new_method_call(
        "xyz.openbmc_project.State.Boot.PostCode0",
        "/xyz/openbmc_project/State/Boot/PostCode0",
        "xyz.openbmc_project.State.Boot.PostCode", "GetPostCodes");
    method.append(static_cast<uint16_t>(1));
    auto result = bus.call(method);
    if (result.is_method_error())
    {
        lg2::error("Could not get POST codes for Power LED Controller");
        return std::vector<postcode_t>();
    }
    result.read(codeArray);

    return codeArray;
}

/*
 * Sets an LED Group to true or false
 */
void setLedGroup(const std::shared_ptr<sdbusplus::asio::connection>& conn,
                 const std::string& name, bool on)
{
    conn->async_method_call(
        [name](const boost::system::error_code ec) {
        if (ec)
        {
            std::cerr << "Failed to set LED " << name << "\n";
        }
    },
        "xyz.openbmc_project.LED.GroupManager",
        "/xyz/openbmc_project/led/groups/" + name, properties::interface,
        properties::set, "xyz.openbmc_project.Led.Group", "Asserted",
        std::variant<bool>(on));
}

/*
 * This is the entry point for the application.
 *
 * This application simply creates an object that registers for incoming value
 * updates for the POST code dbus object.
 */
int main(int argc, char** argv)
{
    int ret = 0;

    sd_event* event = nullptr;
    ret = sd_event_default(&event);
    if (ret < 0)
    {
        lg2::error("Error creating a default sd_event handler");
        return ret;
    }
    EventPtr eventP{event};
    event = nullptr;

    conn = std::make_shared<sdbusplus::asio::connection>(io);
    auto bus = sdbusplus::bus::new_default();

    CLI::App app("power-LED-controller");
    std::string configFile{};
    app.add_option("-c,--config", configFile, "Path to power LED JSON config");
    CLI11_PARSE(app, argc, argv);
    // Read config. Stop execution if failed
    lg2::info("Parsing power LED controller config.");
    if (!readConfig(configFile))
    {
        lg2::info(
            "Power LED config not provided or invalid. Exiting Power LED controller.");
        return 0;
    }
    lg2::info("Successfully parsed power LED controller config.");
    // Initialize power status
    host_power_on = poweredOn(bus);
    // Create snooping objects for postcodes and power status
    PowerLEDMatch powerLEDmatch(bus, updatePostcodeStatus, updatePowerStatus,
                                eventP);
    updatePostcodeStatus(getPostCodesFromDBUS(bus));

    try
    {
        bus.attach_event(eventP.get(), SD_EVENT_PRIORITY_NORMAL);
        ret = sd_event_loop(eventP.get());
        if (ret < 0)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Error occurred during the sd_event_loop",
                phosphor::logging::entry("RET=%d", ret));
        }
    }
    catch (const std::exception& e)
    {
        lg2::error(e.what());
        return -1;
    }

    return 0;
}
