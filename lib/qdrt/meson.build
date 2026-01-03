qdrt_sources = files(
		'src/runtime.c',
		'src/runtime_stack.c',
		'src/runtime_ops.c',
		'src/runtime_io.c',
		'src/stack.c',
		'src/memory.c',
		'src/qd_string.c',
		'src/qd_struct.c',
		'src/array.c',
		'src/platform/thread.c',
		'src/platform/mutex.c',
		'src/platform/posix/exe_path_linux.c',
		'src/platform/posix/process_posix.c',
		'src/platform/posix/dynlib_posix.c',
)

qdrt_inc = include_directories('include')

# Math library dependency
cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

# Dynamic library loading dependency (dl on POSIX systems)
dl_dep = cc.find_library('dl', required: true)

# Shared library - public runtime API
qdrt_shared = shared_library('qdrt',
		qdrt_sources,
		include_directories: qdrt_inc,
		dependencies: [m_dep, dl_dep],
		install_rpath: '$ORIGIN',
		install: false
)

# Static library (with -ffunction-sections -fdata-sections for --gc-sections linking)
qdrt_static = static_library('qdrt_static',
		qdrt_sources,
		include_directories: qdrt_inc,
		dependencies: [m_dep, dl_dep],
		c_args: ['-ffunction-sections', '-fdata-sections'],
		install: false
)

qdrt_dep = declare_dependency(
		link_with: qdrt_shared,
		include_directories: qdrt_inc
)

qdrt_static_dep = declare_dependency(
		link_with: qdrt_static,
		dependencies: [m_dep],
		include_directories: qdrt_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
