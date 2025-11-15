# quadrate - Interactive REPL for Quadrate

if llvmgen_dep.found()
	# Find readline library
	readline_dep = dependency('readline', required: false)

	if not readline_dep.found()
		readline_dep = meson.get_compiler('cpp').find_library('readline', required: false)
	endif

	if readline_dep.found()
		quadrate_repl_sources = files(
			'src/main.cc',
		)

		quadrate_repl_exe = executable('quadrate',
			quadrate_repl_sources,
			dependencies: [qc_dep, llvmgen_dep, readline_dep],
			install: false
		)
	else
		warning('readline library not found, skipping quadrate REPL')
	endif
else
	warning('LLVM not found, skipping quadrate REPL')
endif
