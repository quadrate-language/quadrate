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

test_parser_edge_cases = executable('test_parser_edge_cases',
		'test_parser_edge_cases.cc',
		dependencies: [qc_dep, unit_check_dep],
		build_by_default: true
)

test_semantic_validator_extended = executable('test_semantic_validator_extended',
		'test_semantic_validator_extended.cc',
		dependencies: [qc_dep, unit_check_dep],
		build_by_default: true
)

test('test_ast', test_ast)
test('test_semantic_validator', test_semantic_validator)
test('test_parser_edge_cases', test_parser_edge_cases)
test('test_semantic_validator_extended', test_semantic_validator_extended)
