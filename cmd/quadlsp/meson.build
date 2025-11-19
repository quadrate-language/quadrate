quadlsp_sources = files(
	'src/main.cc',
)

# Find jansson library for JSON handling
jansson_dep = dependency('jansson', required: true)

quadlsp_exe = executable('quadlsp',
	quadlsp_sources,
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
endif
