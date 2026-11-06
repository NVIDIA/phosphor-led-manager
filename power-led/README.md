The Power LED Controller changes the state of LED groups
based on incoming boot progress codes (POST codes). It cycles through 4 phases:
Off, BMC Booted (before POST), Post Active (during POST), and Fully Powered On (after POST).
It consumes an additional json config file, with its path specified as an argument. 
Running it looks something like this:

/usr/bin/env power-led-controller --config /usr/share/phosphor-led-manager/power-led-config.json

The config file requires 5 things. First, it requires the POST codes that indicate the beginning
and ending of POST. They are provided in this format: 

    "POST_start": ["0x05", "0x00", "0x01", "0xC1"],
    "POST_end": ["0x19", "0x10", "0x10", "0x03"],

The codes don't have to be full POST codes. The example provided only compares the last 4 bytes,
not including the instance byte, which should not be provided. 

The other 3 things are the LED groups, which should already exist in your led.yaml config
(that is the config required by Phosphor LED Manager). They should be structured like this:

    "BMC_booted_group": "bmc_booted",
    "POST_active_group": "post_active",
    "fully_powered_on_group": "powered_on"

BMC_booted_group is the LED group that will be active before POST begins.
POST_active_group will be active during POST.
fully_powered_on_group will be active after POST.

These groups are mutually exclusive, so only 1/3 will be active at any given time.

If the host is already powered on and has exited POST before the BMC boots, then
the Power LED Controller will use the POST code log to get the power LED into the correct state.
If the host ever reboots, the Power LED Controller will reset and again cycle through these states.
Optionally, POST progress can be taken from the host's OperatingSystemState D-Bus property
instead of POST codes. Add this to the config:

    "POST_use_os_state": true

In this mode POST is considered started as soon as the host is on, and ended when
`xyz.openbmc_project.State.OperatingSystem.Status.OperatingSystemState` on
`/xyz/openbmc_project/state/host0` becomes `...OSStatus.Standby`. `POST_start` and
`POST_end` are then optional and ignored. When the host powers off the state is reset.
