# cli - Common utilities for Quadrate CLI tools

qdcli_sources = files(
	'src/file_utils.cc',
	'src/cli.cc',
	'src/help.cc',
)

qdcli_inc = include_directories('include')

qdcli_lib = static_library('cli',
	qdcli_sources,
	include_directories: [qdcli_inc, version_inc],
	dependencies: [qc_dep, jansson_dep],
	install: false,
)

qdcli_dep = declare_dependency(
	link_with: qdcli_lib,
	include_directories: qdcli_inc,
	dependencies: [qc_dep, jansson_dep],
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
