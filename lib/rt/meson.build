# Common sources
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
)

# Platform-specific implementations
# TODO: Add Windows implementations when porting to Windows
qdrt_sources += files(
    'src/platform/posix/process_posix.c',
    'src/platform/posix/dynlib_posix.c',
)

# Platform-specific exe_path implementation
if host_machine.system() == 'haiku'
    qdrt_sources += files('src/platform/haiku/exe_path_haiku.c')
elif host_machine.system() == 'freebsd' or host_machine.system() == 'netbsd' or host_machine.system() == 'openbsd'
    qdrt_sources += files('src/platform/bsd/exe_path_bsd.c')
else
    # Linux and other systems with /proc/self/exe
    qdrt_sources += files('src/platform/posix/exe_path_linux.c')
endif

qdrt_inc = include_directories('include')

# Version information from project
version_str = meson.project_version()
version_parts = version_str.split('-')[0].split('.')
version_major = version_parts[0].to_int()
version_minor = version_parts[1].to_int()
version_patch = version_parts[2].to_int()
version_api = version_major * 10000 + version_minor * 100 + version_patch

version_args = [
		'-DQD_VERSION="@0@"'.format(version_str),
		'-DQD_VERSION_MAJOR=@0@'.format(version_major),
		'-DQD_VERSION_MINOR=@0@'.format(version_minor),
		'-DQD_VERSION_PATCH=@0@'.format(version_patch),
		'-DQD_VERSION_API=@0@'.format(version_api),
]

# Math library dependency
cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

# Dynamic library loading dependency
# - On Haiku: dlopen/dlsym are in libroot (linked by default)
# - On Linux/BSD/etc: they're in libdl
if host_machine.system() == 'haiku'
    dl_dep = dependency('', required: false)  # Not needed, libroot is implicit
else
    dl_dep = cc.find_library('dl', required: true)
endif

# Shared library - public runtime API
qdrt_shared = shared_library('qdrt',
		qdrt_sources,
		include_directories: qdrt_inc,
		dependencies: [m_dep, dl_dep],
		c_args: version_args,
		install_rpath: '$ORIGIN',
		install: false
)

# Static library (with -ffunction-sections -fdata-sections for --gc-sections linking)
qdrt_static = static_library('rt_static',
		qdrt_sources,
		include_directories: qdrt_inc,
		dependencies: [m_dep, dl_dep],
		c_args: version_args + ['-ffunction-sections', '-fdata-sections'],
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

# Freestanding (no-libc, no-OS) runtime variant. Built with -ffreestanding
# and without optimizations that pull in libc helpers. Used by
# `quadc --freestanding` builds for kernels, bootloaders, etc.
qdrt_freestanding = static_library('qdrt-freestanding',
		files('src/freestanding.c'),
		include_directories: qdrt_inc,
		c_args: ['-ffreestanding', '-fno-builtin', '-ffunction-sections', '-fdata-sections',
				'-Wno-unused-parameter'],
		install: false
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
