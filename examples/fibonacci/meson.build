custom_target('fibonacci',
	input: 'fibonacci.qd',
	output: 'fibonacci',
	command: ['sh', '-c', 'cd ' + meson.project_source_root() + ' && ' + quadc.full_path() + ' ' + meson.current_source_dir() / 'hello-world.qd' + ' -o ' + meson.project_source_root() / 'dist/examples/@BASENAME@'],
	depends: quadc,
	build_by_default: false
)
