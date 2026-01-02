quadpm_sources = files(
	'src/git_ref.cc',
	'src/main.cc',
	'src/semver.cc',
)

# JSON parsing for qd.json manifest
jansson_dep = dependency('jansson', version: '>=2.14', required: true, fallback: ['jansson', 'jansson_dep'])

quadpm_exe = executable(
	'quadpm',
	quadpm_sources,
	include_directories: [version_inc],
	dependencies: [jansson_dep],
	install: true,
	cpp_args: ['-std=c++20'],
)

# Override find_program so tests use this build
meson.override_find_program('quadpm', quadpm_exe)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
