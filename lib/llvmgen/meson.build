# libllvmgen - LLVM code generator for Quadrate

# Find LLVM
llvm_dep = dependency('llvm', required: false, version: '>= 14.0')

if llvm_dep.found()
	# Get LLVM cxxflags
	llvm_config = find_program('llvm-config', required: false)
	if llvm_config.found()
		llvm_cxxflags = run_command(llvm_config, '--cxxflags', check: true).stdout().strip().split()
		llvm_ldflags = run_command(llvm_config, '--ldflags', check: true).stdout().strip().split()
		llvm_libs = run_command(llvm_config, '--libs', 'core', 'native', check: true).stdout().strip().split()
	else
		llvm_cxxflags = []
		llvm_ldflags = []
		llvm_libs = []
	endif

	llvmgen_sources = files(
		'src/generator.cc',
	)

	llvmgen_inc = include_directories('include')

	# Static library
	llvmgen_lib = static_library('llvmgen',
		llvmgen_sources,
		include_directories: [llvmgen_inc, qc_inc],
		dependencies: [llvm_dep, qc_dep],
		cpp_args: llvm_cxxflags,
		link_args: llvm_ldflags + llvm_libs,
		install: false
	)

	llvmgen_dep = declare_dependency(
		link_with: llvmgen_lib,
		include_directories: llvmgen_inc,
		dependencies: [llvm_dep, qc_dep]
	)
else
	warning('LLVM not found, LLVM backend will not be available')
	llvmgen_dep = dependency('', required: false)
endif
