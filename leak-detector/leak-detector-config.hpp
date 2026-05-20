#pragma once

#include <string>

namespace phosphor
{
namespace led
{
namespace leakdetector
{
namespace config
{

/** @brief Runtime configuration for the leak detector monitor.
 *
 *  A missing or empty config file produces a working monitor that asserts
 *  the "leak_critical" LED group whenever any detector enters the
 *  "Critical" DetectorState.
 */
struct Config
{
    /** @brief Name of the LED group (under /xyz/openbmc_project/led/groups/)
     *         to assert when any detector reports the "Critical"
     *         DetectorState.
     */
    std::string criticalLedGroup{"leak_critical"};
};

/** @brief Load configuration from a JSON file.
 *
 *  Behavior:
 *    - If @p path is empty, returns built-in defaults silently.
 *    - If @p path is set but the file is missing, logs an info message and
 *      returns defaults.
 *    - If @p path is set and the file exists but fails to parse, logs an
 *      error and returns defaults.
 *    - If @p path is set and the file is valid, returns the parsed config.
 *
 *  @param[in] path - Path to the JSON config file (may be empty).
 *  @return The resulting Config.
 */
Config loadConfig(const std::string& path);

} // namespace config
} // namespace leakdetector
} // namespace led
} // namespace phosphor
