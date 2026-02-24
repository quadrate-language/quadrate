# thread library - Threading primitives using C11 threads

# C11 threads may need pthread on some platforms
threads_dep = dependency('threads', required: true)

qdthread_inc = include_directories('include')

qdthread_sources = files(
    'src/thread.c',
)

# Static library only
qdthread_static = static_library('thread',
    qdthread_sources,
    include_directories: qdthread_inc,
    dependencies: [qd_static_dep, threads_dep],
    c_args: ['-ffunction-sections', '-fdata-sections'],
    install: false
)

# Dependency declaration for other parts of the build
qdthread_dep = declare_dependency(
    link_with: qdthread_static,
    include_directories: qdthread_inc,
    dependencies: [threads_dep]
)

# Tests
if get_option('build_tests')
    subdir('tests')
endif
