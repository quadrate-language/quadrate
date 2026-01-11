project('quadrate', ['c', 'cpp'],
		version: '2.0.0-alpha',
		default_options: [
				'c_std=c11',
				'cpp_std=c++20',
				'warning_level=3',
				'werror=true',
				'b_pie=true',
				'wrap_mode=default',
		])

# Common compiler arguments (warning_level=3 already adds -Wall -Wextra -Wpedantic)
common_args = [
		'-Wconversion',
		'-Wsign-conversion',
		'-Wshadow',
		'-Wcast-align',
		'-Wunused',
		'-Wnull-dereference',
		'-Wdouble-promotion',
		'-Wformat=2',
]

# C++ specific warnings
cpp_args = [
		'-Wnon-virtual-dtor',
		'-Wold-style-cast',
		'-Woverloaded-virtual',
]

# Add compiler arguments
add_project_arguments(common_args, language: ['c', 'cpp'])
add_project_arguments(cpp_args, language: 'cpp')

# Common dependencies (used by multiple cmd tools)
jansson_dep = dependency('jansson', version: '>=2.14', required: true, fallback: ['jansson', 'jansson_dep'])

# Subdirectories
subdir('lib')
subdir('cmd')
subdir('tests')

# Examples (optional)
if get_option('build_examples')
		subdir('examples')
endif

# Fuzzing targets (optional, requires clang)
if get_option('build_fuzz')
		subdir('tests/fuzz')
endif

# Test setup for valgrind
if get_option('build_tests')
		valgrind = find_program('valgrind', required: false)
		if valgrind.found()
				add_test_setup('valgrind',
						exe_wrapper: [valgrind,
								'--leak-check=full',
								'--show-leak-kinds=all',
								'--track-origins=yes',
								'--error-exitcode=1'],
						timeout_multiplier: 10)
		endif
endif
