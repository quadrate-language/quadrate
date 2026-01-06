custom_target('threading',
	input: 'threading.qd',
	output: 'threading',
	command: ['sh', '-c', 'cd ' + meson.project_source_root() + ' && QUADRATE_ROOT=dist/share/quadrate QUADRATE_LIBDIR=dist/lib ' + quadc.full_path() + ' ' + meson.current_source_dir() / 'threading.qd' + ' -o ' + meson.project_source_root() / 'dist/examples/@BASENAME@'],
	depends: quadc,
	build_by_default: false
)
