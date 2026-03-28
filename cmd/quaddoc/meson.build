quaddoc_sources = files(
	'src/main.cc',
)

quaddoc = executable('quaddoc',
	quaddoc_sources,
	include_directories: [version_inc],
	dependencies: [qdcli_dep],
	install: false
)
