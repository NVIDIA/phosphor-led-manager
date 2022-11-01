phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
sdbusplus = dependency('sdbusplus')
sdeventplus = dependency('sdeventplus')
systemd = dependency('systemd')
phosphor_logging = dependency('phosphor-logging')

deps = [
  phosphor_dbus_interfaces,
  sdbusplus,
  sdeventplus,
  systemd,
  phosphor_logging,
]

executable(
  'power-led-controller',
  'power-led.cpp',
  dependencies: deps,
  install: true,
)
