# quadc-llvm - LLVM backend compiler for Quadrate

if llvmgen_dep.found()
	quadc_llvm = executable('quadc-llvm',
		'main.cc',
		dependencies: [qc_dep, cgen_dep, llvmgen_dep],
		install: false
	)
else
	message('Skipping quadc-llvm (LLVM not available)')
endif
