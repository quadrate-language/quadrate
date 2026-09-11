# unit-check library (subproject only)
unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_interp = executable('test_interp',
		'test_interp.cc',
		dependencies: [interp_dep, unit_check_dep],
		build_by_default: true
)

test('test_interp', test_interp)
