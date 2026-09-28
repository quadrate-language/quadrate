# unit-check library (subproject only)
unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

# The generated registration table, driven through the interpreter
test_f128_register = executable('test_f128_register',
	'test_f128_register.c',
	dependencies: [qdf128_dep, interp_dep, unit_check_dep],
	build_by_default: true
)

test('test_f128_register', test_f128_register, timeout: 30)
