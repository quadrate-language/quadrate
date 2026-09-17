# libmath - Quadrate Standard Library - Math Module
# Provides mathematical functions

qdmath_sources = files(
	'src/math.c',
)

# Native registration table, generated from the .qd import block
math_register_c = custom_target('math-register',
	input: files('qd/math/math.qd'),
	output: 'math_register.c',
	command: [gen_module_register, '--module', 'math', '@INPUT@', '@OUTPUT@']
)


qdmath_inc = include_directories('include')

# libm is this module's own dependency; declare it here rather than relying on
# lib/rt's probe reaching us transitively.
qdmath_cc = meson.get_compiler('c')
qdmath_m_dep = qdmath_cc.find_library('m', required: false)

# Static library only
qdmath_static = static_library('math',
	qdmath_sources,
	math_register_c,
	include_directories: qdmath_inc,
	dependencies: [qdrt_static_dep, qdmath_m_dep],
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdmath_dep = declare_dependency(
	link_with: qdmath_static,
	dependencies: qdmath_m_dep,
	include_directories: qdmath_inc
)

# Tests
if get_option('build_tests')
	subdir('tests')
endif
