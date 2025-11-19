# Quadrate language tests
# Run .qd test files and compare outputs with expected results

# Find the quadc compiler (will use the override we set)
quadc = find_program('quadc')

test('qd_tests',
	find_program('run_tests.sh'),
	args: ['qd'],
	workdir: meson.project_source_root(),
	env: {
		'QUADC': quadc.full_path(),
		'QUADRATE_LIBDIR': meson.project_build_root() / 'lib',
	},
	depends: quadc,
	timeout: 120
)

# quadpm tests
quadpm_exe = find_program('quadpm', required: false, dirs: meson.project_build_root() / 'cmd/quadpm')
if quadpm_exe.found()
	test('quadpm_tests',
		find_program('quadpm/test_quadpm.sh'),
		suite: 'quadpm',
		workdir: meson.project_source_root(),
		env: {
			'QUADPM': quadpm_exe.full_path(),
		},
		timeout: 60
	)
endif

# Embed examples tests
if get_option('build_examples')
	test('embed_tests',
		find_program('run_embed_tests.sh'),
		suite: 'embed',
		workdir: meson.project_source_root(),
		env: {
			'LD_LIBRARY_PATH': meson.project_build_root() / 'lib/qdrt' + ':' + meson.project_build_root() / 'lib/qd',
		},
		timeout: 60
	)
endif
