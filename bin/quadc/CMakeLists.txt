add_executable(quadc
	src/compiler.cc
	src/main.cc
)

target_link_libraries(quadc
	PRIVATE u8t
)

