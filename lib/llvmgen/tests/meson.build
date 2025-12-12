# LLVM generator tests

# unit-check library (subproject only)
unit_check_dep = subproject('unit-check').get_variable('unit_check_dep')

test_llvmgen = executable('test_llvmgen',
	'test_llvmgen.cc',
	include_directories: [llvmgen_inc, qc_inc],
	dependencies: [llvmgen_dep, qc_dep, unit_check_dep],
	cpp_args: llvm_cxxflags,
	link_args: llvm_ldflags + llvm_libs,
)

test('test_llvmgen', test_llvmgen)
