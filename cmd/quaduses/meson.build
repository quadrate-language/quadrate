quaduses_sources = files(
	'src/main.cc',
)

# u8t is already provided by qc_dep (no need to get it again)

quaduses = executable('quaduses',
	quaduses_sources,
	dependencies: qc_dep,
	install: false
)
