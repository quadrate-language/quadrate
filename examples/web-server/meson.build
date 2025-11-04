custom_target('web-server',
	input: 'web-server.qd',
	output: 'web-server',
	command: ['sh', '-c', 'cd ' + meson.project_source_root() + ' && ' + quadc.full_path() + ' ' + meson.current_source_dir() / 'web-server.qd' + ' -o ' + meson.project_source_root() / 'dist/examples/@BASENAME@'],
	depends: quadc,
	build_by_default: false
)
