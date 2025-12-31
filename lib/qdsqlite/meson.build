# qdsqlite library - SQLite database driver

sqlite3_dep = dependency('sqlite3', required: true)

qdsqlite_inc = include_directories('include')

qdsqlite_sources = files(
    'src/sqlite.c',
)

# Static library only
qdsqlite_static = static_library('qdsqlite',
    qdsqlite_sources,
    include_directories: qdsqlite_inc,
    dependencies: [qd_static_dep, sqlite3_dep],
    c_args: ['-ffunction-sections', '-fdata-sections'],
    install: false
)

# Dependency declaration for other parts of the build
qdsqlite_dep = declare_dependency(
    link_with: qdsqlite_static,
    include_directories: qdsqlite_inc,
    dependencies: [sqlite3_dep]
)

# Tests
if get_option('build_tests')
    subdir('tests')
endif
