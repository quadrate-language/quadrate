# Embed API tests
# These test the Quadrate embedding C API (libqd)

multi_module_test_exe = executable('multi-module-test',
	'multi-module-test.cc',
	dependencies: qd_dep,
	build_rpath: meson.project_build_root() / 'lib/rt' + ':' + meson.project_build_root() / 'lib/qd',
	install_rpath: '$ORIGIN/../lib',
	build_by_default: false
)

native_functions_test_exe = executable('native-functions-test',
	'native-functions-test.cc',
	dependencies: qd_dep,
	build_rpath: meson.project_build_root() / 'lib/rt' + ':' + meson.project_build_root() / 'lib/qd',
	install_rpath: '$ORIGIN/../lib',
	build_by_default: false
)

incremental_test_exe = executable('incremental-test',
	'incremental-test.cc',
	dependencies: qd_dep,
	build_rpath: meson.project_build_root() / 'lib/rt' + ':' + meson.project_build_root() / 'lib/qd',
	install_rpath: '$ORIGIN/../lib',
	build_by_default: false
)

typed_native_test_exe = executable('typed-native-test',
	'typed-native-test.cc',
	dependencies: qd_dep,
	build_rpath: meson.project_build_root() / 'lib/rt' + ':' + meson.project_build_root() / 'lib/qd',
	install_rpath: '$ORIGIN/../lib',
	build_by_default: false
)

embed_comprehensive_test_exe = executable('embed-comprehensive-test',
	'embed-comprehensive-test.cc',
	dependencies: qd_dep,
	build_rpath: meson.project_build_root() / 'lib/rt' + ':' + meson.project_build_root() / 'lib/qd',
	install_rpath: '$ORIGIN/../lib',
	build_by_default: false,
	link_args: ['-lm']
)

# Copy to dist/examples/ and fix RPATH for distribution
patchelf = find_program('patchelf', required: false)
chrpath = find_program('chrpath', required: false)

foreach example_info : [
	['multi-module-test', multi_module_test_exe],
	['native-functions-test', native_functions_test_exe],
	['incremental-test', incremental_test_exe],
	['typed-native-test', typed_native_test_exe],
	['embed-comprehensive-test', embed_comprehensive_test_exe]
]
	example_name = example_info[0]
	example_exe = example_info[1]

	if patchelf.found()
		custom_target(example_name + '_copy',
			output: example_name + '_copy.stamp',
			command: ['sh', '-c', 'mkdir -p ' + meson.project_source_root() / 'dist/examples' + ' && cp -f $0 ' + meson.project_source_root() / 'dist/examples' / example_name + ' && ' + patchelf.full_path() + ' --force-rpath --set-rpath \'$ORIGIN/../lib\' ' + meson.project_source_root() / 'dist/examples' / example_name + ' && touch $1', example_exe.full_path(), '@OUTPUT@'],
			depends: example_exe,
			build_by_default: false,
			build_always_stale: true
		)
	elif chrpath.found()
		custom_target(example_name + '_copy',
			output: example_name + '_copy.stamp',
			command: ['sh', '-c', 'mkdir -p ' + meson.project_source_root() / 'dist/examples' + ' && cp -f $0 ' + meson.project_source_root() / 'dist/examples' / example_name + ' && ' + chrpath.full_path() + ' -r \'$$ORIGIN/../lib\' ' + meson.project_source_root() / 'dist/examples' / example_name + ' && touch $1', example_exe.full_path(), '@OUTPUT@'],
			depends: example_exe,
			build_by_default: false,
			build_always_stale: true
		)
	else
		custom_target(example_name + '_copy',
			output: example_name + '_copy.stamp',
			command: ['sh', '-c',
				'mkdir -p ' + meson.project_source_root() / 'dist/examples' + ' && ' +
				'cp -f $0 ' + meson.project_source_root() / 'dist/examples' / example_name + '.bin' + ' && ' +
				'cat > ' + meson.project_source_root() / 'dist/examples' / example_name + ' << \'EOF\'\n' +
				'#!/bin/sh\n' +
				'DIR="$(cd "$(dirname "$0")" && pwd)"\n' +
				'export LD_LIBRARY_PATH="$DIR/../lib:$LD_LIBRARY_PATH"\n' +
				'exec "$DIR/' + example_name + '.bin" "$@"\n' +
				'EOF\n' +
				'chmod +x ' + meson.project_source_root() / 'dist/examples' / example_name + ' && ' +
				'touch $1',
				example_exe.full_path(), '@OUTPUT@'],
			depends: example_exe,
			build_by_default: false,
			build_always_stale: true
		)
	endif
endforeach
