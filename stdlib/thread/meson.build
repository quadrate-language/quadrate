# thread library - Threading primitives using C11 threads

# C11 threads may need pthread on some platforms
threads_dep = dependency('threads', required: true)

qdthread_inc = include_directories('include')

qdthread_sources = files(
    'src/thread.c',
)

# Native registration table, generated from the .qd import block
thread_register_c = custom_target('thread-register',
	input: files('qd/thread/thread.qd'),
	output: 'thread_register.c',
	command: [gen_module_register, '--module', 'thread', '@INPUT@', '@OUTPUT@']
)


# Static library only
qdthread_static = static_library('thread',
    qdthread_sources,
	thread_register_c,
    include_directories: qdthread_inc,
    dependencies: [qdrt_static_dep, threads_dep],
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
