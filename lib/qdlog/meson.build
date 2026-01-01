# libqdlog - Quadrate Standard Library - Logging Module

qdlog_sources = files(
    'src/log.c',
    'src/qd_log.c',
)

qdlog_inc = include_directories('include')

# Static library only
qdlog_static = static_library('qdlog',
    qdlog_sources,
    include_directories: [qdlog_inc, qdrt_inc],
    c_args: ['-ffunction-sections', '-fdata-sections'],
    install: false
)

# Dependency declaration for other parts of the build
qdlog_dep = declare_dependency(
    link_with: qdlog_static,
    include_directories: qdlog_inc
)

install_headers(
    'include/qdlog/log.h',
    subdir: 'qdlog',
)

# Install Quadrate module
install_data(
    'qd/log/module.qd',
    install_dir: get_option('datadir') / 'quadrate' / 'log',
)
