# libstdosqd - Quadrate Standard Library - OS Module
# Provides operating system utilities

stdosqd_sources = files(
	'src/os.c',
	'src/os_fs.cc',  # C++17 filesystem wrapper for cross-platform directory ops
)

stdosqd_inc = include_directories('include')

# Get C and C++ compilers for dependencies
cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

# Need to link C++ standard library when using C++  code from C
cpp_dep = cc.find_library('stdc++', required: true)

# C++17 filesystem library (integrated into libstdc++ on most modern compilers)
# Some older compilers need explicit -lstdc++fs
fs_dep = cxx.find_library('stdc++fs', required: false)

# Shared library
stdosqd_shared = shared_library('stdosqd',
	stdosqd_sources,
	include_directories: stdosqd_inc,
	dependencies: [qd_dep, cpp_dep, fs_dep],
	install_rpath: '$ORIGIN',
	install: false
)

# Static library
stdosqd_static = static_library('stdosqd_static',
	stdosqd_sources,
	include_directories: stdosqd_inc,
	dependencies: [qd_static_dep, cpp_dep, fs_dep],
	install: false
)

# Dependency declarations for other parts of the build
stdosqd_dep = declare_dependency(
	link_with: stdosqd_shared,
	dependencies: [cpp_dep, fs_dep],
	include_directories: stdosqd_inc
)

stdosqd_static_dep = declare_dependency(
	link_with: stdosqd_static,
	dependencies: [cpp_dep, fs_dep],
	include_directories: stdosqd_inc
)
