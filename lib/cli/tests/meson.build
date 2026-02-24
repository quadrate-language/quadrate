# qdcli tests

# unit-check library (subproject only)
unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_file_utils = executable('test_file_utils',
	'test_file_utils.cc',
	include_directories: [qdcli_inc],
	dependencies: [qdcli_dep, unit_check_dep],
)

test('test_file_utils', test_file_utils)
