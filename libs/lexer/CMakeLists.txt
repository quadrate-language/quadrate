add_library(lexer STATIC
	src/scanner.cc
)

target_include_directories(lexer PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(lexer PUBLIC
	diagnostic
)

if(BUILD_TESTS)
	add_subdirectory(tests)
endif()
