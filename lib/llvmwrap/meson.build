# libllvmwrap - LLVM C API wrappers for the self-hosted compiler

llvm_dep = dependency('llvm', required: true, version: '>=14')

llvmwrap_sources = files('src/llvmwrap.c')
llvmwrap_inc = include_directories('include')

llvmwrap_static = static_library('llvmwrap',
	llvmwrap_sources,
	include_directories: llvmwrap_inc,
	dependencies: [qd_static_dep, llvm_dep],
	c_args: ['-ffunction-sections', '-fdata-sections'],
	install: false
)

llvmwrap_dep = declare_dependency(
	link_with: llvmwrap_static,
	include_directories: llvmwrap_inc,
	dependencies: llvm_dep
)
