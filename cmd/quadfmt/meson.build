quadfmt_sources = files(
		'src/main.cc',
		'src/uses.cc',
)

# u8t is already provided by qc_dep (no need to get it again)

quadfmt = executable('quadfmt',
		quadfmt_sources,
		include_directories: [version_inc],
		dependencies: [qc_dep, qdcli_dep],
		install: false
)
