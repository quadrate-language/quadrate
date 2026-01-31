custom_target('rocket',
	input: 'main.qd',
	output: 'rocket',
	command: ['sh', '-c', 'cd ' + meson.current_source_dir() + ' && QUADRATE_ROOT=' + meson.project_source_root() / 'dist/share/quadrate' + ' QUADRATE_LIBDIR=' + meson.project_source_root() / 'dist/lib' + ' ' + quadc.full_path() + ' main.qd -o ' + meson.project_source_root() / 'dist/examples/rocket'],
	depends: quadc,
	build_by_default: false
)
