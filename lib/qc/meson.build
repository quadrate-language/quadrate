qc_sources = files(
		'src/ast.cc',
		'src/ast_printer.cc',
		'src/colors.cc',
		'src/error_reporter.cc',
		'src/source_formatter.cc',
		'src/semantic_validator.cc',
)

qc_inc = include_directories('include')

# Find u8t library
cc = meson.get_compiler('c')
u8t_lib = cc.find_library('u8t', required: true)
u8t_inc = include_directories('/usr/include')
u8t_dep = declare_dependency(
		dependencies: u8t_lib,
		include_directories: u8t_inc
)

qc_lib = static_library('qc',
		qc_sources,
		include_directories: qc_inc,
		dependencies: u8t_dep,
		install: false
)

qc_dep = declare_dependency(
		link_with: qc_lib,
		include_directories: qc_inc,
		dependencies: u8t_dep
)

# Tests
if get_option('build_tests')
		subdir('tests')
endif
