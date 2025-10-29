# Find unit-check library
cc = meson.get_compiler('c')
unit_check_lib = cc.find_library('unit-check', required: true)

test_ast = executable('test_ast',
		'test_ast.cc',
		dependencies: [qc_dep, unit_check_lib],
		build_by_default: true
)

test_semantic_validator = executable('test_semantic_validator',
		'test_semantic_validator.cc',
		dependencies: [qc_dep, unit_check_lib],
		build_by_default: true
)

test('test_ast', test_ast)
test('test_semantic_validator', test_semantic_validator)
