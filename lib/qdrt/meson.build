qdrt_sources = files(
		'src/runtime.c',
		'src/stack.c',
)

qdrt_inc = include_directories('include')

# Math library dependency
cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

# Shared library - public runtime API
qdrt_shared = shared_library('qdrt',
		qdrt_sources,
		include_directories: qdrt_inc,
		dependencies: m_dep,
		install_rpath: '$ORIGIN',
		install: false
)

# Static library
qdrt_static = static_library('qdrt_static',
		qdrt_sources,
		include_directories: qdrt_inc,
		dependencies: m_dep,
		install: false
)

qdrt_dep = declare_dependency(
		link_with: qdrt_shared,
		include_directories: qdrt_inc
)

qdrt_static_dep = declare_dependency(
		link_with: qdrt_static,
		dependencies: m_dep,
		include_directories: qdrt_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
