# FFI example: Quadrate calling C code
# This builds libgreet.a from greet.c, then compiles main.qd linking against it

custom_target('ffi',
	input: ['greet.c', 'main.qd'],
	output: 'ffi',
	command: ['sh', '-c',
		# Compile greet.c to object file
		'cc -c ' + meson.current_source_dir() / 'greet.c' +
		' -I' + meson.project_source_root() / 'dist/include' +
		' -o ' + meson.current_build_dir() / 'greet.o' +
		# Create static library in dist/lib so quadc can find it
		' && ar rcs ' + meson.project_source_root() / 'dist/lib/libgreet.a ' + meson.current_build_dir() / 'greet.o' +
		# Compile Quadrate program (run from project root so ./dist/lib is found)
		' && cd ' + meson.project_source_root() +
		' && QUADRATE_ROOT=dist/share/quadrate' +
		' ' + quadc.full_path() + ' examples/ffi/main.qd -o dist/examples/ffi'
	],
	depends: quadc,
	build_by_default: false
)
