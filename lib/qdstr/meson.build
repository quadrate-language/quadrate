# libqdstr - Quadrate Standard Library - String Module
# Provides string utilities

qdstr_sources = files(
	'src/str.c',
)

qdstr_inc = include_directories('include')

# Static library only
qdstr_static = static_library('qdstr',
	qdstr_sources,
	include_directories: qdstr_inc,
	dependencies: qd_static_dep,
	install: false
)

# Dependency declaration for other parts of the build
qdstr_dep = declare_dependency(
	link_with: qdstr_static,
	include_directories: qdstr_inc
)
