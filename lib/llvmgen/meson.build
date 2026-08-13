# libllvmgen - LLVM code generator for Quadrate

# Find LLVM
llvm_dep = dependency('llvm', required: false, version: '>= 14.0')

if llvm_dep.found()
	# Get LLVM cxxflags
	llvm_config = find_program('llvm-config', required: false)
	if llvm_config.found()
		llvm_cxxflags = run_command(llvm_config, '--cxxflags', check: true).stdout().strip().split()
		llvm_ldflags = run_command(llvm_config, '--ldflags', check: true).stdout().strip().split()
		llvm_libs = run_command(llvm_config, '--libs', 'core', 'native', 'orcjit', check: true).stdout().strip().split()
	else
		llvm_cxxflags = []
		llvm_ldflags = []
		llvm_libs = []
	endif

	# -Wnull-dereference is an optimiser-driven warning, so at -O3 it fires on LLVM's own
	# inlined internals (llvm/ADT/ilist_node_base.h, llvm/IR/Value.h) and is attributed to
	# whichever of our functions triggered the inlining. It reports code inside LLVM, not
	# ours -- e.g. BasicBlock::getTerminator()'s InstList.empty() guard, which GCC 16 fails
	# to see through. -isystem does not help, because the diagnostic is emitted during
	# optimisation rather than while parsing the header. Downgrade it from error to warning
	# for this target only, so the signal stays visible without breaking the release build.
	llvmgen_warn_args = ['-Wno-error=null-dereference']

	llvmgen_sources = files(
		'src/generator.cc',
		'src/generator_inline.cc',
		'src/generator_typeaware.cc',
		'src/generator_nodes.cc',
		'src/generator_nodes_instructions.cc',
		'src/generator_nodes_locals.cc',
		'src/generator_control.cc',
		'src/generator_structs.cc',
	)

	llvmgen_inc = include_directories('include')

	# Static library (with precompiled headers for LLVM)
	llvmgen_lib = static_library('llvmgen',
		llvmgen_sources,
		include_directories: [llvmgen_inc, qc_inc, qdrt_src_inc],
		dependencies: [llvm_dep, qc_dep],
		cpp_pch: 'src/generator_impl.h',
		cpp_args: llvm_cxxflags + llvmgen_warn_args,
		link_args: llvm_ldflags + llvm_libs,
		install: false
	)

	llvmgen_dep = declare_dependency(
		link_with: llvmgen_lib,
		include_directories: llvmgen_inc,
		dependencies: [llvm_dep, qc_dep]
	)
	# Tests
	if get_option('build_tests')
		subdir('tests')
	endif
else
	warning('LLVM not found, LLVM backend will not be available')
	llvmgen_dep = dependency('', required: false)
endif
