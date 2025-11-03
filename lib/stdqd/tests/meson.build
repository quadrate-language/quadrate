# Tests for libstdqd

test_stdqd = executable('test_stdqd',
	'test_stdqd.c',
	include_directories: [stdqd_inc, quadrate_inc],
	link_with: [stdqd_static, quadrate_static],
	c_args: ['-DTEST_BUILD']
)

test('stdqd', test_stdqd)
