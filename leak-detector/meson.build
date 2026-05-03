if get_option('monitor-leak-detector').allowed()
    executable(
        'leak-detector-led-controller',
        [
            'leak-detector-main.cpp',
            'leak-detector-monitor.cpp',
            'leak-detector-config.cpp',
            '../utils.cpp',
        ],
        include_directories: ['.', '../'],
        dependencies: deps,
        install: true,
        install_dir: get_option('libexecdir') / 'phosphor-led-manager',
    )
endif
