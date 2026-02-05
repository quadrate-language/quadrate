quadlint_sources = files(
	'src/main.cc',
)

# jansson_dep is declared in root meson.build
quadlint = executable('quadlint',
	quadlint_sources,
	include_directories: [version_inc],
	dependencies: [qc_dep, qdcli_dep, jansson_dep],
	install: true
)

# Override find_program so tests use this build
meson.override_find_program('quadlint', quadlint)
