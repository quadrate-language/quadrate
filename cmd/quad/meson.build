# quad - Unified CLI for Quadrate toolchain

quad_sources = files(
	'src/main.cc',
)

# Include qdrt source directory for platform headers
qdrt_src_inc = include_directories('../../lib/rt')

quad_exe = executable('quad',
	quad_sources,
	include_directories: [qdrt_src_inc, version_inc],
	dependencies: [qc_dep, qdcli_dep, qdrt_dep],
	install: false
)
