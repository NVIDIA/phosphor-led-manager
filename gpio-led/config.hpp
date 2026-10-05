#pragma once

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace phosphor::led::gpio
{

using Json = nlohmann::json;

struct ConfigError : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

// One arbitrated LED: an output request line the BMC drives and an input line
// reporting the actual (open-collector) shared line state.
struct GpioLedEntry
{
    std::string name;
    std::string outputGpio;
    bool outputActiveLow{};
    std::string inputGpio;
    bool inputActiveLow{};
};

struct GpioLedConfig
{
    std::vector<GpioLedEntry> leds;
};

void from_json(const Json& j, GpioLedEntry& entry);

// Parse and validate rawJson. Throws ConfigError on any invalid input.
GpioLedConfig parseAndValidate(const std::string& rawJson);

} // namespace phosphor::led::gpio
