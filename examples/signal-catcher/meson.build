custom_target('signal-catcher',
	input: 'signal-catcher.qd',
	output: 'signal-catcher',
	command: ['sh', '-c', 'cd ' + meson.project_source_root() + ' && ' + quadc.full_path() + ' --stack-codegen ' + meson.current_source_dir() / 'signal-catcher.qd' + ' -o ' + meson.project_source_root() / 'dist/examples/@BASENAME@'],
	depends: quadc,
	build_by_default: false
)
