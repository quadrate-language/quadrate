quadrate_sources = files(
		'src/qd.cc',
)

quadrate_inc = include_directories('include')

# Get math library dependency (needed by runtime)
cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

# Shared library - includes runtime statically
# Use link_whole to ensure all runtime symbols are exported
quadrate_shared = shared_library('quadrate',
		quadrate_sources,
		include_directories: [quadrate_inc, runtime_inc],
		link_whole: runtime_static,
		dependencies: m_dep,
		install_rpath: '$ORIGIN',
		install: false
)

# Static library - includes runtime statically
quadrate_static = static_library('quadrate_static',
		quadrate_sources,
		include_directories: [quadrate_inc, runtime_inc],
		link_whole: runtime_static,
		dependencies: m_dep,
		install: false
)

quadrate_dep = declare_dependency(
		link_with: quadrate_shared,
		include_directories: [quadrate_inc, runtime_inc]
)

quadrate_static_dep = declare_dependency(
		link_with: quadrate_static,
		include_directories: [quadrate_inc, runtime_inc]
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
