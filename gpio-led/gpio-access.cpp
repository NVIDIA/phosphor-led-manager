#include "gpio-access.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>

#include <bitset>
#include <stdexcept>
#include <string>

namespace phosphor::led::gpio
{

namespace
{

constexpr const char* consumer = "nvidia-gpio-led";

::gpiod::line findLineOrThrow(const std::string& lineName,
                              const std::string& ledName)
{
    auto line = ::gpiod::find_line(lineName);
    if (!line)
    {
        throw std::runtime_error(
            "GPIO line '" + lineName + "' for LED '" + ledName + "' not found");
    }
    return line;
}

std::bitset<32> requestFlags(bool activeLow)
{
    std::bitset<32> flags;
    if (activeLow)
    {
        flags |= ::gpiod::line_request::FLAG_ACTIVE_LOW;
    }
    return flags;
}

} // namespace

LibgpiodAccess::LibgpiodAccess(const GpioLedEntry& entry) : name(entry.name)
{
    outputLine = findLineOrThrow(entry.outputGpio, entry.name);
    // Request deasserted so a restart doesn't spuriously assert the shared
    // line.
    outputLine.request({consumer, ::gpiod::line_request::DIRECTION_OUTPUT,
                        requestFlags(entry.outputActiveLow)},
                       0);

    inputLine = findLineOrThrow(entry.inputGpio, entry.name);
    inputLine.request({consumer, ::gpiod::line_request::EVENT_BOTH_EDGES,
                       requestFlags(entry.inputActiveLow)});
}

void LibgpiodAccess::drive(bool asserted)
{
    outputLine.set_value(asserted ? 1 : 0);
}

bool LibgpiodAccess::inputAsserted()
{
    return inputLine.get_value() == 1;
}

int LibgpiodAccess::inputEventFd()
{
    return inputLine.event_get_fd();
}

void LibgpiodAccess::readInputEvent()
{
    // Drain the event; only the resulting line level matters, not the edge.
    inputLine.event_read();
}

} // namespace phosphor::led::gpio
