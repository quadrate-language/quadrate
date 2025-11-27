custom_target('modules',
	input: 'main.qd',
	output: 'modules',
	command: ['sh', '-c', 'cd ' + meson.current_source_dir() + ' && QUADRATE_LIBDIR=' + meson.project_source_root() / 'dist/lib' + ' ' + quadc.full_path() + ' main.qd -o ' + meson.project_source_root() / 'dist/examples/modules'],
	depends: quadc,
	build_by_default: false
)
