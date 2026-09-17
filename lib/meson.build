# Common include directories (must be defined before subdirs that use them)
qdrt_src_inc = include_directories('rt')

subdir('version')
subdir('platform')
subdir('cli')
subdir('rt')
subdir('qc')
subdir('interp')

# The compiler back end. lib/rt, lib/qc and lib/interp are the runtime path and
# stay unconditional; llvmgen pulls LLVM and lib/qd is built on top of it.
if build_compiler
	subdir('llvmgen')
	subdir('qd')
endif
