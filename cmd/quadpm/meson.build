quadpm_sources = files(
	'src/git_ref.cc',
	'src/main.cc',
)

quadpm_exe = executable(
	'quadpm',
	quadpm_sources,
	dependencies: [],
	install: true,
	cpp_args: ['-std=c++20'],
)

# Override find_program so tests use this build
meson.override_find_program('quadpm', quadpm_exe)
