add_library(u8 STATIC
	src/str.cc
)

target_include_directories(u8 PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if(BUILD_TESTS)
	add_subdirectory(tests)
endif()
