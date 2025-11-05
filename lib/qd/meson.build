qd_sources = files(
		'src/qd.cc',
)

qd_inc = include_directories('include')

# Shared library - depends on qdrt shared library
qd_shared = shared_library('qd',
		qd_sources,
		include_directories: qd_inc,
		dependencies: qdrt_dep,
		install_rpath: '$ORIGIN',
		install: false
)

# Static library - includes qdrt statically
qd_static = static_library('qd_static',
		qd_sources,
		include_directories: qd_inc,
		dependencies: qdrt_static_dep,
		install: false
)

qd_dep = declare_dependency(
		link_with: qd_shared,
		dependencies: qdrt_dep,
		include_directories: qd_inc
)

qd_static_dep = declare_dependency(
		link_with: qd_static,
		dependencies: qdrt_static_dep,
		include_directories: qd_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
