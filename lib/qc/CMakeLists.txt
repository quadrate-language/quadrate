add_library(qc STATIC
	src/ast.cc
	src/ast_printer.cc
)

find_library(U8T
	NAMES
		u8t
	REQUIRED
)

target_link_libraries(qc
	PUBLIC
		${U8T})

target_include_directories(qc
	PUBLIC
		${U8T_INCLUDE_DIRS}
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if(BUILD_TESTS)
	add_subdirectory(tests)
endif()
