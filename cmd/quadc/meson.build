# quadc - LLVM backend compiler for Quadrate (default)

if llvmgen_dep.found()
	quadc_sources = files(
		'src/main.cc',
		'src/ast_cache.cc',
		'src/module_resolver.cc',
		'src/options.cc',
		'src/temp_dir_guard.cc',
	)

	# Include qdrt source directory for platform headers
	qdrt_src_inc = include_directories('../../lib/rt')

	# jansson_dep is declared in root meson.build

	quadc_exe = executable('quadc',
		quadc_sources,
		include_directories: [qdrt_src_inc, version_inc],
		dependencies: [qc_dep, qdcli_dep, llvmgen_dep, qdrt_dep, jansson_dep],
		install: false
	)

	# Make available to parent scopes
	meson.override_find_program('quadc', quadc_exe)

	# Tests
	if get_option('build_tests')
		subdir('tests')
	endif
else
	error('LLVM is required to build quadc (the default compiler)')
endif
