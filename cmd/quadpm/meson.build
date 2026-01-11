quadpm_sources = files(
	'src/git_ref.cc',
	'src/main.cc',
	'src/pm_commands.cc',
	'src/pm_manifest.cc',
	'src/pm_utils.cc',
	'src/semver.cc',
)

# jansson_dep is declared in root meson.build

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
