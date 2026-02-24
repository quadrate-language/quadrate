# unit-check library (subproject only)
unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_strings = executable('test_strings',
	'test_strings.c',
	dependencies: [qdstrings_dep, qdrt_static_dep, unit_check_dep],
	c_args: ['-Wno-sign-conversion'],
	build_by_default: true
)

test('test_strings', test_strings, timeout: 30)
