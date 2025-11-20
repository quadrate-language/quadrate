# unit-check library (subproject only)
unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_ast = executable('test_ast',
		'test_ast.cc',
		dependencies: [qc_dep, unit_check_dep],
		build_by_default: true
)

test_semantic_validator = executable('test_semantic_validator',
		'test_semantic_validator.cc',
		dependencies: [qc_dep, unit_check_dep],
		build_by_default: true
)

test('test_ast', test_ast)
test('test_semantic_validator', test_semantic_validator)
