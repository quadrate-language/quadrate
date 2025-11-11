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
