# nvidia-gpio-led

`nvidia-gpio-led` serves `xyz.openbmc_project.Led.Physical` for LEDs whose state
is arbitrated across multiple BMCs over a shared open-collector GPIO line. The
BMC drives a request line to ask for the LED, and reads a separate input line to
observe the level the MCU actually drives.

Phosphor LED Manager discovers these objects through the object mapper and
drives them like any other physical LED, so no PLM changes are needed.

## Behavior

- Writing `State` = `Blink` asserts the output request line. The MCU owns the
  blink rate and duty cycle.
- Writing `State` = `On` or `Off` deasserts it. An arbitrated LED has no
  steady-on mode, so `On` is treated as `Off`.
- The reported `State` reflects the input line, not the request: `Blink` when
  the line is asserted, `Off` when it is not. An asserted line means the MCU is
  blinking the LED, and `Blink` is the only action that asserts it. Because the
  line is open-collector it stays asserted while any BMC requests it, so a local
  `Off` write can leave the reported state `Blink` while another node holds it.
- The reported `State` is refreshed on every input edge, so a change driven by
  another BMC propagates without a local write. A write returns the level read
  immediately after driving the request; if the MCU responds later, the edge
  event corrects it.
- The initial reported state is read from the input line at startup, not assumed
  `Off`.
- `DutyOn`, `Period` and `Color` are stored to satisfy the interface contract
  but have no hardware effect.

## Building

Disabled by default. Enable with `-Dgpio-led=enabled`.

## Running

```bash
/usr/libexec/phosphor-led-manager/nvidia-gpio-led \
    --config /usr/share/phosphor-led-manager/gpio-led/gpio-led-config.json
```

The systemd unit `xyz.openbmc_project.LED.NvidiaGpioLed.service` is `Type=dbus`
with `BusName=xyz.openbmc_project.LED.NvidiaGpioLed`. It restarts on failure
every 5 seconds, giving up after the systemd default of 5 attempts.

## Config

The config is parsed and validated at startup. A missing or invalid config is
logged via `lg2` and the service exits non-zero.

```json
{
    "leds": [
        {
            "name": "uid_led",
            "output_gpio": "UID_STATUS_OUTPUT",
            "output_active_low": true,
            "input_gpio": "UID_STATUS_INPUT",
            "input_active_low": true
        }
    ]
}
```

| Field               | Required | Description                                                              |
| ------------------- | -------- | ------------------------------------------------------------------------ |
| `name`              | yes      | LED object leaf name; becomes `/xyz/openbmc_project/led/physical/<name>`. |
| `output_gpio`       | yes      | GPIO line name the BMC drives to request the LED on.                     |
| `output_active_low` | yes      | Whether the output request line is active low.                           |
| `input_gpio`        | yes      | GPIO line name the BMC reads for the actual shared-line state.           |
| `input_active_low`  | yes      | Whether the input readback line is active low.                           |

GPIO line names are resolved by libgpiod at startup and must match the DTS.
`output_gpio` and `input_gpio` must differ, and every LED name and GPIO line
name must be unique across the config.

The line names in `gpio-led-config.json` are placeholders; confirm the real ones
against the platform DTS before use.

## D-Bus interface

- Service: `xyz.openbmc_project.LED.NvidiaGpioLed`
- Objects: `/xyz/openbmc_project/led/physical/<name>`
- Interface: `xyz.openbmc_project.Led.Physical`

The LED groups that assert these objects are defined by the platform's
`led-group-config.json`, which lives in the BSP layer; this repo ships only
the example `gpio-led-config.json`.
