# libqdos - Quadrate Standard Library - OS Module
# Provides operating system utilities

qdos_sources = files(
	'src/os.c',
	'src/os_fs.cc',  # C++17 filesystem wrapper for cross-platform directory ops
)

qdos_inc = include_directories('include')

# Get C and C++ compilers for dependencies
cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

# Need to link C++ standard library when using C++ code from C
cpp_dep = cc.find_library('stdc++', required: true)

# C++17 filesystem library (integrated into libstdc++ on most modern compilers)
# Some older compilers need explicit -lstdc++fs
fs_dep = cxx.find_library('stdc++fs', required: false)

# Static library only
qdos_static = static_library('qdos',
	qdos_sources,
	include_directories: qdos_inc,
	dependencies: [qd_static_dep, cpp_dep, fs_dep],
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdos_dep = declare_dependency(
	link_with: qdos_static,
	dependencies: [cpp_dep, fs_dep],
	include_directories: qdos_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
