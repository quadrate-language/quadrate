quaduses_sources = files(
	'src/main.cc',
)

# Find u8t library
cc = meson.get_compiler('c')
u8t_lib = cc.find_library('u8t', required: true)
u8t_inc = include_directories('/usr/include')
u8t_dep = declare_dependency(
	dependencies: u8t_lib,
	include_directories: u8t_inc
)

quaduses = executable('quaduses',
	quaduses_sources,
	dependencies: [u8t_dep, qc_dep],
	install: false
)
