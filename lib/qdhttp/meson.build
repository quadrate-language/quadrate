qdhttp_inc = include_directories('include')

qdhttp_sources = files(
    'src/http.c',
    'src/http_server.c',
)

thread_dep = dependency('threads')

qdhttp_static = static_library('qdhttp',
    qdhttp_sources,
    include_directories: [qdhttp_inc, qdrt_inc, qdnet_inc, qdtls_inc],
    dependencies: [thread_dep],
    c_args: ['-Wall', '-Wextra'],
    install: false,
)

qdhttp_dep = declare_dependency(
    include_directories: qdhttp_inc,
    link_with: qdhttp_static,
)

# Tests
if get_option('build_tests')
    subdir('tests')
endif
