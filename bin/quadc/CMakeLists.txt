add_executable(quadc
	src/compiler.cc
	src/main.cc
)

target_link_libraries(quadc
	PRIVATE u8t
)

set_target_properties(quadc PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/bin
)

