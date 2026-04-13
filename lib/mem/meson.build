# libmem - Quadrate Standard Library - Memory Module

# Safe subset: no malloc/realloc/free/aligned_alloc/strdup. Linkable into
# both hosted (`libmem.a`) and freestanding (`libmem-freestanding.a`)
# builds without a configuration switch.
qdmem_safe_sources = files(
	'src/mem.c',
)

# Heap-using subset: alloc, realloc, alloc_aligned, to_string, from_string.
# Drags in libc (malloc/realloc/aligned_alloc/strdup); only linked into
# the hosted variant.
qdmem_heap_sources = files(
	'src/mem_heap.c',
)

qdmem_inc = include_directories('include')

# Hosted static library: safe + heap.
qdmem_static = static_library('mem',
	qdmem_safe_sources + qdmem_heap_sources,
	include_directories: qdmem_inc,
	dependencies: qdrt_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdmem_dep = declare_dependency(
	link_with: qdmem_static,
	include_directories: qdmem_inc
)

# Freestanding (no-libc) build of the safe subset only. Heap functions
# are physically excluded by source-file selection, not by `#ifdef`.
qdmem_freestanding = static_library('mem-freestanding',
	qdmem_safe_sources,
	include_directories: qdmem_inc,
	c_args: ['-ffreestanding', '-fno-builtin', '-ffunction-sections', '-fdata-sections'],
	install: false
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
