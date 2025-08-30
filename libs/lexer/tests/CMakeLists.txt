add_executable(test_scanner
	test_scanner.cc
)
find_library(UNIT_CHECK
	NAMES unit-check
	REQUIRED
)

target_link_libraries(test_scanner PRIVATE lexer ${UNIT_CHECK})

add_test(NAME test_scanner COMMAND test_scanner)
