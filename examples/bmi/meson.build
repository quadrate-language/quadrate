custom_target('bmi',
	input: 'bmi.qd',
	output: 'bmi',
	command: ['sh', '-c', 'cd ' + meson.project_source_root() + ' && ' + quadc.full_path() + ' ' + meson.current_source_dir() / 'bmi.qd' + ' -o ' + meson.project_source_root() / 'dist/examples/@BASENAME@'],
	depends: quadc,
	build_by_default: false
)
