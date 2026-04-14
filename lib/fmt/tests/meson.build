unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_fmt = executable('test_fmt',
    'test_fmt.c',
    dependencies: [qdfmt_dep, qdrt_static_dep, unit_check_dep],
    c_args: ['-Wno-sign-conversion'],
    build_by_default: true
)

test('test_fmt', test_fmt, timeout: 30)
