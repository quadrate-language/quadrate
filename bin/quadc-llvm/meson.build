# quadc-llvm - LLVM backend compiler for Quadrate

if llvmgen_dep.found()
	quadc_llvm_sources = files(
		'src/main.cc',
	)

	quadc_llvm = executable('quadc-llvm',
		quadc_llvm_sources,
		dependencies: [qc_dep, llvmgen_dep],
		install: false
	)

	# Make available to parent scopes
	meson.override_find_program('quadc-llvm', quadc_llvm)
else
	message('Skipping quadc-llvm (LLVM not available)')
endif
