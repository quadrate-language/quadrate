unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_log = executable('test_log',
    'test_log.c',
    dependencies: [qdlog_dep, qdrt_static_dep, unit_check_dep],
    c_args: ['-Wno-sign-conversion'],
    build_by_default: true
)

test('test_log', test_log, timeout: 30)
