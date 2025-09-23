add_executable(test_str
	test_str.cc
)
find_library(UNIT_CHECK
	NAMES
		unit-check
	REQUIRED
)

target_link_libraries(test_str
	PRIVATE
		u8
		${UNIT_CHECK}
)

add_test(NAME test_str COMMAND test_str)
