#pragma once

#include "../utils.hpp"
#include "leak-detector-config.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/server.hpp>

#include <string>
#include <unordered_set>

namespace phosphor
{
namespace led
{
namespace leakdetector
{
namespace monitor
{

/** @class Monitor
 *  @brief Watches xyz.openbmc_project.State.LeakDetector objects and drives
 *         a configured LED group based on whether any detector is in the
 *         Critical state.
 *
 *  @details
 *  The LED group is asserted as soon as any detector reports Critical, and
 *  is only deasserted once every detector has left the Critical state. The
 *  set of currently-Critical detectors is tracked by D-Bus object path so
 *  that the LED is not deasserted while at least one detector remains
 *  Critical.
 */
class Monitor
{
  public:
    Monitor() = delete;
    ~Monitor() = default;
    Monitor(const Monitor&) = delete;
    Monitor& operator=(const Monitor&) = delete;
    Monitor(Monitor&&) = delete;
    Monitor& operator=(Monitor&&) = delete;

    /** @brief Construct a leak detector monitor.
     *
     *  Sets up signal matches for property changes and interface
     *  add/remove events, then performs an initial scan via ObjectMapper
     *  to seed state from any detectors that are already on the bus.
     *
     *  @param[in] bus    - The D-Bus bus object
     *  @param[in] config - Runtime configuration (LED group name).
     */
    Monitor(sdbusplus::bus_t& bus, config::Config config);

  private:
    /** @brief D-Bus connection. */
    sdbusplus::bus_t& bus;

    /** @brief Runtime configuration (LED group name). */
    config::Config config;

    /** @brief Object paths of detectors currently observed in the Critical
     *         state. The LED group is asserted iff this set is non-empty.
     */
    std::unordered_set<std::string> criticalDetectors;

    /** @brief Watch for DetectorState property changes. */
    sdbusplus::bus::match_t propertiesChangedMatch;

    /** @brief Watch for new LeakDetector objects appearing at runtime. */
    sdbusplus::bus::match_t interfacesAddedMatch;

    /** @brief Watch for LeakDetector objects being removed at runtime. */
    sdbusplus::bus::match_t interfacesRemovedMatch;

    /** @brief Walk the bus via ObjectMapper for any existing detectors and
     *         seed criticalDetectors with the ones currently in Critical.
     */
    void processExistingDetectors();

    /** @brief Update tracked state for a single detector path.
     *
     *  Inserts or removes the path from criticalDetectors based on
     *  whether @p state represents Critical, and toggles the LED group
     *  on the appropriate empty <-> non-empty transitions.
     *
     *  @param[in] path  - Detector D-Bus object path
     *  @param[in] state - Current DetectorState string for that detector.
     */
    void updateDetectorState(const std::string& path, const std::string& state);

    /** @brief Forget a detector path entirely (e.g. when its object is
     *         removed). Drops the LED group if this empties the set.
     */
    void forgetDetector(const std::string& path);

    /** @brief PropertiesChanged signal handler. */
    void onPropertiesChanged(sdbusplus::message_t& msg);

    /** @brief InterfacesAdded signal handler. */
    void onInterfacesAdded(sdbusplus::message_t& msg);

    /** @brief InterfacesRemoved signal handler. */
    void onInterfacesRemoved(sdbusplus::message_t& msg);

    /** @brief Set the Asserted property of the configured LED group. */
    void setLeakCriticalLed(bool asserted);
};

} // namespace monitor
} // namespace leakdetector
} // namespace led
} // namespace phosphor
