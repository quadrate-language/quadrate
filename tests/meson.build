# Quadrate language tests
# Run .qd test files and compare outputs with expected results

# Use the quadc build target directly (needed for test() depends: on stricter Meson)
quadc = quadc_exe

test('qd_tests',
	find_program('run_all.sh'),
	args: ['--suite', 'qd'],
	workdir: meson.project_source_root(),
	env: {
		'QUADC': quadc.full_path(),
		'QUADRATE_LIBDIR': meson.project_build_root() / 'lib',
	},
	depends: quadc,
	timeout: 300
)

# Linter tests are run directly by run_all.sh (not via meson test)
# to avoid circular dependency

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

# Embed API tests (standalone executables linking against libqd)
if get_option('build_examples')
	subdir('embed')
	test('embed_tests',
		find_program('run_all.sh'),
		args: ['--suite', 'embed'],
		suite: 'embed',
		workdir: meson.project_source_root(),
		env: {
			'LD_LIBRARY_PATH': meson.project_build_root() / 'lib/rt' + ':' + meson.project_build_root() / 'lib/qd',
		},
		timeout: 60
	)
endif
