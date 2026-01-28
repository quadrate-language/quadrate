quadlsp_sources = files(
	'src/main.cc',
	'src/lsp_completion.cc',
	'src/lsp_navigation.cc',
)

# jansson_dep is declared in root meson.build

quadlsp_exe = executable('quadlsp',
	quadlsp_sources,
	include_directories: [version_inc],
	dependencies: [qc_dep, jansson_dep],
	install: true
)

# LSP tests
if get_option('build_tests')
	# Basic LSP tests
	test_lsp_script = find_program('tests/test_lsp.py', required: false)
	if test_lsp_script.found()
		test('test_lsp',
			test_lsp_script,
			depends: [quadlsp_exe],
			timeout: 30
		)
	endif

	# Extended LSP tests (edge cases, error handling)
	test_lsp_extended_script = find_program('tests/test_lsp_extended.py', required: false)
	if test_lsp_extended_script.found()
		test('test_lsp_extended',
			test_lsp_extended_script,
			depends: [quadlsp_exe],
			timeout: 60
		)
	endif

	# Stress tests (performance, load testing)
	test_lsp_stress_script = find_program('tests/test_lsp_stress.py', required: false)
	if test_lsp_stress_script.found()
		test('test_lsp_stress',
			test_lsp_stress_script,
			depends: [quadlsp_exe],
			timeout: 120
		)
	endif

	# Feature tests (hover, go-to-definition, find references, etc.)
	test_lsp_features_script = find_program('tests/test_lsp_features.py', required: false)
	if test_lsp_features_script.found()
		test('test_lsp_features',
			test_lsp_features_script,
			depends: [quadlsp_exe],
			timeout: 60
		)
	endif

	# Comprehensive tests (all LSP features)
	test_lsp_comprehensive_script = find_program('tests/test_lsp_comprehensive.py', required: false)
	if test_lsp_comprehensive_script.found()
		test('test_lsp_comprehensive',
			test_lsp_comprehensive_script,
			depends: [quadlsp_exe],
			timeout: 180
		)
	endif
endif
