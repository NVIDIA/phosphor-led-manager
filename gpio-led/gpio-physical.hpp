#pragma once

#include "gpio-access.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Led/Physical/server.hpp>

#include <memory>
#include <string>

namespace phosphor::led::gpio
{

using PhysicalObject = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Led::server::Physical>;

// xyz.openbmc_project.Led.Physical for one arbitrated LED. Writing Blink
// asserts the output request line; On and Off both deassert it. Reported State
// tracks the input line, so it stays correct when another BMC also drives the
// shared line. Every other property on the interface (DutyOn, Period, Color) is
// stored by the base class and has no hardware effect: the MCU owns blink rate
// and the LED color is fixed.
class GpioPhysical : public PhysicalObject
{
  public:
    using Action =
        sdbusplus::xyz::openbmc_project::Led::server::Physical::Action;

    GpioPhysical() = delete;
    ~GpioPhysical() override = default;
    GpioPhysical(const GpioPhysical&) = delete;
    GpioPhysical& operator=(const GpioPhysical&) = delete;
    GpioPhysical(GpioPhysical&&) = delete;
    GpioPhysical& operator=(GpioPhysical&&) = delete;

    GpioPhysical(sdbusplus::bus_t& bus, const std::string& objPath,
                 std::unique_ptr<GpioAccess> gpio);

    // Keep the base getter visible; declaring the setter would hide it.
    using PhysicalObject::state;
    Action state(Action value) override;

    // Re-read the input line and update reported State without driving output.
    // Called on input edge events.
    Action updateStateFromInput();

    GpioAccess& gpio()
    {
        return *gpioAccess;
    }

  private:
    std::unique_ptr<GpioAccess> gpioAccess;

    void setInitialState();
};

} // namespace phosphor::led::gpio
