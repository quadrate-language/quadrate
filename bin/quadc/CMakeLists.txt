add_executable(quadc
	src/main.cc
)

target_link_libraries(quadc
	PRIVATE
		cgen
		qc
)

set_target_properties(quadc PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/bin
)

