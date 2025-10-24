u8_sources = files(
		'src/str.cc',
)

u8_inc = include_directories('include')

u8_lib = static_library('u8',
		u8_sources,
		include_directories: u8_inc,
		install: false
)

u8_dep = declare_dependency(
		link_with: u8_lib,
		include_directories: u8_inc
)

# Tests
if get_option('build_tests')
		subdir('tests')
endif
