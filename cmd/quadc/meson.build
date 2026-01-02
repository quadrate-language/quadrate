# quadc - LLVM backend compiler for Quadrate (default)

if llvmgen_dep.found()
	quadc_sources = files(
		'src/main.cc',
		'src/module_resolver.cc',
		'src/options.cc',
		'src/temp_dir_guard.cc',
	)

	# Include qdrt source directory for platform headers
	qdrt_src_inc = include_directories('../../lib/qdrt')

	# JSON parsing for qd.json manifest
	jansson_dep = dependency('jansson', version: '>=2.14', required: true, fallback: ['jansson', 'jansson_dep'])

	quadc_exe = executable('quadc',
		quadc_sources,
		include_directories: [qdrt_src_inc, version_inc],
		dependencies: [qc_dep, llvmgen_dep, qdrt_dep, jansson_dep],
		install: false
	)

	# Make available to parent scopes
	meson.override_find_program('quadc', quadc_exe)
else
	error('LLVM is required to build quadc (the default compiler)')
endif
