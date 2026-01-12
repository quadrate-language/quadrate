# quadc command-line tests

# unit-check library (subproject only)
unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_options = executable('test_options',
	'test_options.cc',
	'../src/options.cc',
	include_directories: [version_inc],
	dependencies: [unit_check_dep],
)

test('test_options', test_options)
