quadlint_sources = files(
	'src/main.cc',
)

quadlint = executable('quadlint',
	quadlint_sources,
	include_directories: [version_inc],
	dependencies: [qc_dep, qdcli_dep],
	install: true
)

# Override find_program so tests use this build
meson.override_find_program('quadlint', quadlint)
