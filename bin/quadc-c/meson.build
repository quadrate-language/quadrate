# quadc-c - Legacy C backend compiler for Quadrate

quadc_c_sources = files(
	'src/main.cc',
)

quadc_c_exe = executable('quadc-c',
	quadc_c_sources,
	dependencies: [cgen_dep, qc_dep],
	install: false
)

# Make available to parent scopes
meson.override_find_program('quadc-c', quadc_c_exe)
