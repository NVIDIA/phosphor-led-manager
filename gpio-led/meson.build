gpio_led_inc = include_directories('.')

libgpiod_dep = dependency('libgpiodcxx')

gpio_led_deps = [
    phosphor_dbus_interfaces_dep,
    sdbusplus_dep,
    phosphor_logging_dep,
    nlohmann_json_dep,
    libgpiod_dep,
    CLI11_dep,
]

# Sources without main(), reused by the unit tests.
gpio_led_lib_sources = files(
    'config.cpp',
    'gpio-access.cpp',
    'gpio-physical.cpp',
)

executable(
    'nvidia-gpio-led',
    ['gpio-led-main.cpp', gpio_led_lib_sources],
    include_directories: gpio_led_inc,
    dependencies: gpio_led_deps,
    install: true,
    install_dir: get_option('libexecdir') / 'phosphor-led-manager',
)

# Install the example config into a dedicated subdirectory so it is not caught
# by the base recipe's top-level ${datadir}/${PN}/*.json prune.
install_data(
    'gpio-led-config.json',
    install_dir: get_option('datadir') / 'phosphor-led-manager' / 'gpio-led',
)
