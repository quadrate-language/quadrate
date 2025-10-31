quadc_sources = files(
		'src/main.cc',
)

# Declare as a global variable so it can be used by tests
quadc_exe = executable('quadc',
		quadc_sources,
		dependencies: [cgen_dep, qc_dep],
		install: false
)

# Make available to parent scopes
meson.override_find_program('quadc', quadc_exe)
