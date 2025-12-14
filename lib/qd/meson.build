qd_sources = files(
		'src/qd.cc',
)

qd_inc = include_directories('include')

# Include qdrt source directory for platform headers
qdrt_src_inc = include_directories('../qdrt')

# Define MESON_BUILD_ROOT for finding runtime library (only in debug builds)
if get_option('buildtype') == 'debug' or get_option('buildtype') == 'debugoptimized'
	qd_cpp_args = ['-DMESON_BUILD_ROOT="' + meson.project_build_root() + '"']
else
	qd_cpp_args = ['-DMESON_BUILD_ROOT=""']
endif

# Shared library - depends on qdrt, qc, llvmgen
qd_shared = shared_library('qd',
		qd_sources,
		include_directories: [qd_inc, qdrt_src_inc],
		dependencies: [qdrt_dep, qc_dep, llvmgen_dep],
		cpp_args: qd_cpp_args,
		install_rpath: '$ORIGIN',
		install: false
)

# Static library - includes dependencies statically
qd_static = static_library('qd_static',
		qd_sources,
		include_directories: [qd_inc, qdrt_src_inc],
		dependencies: [qdrt_static_dep, qc_dep, llvmgen_dep],
		cpp_args: qd_cpp_args,
	c_args: ['-ffunction-sections', '-fdata-sections'],
		install: false
)

qd_dep = declare_dependency(
		link_with: qd_shared,
		dependencies: [qdrt_dep, qc_dep, llvmgen_dep],
		include_directories: qd_inc
)

qd_static_dep = declare_dependency(
		link_with: qd_static,
		dependencies: [qdrt_static_dep, qc_dep, llvmgen_dep],
		include_directories: qd_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
