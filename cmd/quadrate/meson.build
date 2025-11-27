# quadrate - Interactive REPL for Quadrate

if qd_static_dep.found()
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
			dependencies: [qd_static_dep, qc_dep, readline_dep],
			install: false
		)
	else
		warning('readline library not found, skipping quadrate REPL')
	endif
else
	warning('qd library not available, skipping quadrate REPL')
endif
