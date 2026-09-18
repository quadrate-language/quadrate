# libunicode - Quadrate Standard Library - Unicode Module
# Classification and case mapping for a single codepoint.
#
# The include directory is also used by stdlib/strings, which shares this module's
# codepoint.h so the string-level and codepoint-level answers cannot diverge. That is a
# header-only dependency (static inline), so strings does not link against this library.

qdunicode_sources = files(
	'src/unicode.c',
)

# Native registration table, generated from the .qd import block
unicode_register_c = custom_target('unicode-register',
	input: files('qd/unicode/unicode.qd'),
	output: 'unicode_register.c',
	command: [gen_module_register, '--module', 'unicode', '@INPUT@', '@OUTPUT@']
)

qdunicode_inc = include_directories('include')

# Static library only
qdunicode_static = static_library('unicode',
	qdunicode_sources,
	unicode_register_c,
	include_directories: qdunicode_inc,
	dependencies: qdrt_static_dep,
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

# Dependency declaration for other parts of the build
qdunicode_dep = declare_dependency(
	link_with: qdunicode_static,
	include_directories: qdunicode_inc
)
