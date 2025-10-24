custom_target('hello-world',
	input: 'hello-world.qd',
	output: 'hello-world',
	command: [quadc, '@INPUT@', '-o', '@OUTPUT@'],
	depends: quadc,
	build_by_default: false
)
