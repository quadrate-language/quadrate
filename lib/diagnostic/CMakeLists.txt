add_library(diagnostic INTERFACE)
target_include_directories(diagnostic INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
