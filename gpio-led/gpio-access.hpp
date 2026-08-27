#pragma once

#include "config.hpp"

#include <gpiod.hpp>

#include <string>

namespace phosphor::led::gpio
{

// Abstraction over the two GPIO lines backing one arbitrated LED (output
// request + input readback). Abstract so LED logic is unit-testable.
class GpioAccess
{
  public:
    GpioAccess() = default;
    GpioAccess(const GpioAccess&) = delete;
    GpioAccess& operator=(const GpioAccess&) = delete;
    GpioAccess(GpioAccess&&) = delete;
    GpioAccess& operator=(GpioAccess&&) = delete;
    virtual ~GpioAccess() = default;

    // Drive the output request line (asserted = request LED on).
    virtual void drive(bool asserted) = 0;

    // Logical state of the input line (true = asserted).
    virtual bool inputAsserted() = 0;

    // FD readable on an input-line edge; -1 if unavailable.
    virtual int inputEventFd() = 0;

    // Consume one pending edge event.
    virtual void readInputEvent() = 0;
};

// libgpiod-backed GpioAccess: resolves lines by name and honors active-low.
class LibgpiodAccess : public GpioAccess
{
  public:
    explicit LibgpiodAccess(const GpioLedEntry& entry);

    void drive(bool asserted) override;
    bool inputAsserted() override;
    int inputEventFd() override;
    void readInputEvent() override;

  private:
    std::string name;
    ::gpiod::line outputLine;
    ::gpiod::line inputLine;
};

} // namespace phosphor::led::gpio
