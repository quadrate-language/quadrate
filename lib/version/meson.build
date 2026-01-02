# Version header with git info
version_data = configuration_data()
version_data.set('VERSION', meson.project_version())

# Get git commit hash
git = find_program('git', required: false)
if git.found()
    vcs_tag_cmd = run_command(git, 'describe', '--always', '--dirty', check: false)
    if vcs_tag_cmd.returncode() == 0
        vcs_tag = vcs_tag_cmd.stdout().strip()
    else
        vcs_tag = 'unknown'
    endif
else
    vcs_tag = 'unknown'
endif
version_data.set('VCS_TAG', vcs_tag)

version_h = configure_file(
    input: 'version.h.in',
    output: 'version.h',
    configuration: version_data
)

version_inc = include_directories('.')
