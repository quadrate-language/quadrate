# Embed example
embed_exe = executable('embed',
	'main.cc',
	dependencies: qd_dep,
	build_rpath: meson.project_build_root() / 'lib/rt' + ':' + meson.project_build_root() / 'lib/qd',
	install_rpath: '$ORIGIN/../lib',
	build_by_default: false
)

# Copy to dist/examples/ and fix RPATH for distribution
patchelf = find_program('patchelf', required: false)
chrpath = find_program('chrpath', required: false)

if patchelf.found()
	custom_target('embed_copy',
		output: 'embed_copy.stamp',
		command: ['sh', '-c', 'mkdir -p ' + meson.project_source_root() / 'dist/examples' + ' && cp -f $0 ' + meson.project_source_root() / 'dist/examples/embed' + ' && ' + patchelf.full_path() + ' --force-rpath --set-rpath \'$ORIGIN/../lib\' ' + meson.project_source_root() / 'dist/examples/embed' + ' && touch $1', embed_exe.full_path(), '@OUTPUT@'],
		depends: embed_exe,
		build_by_default: false,
		build_always_stale: true
	)
elif chrpath.found()
	custom_target('embed_copy',
		output: 'embed_copy.stamp',
		command: ['sh', '-c', 'mkdir -p ' + meson.project_source_root() / 'dist/examples' + ' && cp -f $0 ' + meson.project_source_root() / 'dist/examples/embed' + ' && ' + chrpath.full_path() + ' -r \'$$ORIGIN/../lib\' ' + meson.project_source_root() / 'dist/examples/embed' + ' && touch $1', embed_exe.full_path(), '@OUTPUT@'],
		depends: embed_exe,
		build_by_default: false,
		build_always_stale: true
	)
else
	custom_target('embed_copy',
		output: 'embed_copy.stamp',
		command: ['sh', '-c',
			'mkdir -p ' + meson.project_source_root() / 'dist/examples' + ' && ' +
			'cp -f $0 ' + meson.project_source_root() / 'dist/examples/embed.bin' + ' && ' +
			'cat > ' + meson.project_source_root() / 'dist/examples/embed' + ' << \'EOF\'\n' +
			'#!/bin/sh\n' +
			'DIR="$(cd "$(dirname "$0")" && pwd)"\n' +
			'export LD_LIBRARY_PATH="$DIR/../lib:$LD_LIBRARY_PATH"\n' +
			'exec "$DIR/embed.bin" "$@"\n' +
			'EOF\n' +
			'chmod +x ' + meson.project_source_root() / 'dist/examples/embed' + ' && ' +
			'touch $1',
			embed_exe.full_path(), '@OUTPUT@'],
		depends: embed_exe,
		build_by_default: false,
		build_always_stale: true
	)
endif
