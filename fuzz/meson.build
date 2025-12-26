# Fuzzing targets for Quadrate
#
# Build fuzzing targets with:
#   meson setup build/fuzz --buildtype=debug -Dbuild_fuzz=true
#   meson compile -C build/fuzz
#
# Run fuzzer with:
#   ./build/fuzz/fuzz/fuzz_parser fuzz/corpus/ -max_len=10000

# Check if fuzzing is enabled and we're using clang
if not get_option('build_fuzz')
	subdir_done()
endif

cc = meson.get_compiler('cpp')
if cc.get_id() != 'clang'
	warning('Fuzzing requires clang compiler. Skipping fuzz targets.')
	subdir_done()
endif

# Fuzzing flags
fuzz_args = [
	'-fsanitize=fuzzer,address,undefined',
	'-fno-omit-frame-pointer',
	'-g',
]

fuzz_link_args = [
	'-fsanitize=fuzzer,address,undefined',
]

# Parser fuzz target
fuzz_parser = executable('fuzz_parser',
	'fuzz_parser.cc',
	dependencies: [qc_dep],
	cpp_args: fuzz_args,
	link_args: fuzz_link_args,
	build_by_default: true,
)
