project('unit-check', 'c',
	version: '1.1.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
	]
)

unit_check_sources = files('src/uc.c')
unit_check_inc = include_directories('include')

unit_check_lib = static_library('unit-check',
	unit_check_sources,
	include_directories: unit_check_inc,
	install: false
)

unit_check_dep = declare_dependency(
	link_with: unit_check_lib,
	include_directories: unit_check_inc
)
