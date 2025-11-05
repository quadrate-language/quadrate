runtime_sources = files(
		'src/runtime.c',
		'src/stack.c',
)

runtime_inc = include_directories('include')

# Math library dependency
cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

# Static library only - runtime is an implementation detail of libquadrate
runtime_static = static_library('runtime_static',
		runtime_sources,
		include_directories: runtime_inc,
		install: false
)

runtime_dep = declare_dependency(
		link_with: runtime_static,
		dependencies: m_dep,
		include_directories: runtime_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
