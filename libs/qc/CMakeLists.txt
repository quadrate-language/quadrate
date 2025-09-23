add_library(qc STATIC
	src/ast.cc
)

find_library(U8T
	NAMES
		u8t
	REQUIRED
)

target_link_libraries(qc
	PRIVATE
	${U8T_LIBRARIES})

target_include_directories(qc
	PUBLIC
		${U8T_INCLUDE_DIRS}
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if(BUILD_TESTS)
	add_subdirectory(tests)
endif()
