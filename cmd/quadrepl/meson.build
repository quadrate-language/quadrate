# quadrepl - Interactive REPL for Quadrate

if qd_static_dep.found()
	# Find readline library
	readline_dep = dependency('readline', required: false)

	if not readline_dep.found()
		readline_dep = meson.get_compiler('cpp').find_library('readline', required: false)
	endif

	if readline_dep.found()
		quadrepl_sources = files(
			'src/main.cc',
		)

		quadrepl_exe = executable('quadrepl',
			quadrepl_sources,
			include_directories: [version_inc],
			dependencies: [qd_static_dep, qc_dep, readline_dep],
			install: false
		)
	else
		warning('readline library not found, skipping quadrepl')
	endif
else
	warning('qd library not available, skipping quadrepl')
endif
