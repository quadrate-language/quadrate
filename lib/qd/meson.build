qd_sources = files(
		'src/qd.cc',
)

qd_inc = include_directories('include')

# Dependencies for embedding API
dl_dep = meson.get_compiler('cpp').find_library('dl', required: true)

# Define MESON_BUILD_ROOT for finding runtime library
qd_cpp_args = ['-DMESON_BUILD_ROOT="' + meson.project_build_root() + '"']

# Shared library - depends on qdrt, qc, llvmgen, and dl
qd_shared = shared_library('qd',
		qd_sources,
		include_directories: qd_inc,
		dependencies: [qdrt_dep, qc_dep, llvmgen_dep, dl_dep],
		cpp_args: qd_cpp_args,
		install_rpath: '$ORIGIN',
		install: false
)

# Static library - includes dependencies statically
qd_static = static_library('qd_static',
		qd_sources,
		include_directories: qd_inc,
		dependencies: [qdrt_static_dep, qc_dep, llvmgen_dep, dl_dep],
		cpp_args: qd_cpp_args,
		install: false
)

qd_dep = declare_dependency(
		link_with: qd_shared,
		dependencies: [qdrt_dep, qc_dep, llvmgen_dep, dl_dep],
		include_directories: qd_inc
)

qd_static_dep = declare_dependency(
		link_with: qd_static,
		dependencies: [qdrt_static_dep, qc_dep, llvmgen_dep, dl_dep],
		include_directories: qd_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
