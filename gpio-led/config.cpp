#include "config.hpp"

#include <cstddef>
#include <format>
#include <set>
#include <string>
#include <string_view>

namespace phosphor::led::gpio
{

void from_json(const Json& j, GpioLedEntry& entry)
{
    j.at("name").get_to(entry.name);
    j.at("output_gpio").get_to(entry.outputGpio);
    j.at("output_active_low").get_to(entry.outputActiveLow);
    j.at("input_gpio").get_to(entry.inputGpio);
    j.at("input_active_low").get_to(entry.inputActiveLow);
}

namespace
{

// Characters permitted in a D-Bus object path element.
constexpr std::string_view dbusPathChars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";

[[noreturn]] void fail(std::size_t i, std::string_view what)
{
    throw ConfigError(std::format("leds[{}]: {}", i, what));
}

} // namespace

GpioLedConfig parseAndValidate(const std::string& rawJson)
{
    GpioLedConfig cfg;
    try
    {
        Json::parse(rawJson).at("leds").get_to(cfg.leds);
    }
    catch (const Json::exception& e)
    {
        throw ConfigError(std::format("invalid config: {}", e.what()));
    }

    if (cfg.leds.empty())
    {
        throw ConfigError("'leds' must be a non-empty array");
    }

    std::set<std::string> ledNames;
    std::set<std::string> gpioNames;
    for (std::size_t i = 0; i < cfg.leds.size(); ++i)
    {
        const auto& entry = cfg.leds[i];

        if (entry.name.empty() || entry.outputGpio.empty() ||
            entry.inputGpio.empty())
        {
            fail(i, "name, output_gpio and input_gpio must not be empty");
        }
        if (entry.name.find_first_not_of(dbusPathChars) != std::string::npos)
        {
            fail(i, std::format(
                        "name '{}' is not a valid D-Bus object path element",
                        entry.name));
        }
        if (entry.outputGpio == entry.inputGpio)
        {
            fail(i, std::format(
                        "output_gpio and input_gpio must differ (both '{}')",
                        entry.outputGpio));
        }
        if (!ledNames.insert(entry.name).second)
        {
            fail(i, std::format("duplicate LED name '{}'", entry.name));
        }
        for (const std::string& line : {entry.outputGpio, entry.inputGpio})
        {
            if (!gpioNames.insert(line).second)
            {
                fail(i, std::format("duplicate GPIO line name '{}'", line));
            }
        }
    }
    return cfg;
}

} // namespace phosphor::led::gpio
