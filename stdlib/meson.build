# Standard library modules with a C backend. The pure-Quadrate ones carry no
# meson.build and are installed from their qd/ directory by the Makefile.
#
# Every module here builds a static library against qdrt_static_dep and adds
# only what it needs on top (threads, OpenSSL, stdc++fs). None of them needs the
# compiler, so none depends on qd_static_dep; keep it that way.
#
# Each module also compiles in a generated registration table so an embedder
# running the interpreter can make its words resolvable; see
# tools/gen_module_register.py.
gen_module_register = find_program(meson.project_source_root() / 'tools' / 'gen_module_register.py')

# Only the modules asked for, in this order. None of them depends on another,
# but http's tests use net and tls.
stdlib_modules = get_option('stdlib_modules')
if get_option('build_tests') and 'http' in stdlib_modules and not ('net' in stdlib_modules and 'tls' in stdlib_modules)
	error('stdlib_modules: http\'s tests need net and tls; add them, or set build_tests=false')
endif

foreach module : ['fmt', 'io', 'math', 'mem', 'net', 'os', 'signal', 'unicode', 'strings', 'strconv', 'testing', 'time', 'thread', 'tty', 'tls', 'http', 'log']
	if module in stdlib_modules
		subdir(module)
	endif
endforeach
