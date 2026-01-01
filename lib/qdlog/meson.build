qdlog_inc = include_directories('include')

qdlog_sources = files(
    'src/log.c',
    'src/qd_log.c',
)

libqdlog = static_library(
    'qdlog',
    qdlog_sources,
    include_directories: [qdlog_inc, qdrt_inc],
    install: true,
    install_dir: get_option('libdir'),
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
