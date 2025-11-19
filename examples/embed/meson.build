# Build all embed examples
embed_exe = executable('embed',
	'main.cc',
	dependencies: qd_dep,
	build_rpath: meson.project_build_root() / 'lib/qdrt' + ':' + meson.project_build_root() / 'lib/qd',
	install_rpath: '$ORIGIN/../lib',
	build_by_default: false
)

multi_module_test_exe = executable('multi_module_test',
	'multi_module_test.cc',
	dependencies: qd_dep,
	build_rpath: meson.project_build_root() / 'lib/qdrt' + ':' + meson.project_build_root() / 'lib/qd',
	install_rpath: '$ORIGIN/../lib',
	build_by_default: false
)

native_functions_test_exe = executable('native_functions_test',
	'native_functions_test.cc',
	dependencies: qd_dep,
	build_rpath: meson.project_build_root() / 'lib/qdrt' + ':' + meson.project_build_root() / 'lib/qd',
	install_rpath: '$ORIGIN/../lib',
	build_by_default: false
)

incremental_test_exe = executable('incremental_test',
	'incremental_test.cc',
	dependencies: qd_dep,
	build_rpath: meson.project_build_root() / 'lib/qdrt' + ':' + meson.project_build_root() / 'lib/qd',
	install_rpath: '$ORIGIN/../lib',
	build_by_default: false
)

# Copy to dist/examples/ and fix RPATH for distribution
patchelf = find_program('patchelf', required: false)
chrpath = find_program('chrpath', required: false)

# Helper function to create copy target for each example
foreach example_info : [
	['embed', embed_exe],
	['multi_module_test', multi_module_test_exe],
	['native_functions_test', native_functions_test_exe],
	['incremental_test', incremental_test_exe]
]
	example_name = example_info[0]
	example_exe = example_info[1]

	if patchelf.found()
		# Use patchelf to fix RPATH
		custom_target(example_name + '_copy',
			output: example_name + '_copy.stamp',
			command: ['sh', '-c', 'mkdir -p ' + meson.project_source_root() / 'dist/examples' + ' && cp -f $0 ' + meson.project_source_root() / 'dist/examples' / example_name + ' && ' + patchelf.full_path() + ' --set-rpath \'$$ORIGIN/../lib\' ' + meson.project_source_root() / 'dist/examples' / example_name + ' && touch $1', example_exe.full_path(), '@OUTPUT@'],
			depends: example_exe,
			build_by_default: false,
			build_always_stale: true
		)
	elif chrpath.found()
		# Use chrpath to fix RPATH
		custom_target(example_name + '_copy',
			output: example_name + '_copy.stamp',
			command: ['sh', '-c', 'mkdir -p ' + meson.project_source_root() / 'dist/examples' + ' && cp -f $0 ' + meson.project_source_root() / 'dist/examples' / example_name + ' && ' + chrpath.full_path() + ' -r \'$$ORIGIN/../lib\' ' + meson.project_source_root() / 'dist/examples' / example_name + ' && touch $1', example_exe.full_path(), '@OUTPUT@'],
			depends: example_exe,
			build_by_default: false,
			build_always_stale: true
		)
	else
		# Create wrapper script to set LD_LIBRARY_PATH
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
