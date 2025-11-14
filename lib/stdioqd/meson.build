# libstdioqd - Quadrate Standard Library - File I/O Module

stdioqd_sources = files(
    'src/io.c',
)

stdioqd_inc = include_directories('include')

# Shared library
stdioqd_shared = shared_library('stdioqd',
    stdioqd_sources,
    include_directories: stdioqd_inc,
    dependencies: qd_dep,
    install_rpath: '$ORIGIN',
    install: false
)

# Static library
stdioqd_static = static_library('stdioqd_static',
    stdioqd_sources,
    include_directories: stdioqd_inc,
    dependencies: qd_static_dep,
    install: false
)

# Dependency declarations
stdioqd_dep = declare_dependency(
    link_with: stdioqd_shared,
    include_directories: stdioqd_inc
)

stdioqd_static_dep = declare_dependency(
    link_with: stdioqd_static,
    include_directories: stdioqd_inc
)
