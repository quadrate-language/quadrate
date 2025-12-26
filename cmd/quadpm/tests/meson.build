# quadpm tests

unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_semver = executable('test_semver',
	'test_semver.cc',
	'../src/semver.cc',
	dependencies: [unit_check_dep],
	cpp_args: ['-std=c++20'],
	build_by_default: true
)

test('test_semver', test_semver, timeout: 30)
