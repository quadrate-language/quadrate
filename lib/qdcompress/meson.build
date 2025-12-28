# qdcompress library - Compression support (gzip/zlib wrapper)

zlib_dep = dependency('zlib', required: true)

qdcompress_inc = include_directories('include')

qdcompress_sources = files(
    'src/compress.c',
)

# Static library only
qdcompress_static = static_library('qdcompress',
    qdcompress_sources,
    include_directories: qdcompress_inc,
    dependencies: [qd_static_dep, zlib_dep],
    c_args: ['-ffunction-sections', '-fdata-sections'],
    install: false
)

# Dependency declaration for other parts of the build
qdcompress_dep = declare_dependency(
    link_with: qdcompress_static,
    include_directories: qdcompress_inc,
    dependencies: [zlib_dep]
)

# Tests
if get_option('build_tests')
    subdir('tests')
endif
