add_executable(test_ast
	test_ast.cc
)
find_library(UNIT_CHECK
	NAMES
		unit-check
	REQUIRED
)

target_link_libraries(test_ast
	PRIVATE
		qc
		${UNIT_CHECK}
)

add_test(NAME test_ast COMMAND test_ast)
