# Find unit-check library
cc = meson.get_compiler('c')
unit_check_lib = cc.find_library('unit-check', required: true)

test_runtime = executable('test_runtime',
	'test_runtime.c',
	dependencies: [qdrt_static_dep, unit_check_lib],
	build_by_default: true
)

test('test_runtime', test_runtime)
