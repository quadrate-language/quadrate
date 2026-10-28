# unit-check library (subproject only)
unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

# The generated registration table, driven through the interpreter
test_decimal_register = executable('test_decimal_register',
	'test_decimal_register.c',
	dependencies: [qddecimal_dep, interp_dep, unit_check_dep],
	build_by_default: true
)

test('test_decimal_register', test_decimal_register, timeout: 30)
