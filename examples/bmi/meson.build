bmi_example = custom_target('bmi',
	input: 'bmi.qd',
	output: 'bmi',
	command: [quadc, '@INPUT@', '-o', '@OUTPUT@'],
	depends: quadc,
	build_by_default: false
)
