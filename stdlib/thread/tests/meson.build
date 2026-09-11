unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_thread = executable('test_thread',
    'test_thread.c',
    dependencies: [qdthread_dep, qdrt_static_dep, unit_check_dep],
    c_args: ['-Wno-sign-conversion'],
    build_by_default: true
)

test('test_thread', test_thread, timeout: 30)
