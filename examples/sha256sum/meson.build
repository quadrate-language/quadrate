# sha256sum example - depends on external crypto module
# Check for crypto module in sibling directory (CI environment) or via QUADRATE_EXTERNAL_MODULES
project_parent = meson.project_source_root() / '..'
crypto_sibling = project_parent / 'crypto' / 'module.qd'
external_modules_env = run_command('sh', '-c', 'echo $QUADRATE_EXTERNAL_MODULES', check: false).stdout().strip()

include_flag = ''
can_build = false

if run_command('test', '-f', crypto_sibling, check: false).returncode() == 0
  include_flag = '-I ' + project_parent
  can_build = true
elif external_modules_env != ''
  external_crypto = external_modules_env / 'crypto' / 'module.qd'
  if run_command('test', '-f', external_crypto, check: false).returncode() == 0
    include_flag = '-I ' + external_modules_env
    can_build = true
  endif
endif

if can_build
  custom_target('sha256sum',
    input: 'sha256sum.qd',
    output: 'sha256sum',
    command: ['sh', '-c', 'cd ' + meson.project_source_root() + ' && QUADRATE_ROOT=dist/share/quadrate QUADRATE_LIBDIR=dist/lib ' + quadc.full_path() + ' ' + include_flag + ' ' + meson.current_source_dir() / 'sha256sum.qd' + ' -o ' + meson.project_source_root() / 'dist/examples/@BASENAME@'],
    depends: quadc,
    build_by_default: false
  )
else
  # Create a dummy target that explains why sha256sum isn't being built
  custom_target('sha256sum',
    input: 'sha256sum.qd',
    output: 'sha256sum',
    command: ['sh', '-c', 'echo "sha256sum example skipped - crypto module not found (install with: quadpm install crypto)" && mkdir -p ' + meson.project_source_root() / 'dist/examples' + ' && echo "#!/bin/sh" > @OUTPUT@ && echo "echo sha256sum requires crypto module" >> @OUTPUT@ && chmod +x @OUTPUT@'],
    build_by_default: false
  )
endif
