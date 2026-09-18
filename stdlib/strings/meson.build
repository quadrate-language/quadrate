# libstrings - Quadrate Standard Library - String Module
# Provides string utilities

qdstrings_sources = files(
	'src/strings.c',
)

# Native registration table, generated from the .qd import block
strings_register_c = custom_target('strings-register',
	input: files('qd/strings/strings.qd'),
	output: 'strings_register.c',
	command: [gen_module_register, '--module', 'strings', '@INPUT@', '@OUTPUT@']
)


qdstrings_inc = include_directories('include')

# Static library only
qdstrings_static = static_library('strings',
	qdstrings_sources,
	strings_register_c,
	include_directories: [qdstrings_inc, qdunicode_inc],
	dependencies: qdrt_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdstrings_dep = declare_dependency(
	link_with: qdstrings_static,
	include_directories: qdstrings_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
