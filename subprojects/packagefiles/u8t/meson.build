project('u8t', 'c',
	version: '1.1.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
	]
)

u8t_sources = files('src/scanner.c')
u8t_inc = include_directories('include')

u8t_lib = static_library('u8t',
	u8t_sources,
	include_directories: u8t_inc,
	install: false
)

u8t_dep = declare_dependency(
	link_with: u8t_lib,
	include_directories: u8t_inc
)
