custom_target('sha256sum',
	input: 'sha256sum.qd',
	output: 'sha256sum',
	command: ['sh', '-c', 'cd ' + meson.project_source_root() + ' && ' + quadc.full_path() + ' ' + meson.current_source_dir() / 'sha256sum.qd' + ' -o ' + meson.project_source_root() / 'dist/examples/@BASENAME@'],
	depends: quadc,
	build_by_default: false
)
