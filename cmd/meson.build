fs = import('fs')

# Listed rather than globbed so the build order and the set of tools stay
# explicit, but skipped when the directory is absent: the floppy distribution
# ships a source subset with cmd/quadlsp, cmd/quadrepl and cmd/quadmcp removed,
# and used to patch this file with sed to match.
foreach tool : ['quad', 'quadc', 'quadfmt', 'quadlint', 'quadlsp', 'quadpm', 'quaduses', 'quaddoc', 'quadrepl']
	if fs.is_dir(tool)
		subdir(tool)
	endif
endforeach
