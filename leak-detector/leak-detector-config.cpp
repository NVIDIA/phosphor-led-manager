#include "leak-detector-config.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <fstream>

namespace phosphor
{
namespace led
{
namespace leakdetector
{
namespace config
{

namespace
{

constexpr auto KEY_CRITICAL_LED_GROUP = "critical_led_group";

} // namespace

Config loadConfig(const std::string& path)
{
    Config config;

    if (path.empty())
    {
        return config;
    }

    if (!std::filesystem::exists(path))
    {
        lg2::info(
            "Leak detector config file not found, using defaults. PATH = {PATH}",
            "PATH", path);
        return config;
    }

    nlohmann::json json;
    try
    {
        std::ifstream input{path};
        json = nlohmann::json::parse(input);
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Failed to parse leak detector config, falling back to defaults. PATH = {PATH}, ERROR = {ERROR}",
            "PATH", path, "ERROR", e.what());
        return config;
    }

    if (auto it = json.find(KEY_CRITICAL_LED_GROUP); it != json.end())
    {
        if (it->is_string())
        {
            config.criticalLedGroup = it->get<std::string>();
        }
        else
        {
            lg2::warning(
                "Config key '{KEY}' is not a string; using default '{GROUP}'",
                "KEY", KEY_CRITICAL_LED_GROUP, "GROUP",
                config.criticalLedGroup);
        }
    }
    else
    {
        lg2::info("Config key '{KEY}' not present; using default '{GROUP}'",
                  "KEY", KEY_CRITICAL_LED_GROUP, "GROUP",
                  config.criticalLedGroup);
    }

    lg2::info("Loaded leak detector config: CRITICAL_GROUP = {GROUP}", "GROUP",
              config.criticalLedGroup);
    return config;
}

} // namespace config
} // namespace leakdetector
} // namespace led
} // namespace phosphor
