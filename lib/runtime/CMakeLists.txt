add_library(runtime STATIC
	src/stack.c
)

set_target_properties(runtime PROPERTIES
	LINKER_LANGUAGE C
	C_STANDARD 11
	C_STANDARD_REQUIRED ON
)

target_include_directories(runtime
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
