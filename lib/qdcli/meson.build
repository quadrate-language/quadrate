# qdcli - Common utilities for Quadrate CLI tools

qdcli_sources = files(
	'src/file_utils.cc',
	'src/cli.cc',
)

qdcli_inc = include_directories('include')

qdcli_lib = static_library('qdcli',
	qdcli_sources,
	include_directories: [qdcli_inc, version_inc],
	install: false,
)

qdcli_dep = declare_dependency(
	link_with: qdcli_lib,
	include_directories: qdcli_inc,
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
