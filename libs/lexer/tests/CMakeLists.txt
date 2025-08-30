add_executable(test_scanner
	test_scanner.cc
)

target_link_libraries(test_scanner PRIVATE lexer)

add_test(NAME test_scanner COMMAND test_scanner)
