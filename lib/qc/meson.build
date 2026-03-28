qc_sources = files(
		'src/ast.cc',
		'src/ast_statements.cc',
		'src/ast_expressions.cc',
		'src/ast_declarations.cc',
		'src/ast_types.cc',
		'src/ast_printer.cc',
		'src/colors.cc',
		'src/error_reporter.cc',
		'src/source_formatter.cc',
		'src/semantic_validator.cc',
		'src/semantic_validator_modules.cc',
		'src/semantic_validator_collect.cc',
		'src/semantic_validator_typecheck.cc',
		'src/semantic_validator_instructions.cc',
)

qc_inc = include_directories('include')

# u8t library (subproject only)
u8t_dep = subproject('u8t').get_variable('u8t_dep')

qc_lib = static_library('qc',
		qc_sources,
		include_directories: [qc_inc, qdrt_src_inc],
		dependencies: u8t_dep,
		install: false
)

# qc depends on qdrt_static_dep for exe_path_platform_get (used in semantic_validator_modules.cc)
qc_dep = declare_dependency(
		link_with: qc_lib,
		include_directories: qc_inc,
		dependencies: [u8t_dep, qdrt_static_dep]
)

# Tests
if get_option('build_tests')
		subdir('tests')
endif
