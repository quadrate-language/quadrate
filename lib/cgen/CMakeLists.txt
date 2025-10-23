add_library(cgen STATIC
	src/compiler.cc
	src/linker.cc
	src/transpiler.cc
)

set_target_properties(cgen PROPERTIES
	LINKER_LANGUAGE CXX
)

target_link_libraries(cgen
	PUBLIC
		qc
)

target_include_directories(cgen
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if(BUILD_TESTS)
	add_subdirectory(tests)
endif()
