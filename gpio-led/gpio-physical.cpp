#include "gpio-physical.hpp"

#include <memory>
#include <string>
#include <utility>

namespace phosphor::led::gpio
{

GpioPhysical::GpioPhysical(sdbusplus::bus_t& bus, const std::string& objPath,
                           std::unique_ptr<GpioAccess> gpio) :
    PhysicalObject(bus, objPath.c_str(), PhysicalObject::action::defer_emit),
    gpioAccess(std::move(gpio))
{
    setInitialState();
    emit_object_added();
}

void GpioPhysical::setInitialState()
{
    Action initial =
        gpioAccess->inputAsserted() ? Action::Blink : Action::Off;
    PhysicalObject::state(initial);
}

auto GpioPhysical::state(Action value) -> Action
{
    // Only Blink asserts the request line; the MCU owns the blink behavior.
    // An arbitrated LED has no steady-on mode, so On is treated as Off.
    gpioAccess->drive(value == Action::Blink);

    // Report the actual shared-line level: another BMC may still hold it.
    return updateStateFromInput();
}

auto GpioPhysical::updateStateFromInput() -> Action
{
    // An asserted shared line means the MCU is blinking the LED; Blink is the
    // only action that asserts it, so report Blink rather than On.
    Action actual =
        gpioAccess->inputAsserted() ? Action::Blink : Action::Off;
    return PhysicalObject::state(actual);
}

} // namespace phosphor::led::gpio
