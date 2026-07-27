#include "leak-detector-monitor.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Led/Group/common.hpp>
#include <xyz/openbmc_project/State/LeakDetector/common.hpp>

#include <algorithm>

namespace phosphor
{
namespace led
{
namespace leakdetector
{
namespace monitor
{

namespace
{

using LeakDetector =
    sdbusplus::common::xyz::openbmc_project::state::LeakDetector;
using LedGroup = sdbusplus::common::xyz::openbmc_project::led::Group;

constexpr auto LEAK_DETECTOR_INTERFACE = LeakDetector::interface;
constexpr auto DETECTOR_STATE_PROPERTY = "DetectorState";

const auto CRITICAL_STATE = LeakDetector::convertDetectorStateEnumToString(
    LeakDetector::DetectorStateEnum::Critical);

constexpr auto LED_GROUP_PATH_PREFIX = "/xyz/openbmc_project/led/groups/";
constexpr auto LED_GROUP_INTERFACE = LedGroup::interface;
constexpr auto LED_GROUP_ASSERTED_PROPERTY = "Asserted";

constexpr auto OBJECT_ROOT = "/xyz/openbmc_project/state";

bool isCriticalState(const std::string& state)
{
    return state == CRITICAL_STATE;
}

} // namespace

Monitor::Monitor(sdbusplus::bus_t& busIn, config::Config configIn) :
    bus(busIn), config(std::move(configIn)),
    propertiesChangedMatch(
        bus,
        sdbusplus::bus::match::rules::propertiesChangedNamespace(
            OBJECT_ROOT, LEAK_DETECTOR_INTERFACE),
        [this](sdbusplus::message_t& m) { onPropertiesChanged(m); }),
    interfacesAddedMatch(
        bus, sdbusplus::bus::match::rules::interfacesAddedAtPath(OBJECT_ROOT),
        [this](sdbusplus::message_t& m) { onInterfacesAdded(m); }),
    interfacesRemovedMatch(
        bus, sdbusplus::bus::match::rules::interfacesRemovedAtPath(OBJECT_ROOT),
        [this](sdbusplus::message_t& m) { onInterfacesRemoved(m); })
{
    processExistingDetectors();
}

void Monitor::processExistingDetectors()
{
    std::vector<std::string> paths;
    try
    {
        paths = phosphor::led::utils::DBusHandler::getSubTreePaths(
            OBJECT_ROOT, LEAK_DETECTOR_INTERFACE);
    }
    catch (const sdbusplus::exception_t& e)
    {
        // Likely no LeakDetector objects exist yet. That's fine; we'll
        // pick them up when InterfacesAdded fires.
        lg2::info("No existing leak detectors found, ERROR = {ERROR}", "ERROR",
                  e);
        return;
    }

    for (const auto& path : paths)
    {
        try
        {
            auto value = phosphor::led::utils::DBusHandler::getProperty(
                path, LEAK_DETECTOR_INTERFACE, DETECTOR_STATE_PROPERTY);
            const auto* state = std::get_if<std::string>(&value);
            if (state == nullptr)
            {
                lg2::error(
                    "DetectorState property is not a string, PATH = {PATH}",
                    "PATH", path);
                continue;
            }
            updateDetectorState(path, *state);
        }
        catch (const sdbusplus::exception_t& e)
        {
            lg2::error(
                "Failed to read DetectorState, PATH = {PATH}, ERROR = {ERROR}",
                "PATH", path, "ERROR", e);
        }
    }
}

void Monitor::updateDetectorState(const std::string& path,
                                  const std::string& state)
{
    const bool wasEmpty = criticalDetectors.empty();

    if (isCriticalState(state))
    {
        const auto [_, inserted] = criticalDetectors.insert(path);
        if (inserted)
        {
            lg2::info("Leak detector entered Critical state, PATH = {PATH}",
                      "PATH", path);
        }
    }
    else
    {
        const auto erased = criticalDetectors.erase(path);
        if (erased > 0)
        {
            lg2::info(
                "Leak detector left Critical state, PATH = {PATH}, STATE = {STATE}",
                "PATH", path, "STATE", state);
        }
    }

    const bool isEmpty = criticalDetectors.empty();
    if (wasEmpty && !isEmpty)
    {
        setLeakCriticalLed(true);
    }
    else if (!wasEmpty && isEmpty)
    {
        setLeakCriticalLed(false);
    }
}

void Monitor::forgetDetector(const std::string& path)
{
    const bool wasEmpty = criticalDetectors.empty();
    const auto erased = criticalDetectors.erase(path);
    if (erased == 0)
    {
        return;
    }

    lg2::info("Leak detector removed while Critical, PATH = {PATH}", "PATH",
              path);

    if (!wasEmpty && criticalDetectors.empty())
    {
        setLeakCriticalLed(false);
    }
}

void Monitor::onPropertiesChanged(sdbusplus::message_t& msg)
{
    try
    {
        auto value = phosphor::led::utils::DBusHandler::getProperty(
            msg.get_path(), LEAK_DETECTOR_INTERFACE, DETECTOR_STATE_PROPERTY);
        const auto* state = std::get_if<std::string>(&value);
        if (state == nullptr)
        {
            lg2::error("DetectorState property is not a string, PATH = {PATH}",
                       "PATH", msg.get_path());
            return;
        }
        updateDetectorState(msg.get_path(), *state);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to read DetectorState on PropertiesChanged, PATH = {PATH}, ERROR = {ERROR}",
            "PATH", msg.get_path(), "ERROR", e);
    }
}

void Monitor::onInterfacesAdded(sdbusplus::message_t& msg)
{
    sdbusplus::object_path objectPath;
    try
    {
        msg.read(objectPath);
    }
    catch (const sdbusplus::exception_t&)
    {
        return;
    }

    try
    {
        auto value = phosphor::led::utils::DBusHandler::getProperty(
            objectPath.str, LEAK_DETECTOR_INTERFACE, DETECTOR_STATE_PROPERTY);
        const auto* state = std::get_if<std::string>(&value);
        if (state == nullptr)
        {
            lg2::error("DetectorState property is not a string, PATH = {PATH}",
                       "PATH", objectPath.str);
            return;
        }
        updateDetectorState(objectPath.str, *state);
    }
    catch (const sdbusplus::exception_t&)
    {
        // Not a leak detector or not yet readable -- ignore.
    }
}

void Monitor::onInterfacesRemoved(sdbusplus::message_t& msg)
{
    sdbusplus::object_path objectPath;
    std::vector<std::string> interfaces;
    try
    {
        msg.read(objectPath, interfaces);
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("Failed to parse InterfacesRemoved, ERROR = {ERROR}",
                   "ERROR", e);
        return;
    }

    if (std::find(interfaces.begin(), interfaces.end(),
                  LEAK_DETECTOR_INTERFACE) == interfaces.end())
    {
        return;
    }

    forgetDetector(objectPath.str);
}

void Monitor::setLeakCriticalLed(bool asserted)
{
    const auto* assertedStr = asserted ? "true" : "false";
    const std::string ledPath =
        std::string{LED_GROUP_PATH_PREFIX} + config.criticalLedGroup;

    lg2::info(
        "Updating leak critical LED group, GROUP = {GROUP}, ASSERTED = {ASSERTED}",
        "GROUP", config.criticalLedGroup, "ASSERTED", assertedStr);
    try
    {
        phosphor::led::utils::DBusHandler::setProperty(
            ledPath, LED_GROUP_INTERFACE, LED_GROUP_ASSERTED_PROPERTY,
            phosphor::led::utils::PropertyValue{asserted});
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error(
            "Failed to set leak critical LED group, GROUP = {GROUP}, ASSERTED = {ASSERTED}, ERROR = {ERROR}",
            "GROUP", config.criticalLedGroup, "ASSERTED", assertedStr, "ERROR",
            e);
    }
}

} // namespace monitor
} // namespace leakdetector
} // namespace led
} // namespace phosphor
