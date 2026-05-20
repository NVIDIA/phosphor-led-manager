# Leak Detector LED Controller

The Leak Detector LED controller watches every D-Bus object that exposes the
`xyz.openbmc_project.State.LeakDetector` interface and asserts a configured
LED group whenever any detector reports the `Critical` `DetectorState`. The
LED group is deasserted only once every detector has left the `Critical`
state, so the LED stays on as long as at least one detector is still
critical.

The daemon is built when the `monitor-leak-detector` Meson feature is enabled
and runs as the `leak-detector-led-controller` systemd service:

```
/usr/libexec/phosphor-led-manager/leak-detector-led-controller \
    --config /usr/share/phosphor-led-manager/leak-detector-led-config.json
```

The `--config` argument is optional. If the file is missing or unreadable the
daemon falls back to built-in defaults and logs a message.

## JSON configuration

The config file accepts a single key:

```json
{
    "critical_led_group": "leak_critical"
}
```

| Key                  | Type   | Default          | Meaning                                                                                                          |
| -------------------- | ------ | ---------------- | ---------------------------------------------------------------------------------------------------------------- |
| `critical_led_group` | string | `leak_critical`  | Name of the LED group under `/xyz/openbmc_project/led/groups/` to assert when any detector enters `Critical`.    |

The named LED group must already exist in your `phosphor-led-manager`
configuration (i.e. defined in the LED group config consumed by the manager).

If `critical_led_group` is omitted or has the wrong type, the daemon logs a
diagnostic and uses the default value above. Unknown keys are ignored.

## Platform integration

This repository does not ship a default `leak-detector-led-config.json`;
platform layers should drop their own copy at
`/usr/share/phosphor-led-manager/leak-detector-led-config.json` via a Yocto
`bbappend` if they need a non-default LED group name. When the platform is
happy with `leak_critical`, no file is needed at all — the daemon's built-in
default applies.
