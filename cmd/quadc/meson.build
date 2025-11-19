# quadc - LLVM backend compiler for Quadrate (default)

if llvmgen_dep.found()
	quadc_sources = files(
		'src/main.cc',
	)

	quadc_exe = executable('quadc',
		quadc_sources,
		dependencies: [qc_dep, llvmgen_dep],
		install: false
	)

	# Make available to parent scopes
	meson.override_find_program('quadc', quadc_exe)
else
	error('LLVM is required to build quadc (the default compiler)')
endif
