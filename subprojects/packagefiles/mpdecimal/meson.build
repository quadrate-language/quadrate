# libmpdec only, C only, built portably.
#
# Upstream's own build is autotools, and the WrapDB meson port of it adds -m64
# on every 64-bit host, which GCC rejects on aarch64 -- a Raspberry Pi could not
# build the runtime. This picks the ANSI C configuration everywhere instead of
# the x64 inline assembler: decimal arithmetic in the runtime is nowhere near
# hot enough for that to matter, and it is the configuration that builds the
# same on every host.
project('mpdecimal', 'c',
	version: '4.0.1',
	license: 'BSD-2-Clause',
	default_options: [
		'c_std=c99',
		'b_ndebug=true',
		'warning_level=0',
		'werror=false',
	]
)

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

header_config_64 = '''
/* ABI: 64-bit */
#define MPD_CONFIG_64 1

#ifdef MPD_CONFIG_32
  #error "cannot use MPD_CONFIG_32 with 64-bit header."
#endif

#ifdef CONFIG_32
  #error "cannot use CONFIG_32 with 64-bit header."
#endif'''

header_config_32 = '''
/* ABI: 32-bit */
#define MPD_CONFIG_32 1

#ifdef MPD_CONFIG_64
  #error "cannot use MPD_CONFIG_64 with 32-bit header."
#endif

#ifdef CONFIG_64
  #error "cannot use CONFIG_64 with 32-bit header."
#endif'''

conf = configuration_data()
mpd_c_args = ['-DANSI']
if cc.sizeof('size_t') == 8
	conf.set('MPD_HEADER_CONFIG', header_config_64)
	mpd_c_args += '-DCONFIG_64'
	if cc.sizeof('__uint128_t') > 0
		mpd_c_args += '-DHAVE_UINT128_T'
	endif
else
	conf.set('MPD_HEADER_CONFIG', header_config_32)
	mpd_c_args += '-DCONFIG_32'
endif

mpdecimal_h = configure_file(
	input: 'libmpdec/mpdecimal.h.in',
	output: 'mpdecimal.h',
	configuration: conf
)

libmpdec = static_library('mpdec',
	files(
		'libmpdec/basearith.c',
		'libmpdec/constants.c',
		'libmpdec/context.c',
		'libmpdec/convolute.c',
		'libmpdec/crt.c',
		'libmpdec/difradix2.c',
		'libmpdec/fnt.c',
		'libmpdec/fourstep.c',
		'libmpdec/io.c',
		'libmpdec/mpalloc.c',
		'libmpdec/mpdecimal.c',
		'libmpdec/mpsignal.c',
		'libmpdec/numbertheory.c',
		'libmpdec/sixstep.c',
		'libmpdec/transpose.c',
	),
	c_args: mpd_c_args + ['-ffunction-sections', '-fdata-sections'],
	dependencies: m_dep,
	pic: true,
	install: false,
	override_options: ['optimization=2']
)

mpdec_inc = include_directories('.', 'libmpdec')

mpdec_dep = declare_dependency(
	link_with: libmpdec,
	dependencies: m_dep,
	include_directories: mpdec_inc
)
