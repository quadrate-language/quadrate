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

subdir('fmt')
subdir('io')
subdir('math')
subdir('mem')
subdir('net')
subdir('os')
subdir('signal')
subdir('unicode')
subdir('strings')
subdir('strconv')
subdir('testing')
subdir('time')
subdir('thread')
subdir('tty')
subdir('tls')
subdir('http')
subdir('log')
