custom_target('stars',
	input: 'stars.qd',
	output: 'stars',
	command: ['sh', '-c', 'cd ' + meson.project_source_root() + ' && ' + quadc.full_path() + ' --stack-codegen ' + meson.current_source_dir() / 'stars.qd' + ' -o ' + meson.project_source_root() / 'dist/examples/@BASENAME@'],
	depends: quadc,
	build_by_default: false
)
