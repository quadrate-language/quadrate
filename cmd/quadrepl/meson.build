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

		if get_option('buildtype') == 'debug' or get_option('buildtype') == 'debugoptimized'
			quadrepl_cpp_args = ['-DQUADRATE_SOURCE_ROOT="' + meson.project_source_root() + '"']
		else
			quadrepl_cpp_args = ['-DQUADRATE_SOURCE_ROOT=""']
		endif

		quadrepl_exe = executable('quadrepl',
			quadrepl_sources,
			cpp_args: quadrepl_cpp_args,
			include_directories: [version_inc, qdplatform_inc],
			dependencies: [qd_static_dep, qc_dep, qdcli_dep, readline_dep],
			install: false
		)
	else
		warning('readline library not found, skipping quadrepl')
	endif
else
	warning('qd library not available, skipping quadrepl')
endif
