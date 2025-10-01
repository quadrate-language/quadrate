add_executable(embed
	main.c
)

target_link_libraries(embed
	PRIVATE quadrate
)

set_target_properties(embed PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/examples
)
