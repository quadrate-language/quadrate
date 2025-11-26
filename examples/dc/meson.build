custom_target('dc',
	input: 'dc.qd',
	output: 'dc',
	command: ['sh', '-c', 'cd ' + meson.project_source_root() + ' && ' + quadc.full_path() + ' ' + meson.current_source_dir() / 'dc.qd' + ' -o ' + meson.project_source_root() / 'dist/examples/@BASENAME@'],
	depends: quadc,
	build_by_default: false
)
