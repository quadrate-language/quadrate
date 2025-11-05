# Tests for libstdqd

# Find unit-check library
cc = meson.get_compiler('c')
unit_check_lib = cc.find_library('unit-check', required: true)

test_stdqd = executable('test_stdqd',
	'test_stdqd.c',
	include_directories: stdqd_inc,
	dependencies: [qd_static_dep, unit_check_lib],
	link_with: stdqd_static,
	c_args: ['-DTEST_BUILD']
)

test('stdqd', test_stdqd)
