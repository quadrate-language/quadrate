add_executable(quadfmt
	src/main.cc
)

target_link_libraries(quadfmt
	PRIVATE u8t
)

set_target_properties(quadfmt PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/bin
)

