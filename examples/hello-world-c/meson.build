custom_target('hello-world-c',
	input: 'hello-world.c',
	output: 'hello-world-c',
	command: ['sh', '-c', 'gcc -I' + meson.project_source_root() / 'dist/include' + ' -L' + meson.project_source_root() / 'dist/lib -Wl,-rpath,dist/lib' + ' ' + meson.current_source_dir() / 'hello-world.c' + ' -lqdrt -lm -o ' + meson.project_source_root() / 'dist/examples/hello-world-c'],
	depends: qdrt_shared,
	build_by_default: false
)
