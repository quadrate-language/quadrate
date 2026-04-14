unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_tty = executable('test_tty',
    'test_tty.c',
    dependencies: [qdtty_dep, qdrt_static_dep, unit_check_dep],
    c_args: ['-Wno-sign-conversion'],
    build_by_default: true
)

test('test_tty', test_tty, timeout: 30)
