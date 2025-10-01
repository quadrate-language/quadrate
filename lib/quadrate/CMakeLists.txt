add_library(quadrate SHARED
	src/qd.cc
)

target_include_directories(quadrate
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

set_target_properties(quadrate PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/lib
)

add_custom_command(TARGET quadrate POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/dist/include/quadrate
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/include/quadrate/qd.h ${CMAKE_SOURCE_DIR}/dist/include/quadrate/qd.h
)

