# Find unit-check library
cc = meson.get_compiler('c')
unit_check_lib = cc.find_library('unit-check', required: true)

test_str = executable('test_str',
		'test_str.cc',
		dependencies: [u8_dep, unit_check_lib],
		build_by_default: true
)

test('test_str', test_str)
