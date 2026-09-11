unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_signal = executable('test_signal',
    'test_signal.c',
    dependencies: [qdsignal_dep, qdrt_static_dep, unit_check_dep],
    c_args: ['-Wno-sign-conversion'],
    build_by_default: true
)

test('test_signal', test_signal, timeout: 30)
