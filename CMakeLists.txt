cmake_minimum_required(VERSION 3.20)
project(Quadrate LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_compile_options(
	-Wall
	-Wextra
	-Wpedantic
	-Wconversion
	-Wsign-conversion
	-Wshadow
	-Wcast-align
	-Wunused
	-Wnull-dereference
	-Wdouble-promotion
	-Wformat=2
	-Werror
)

# C++-specific warnings
add_compile_options(
	$<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
	$<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
	$<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
)

# if(CMAKE_BUILD_TYPE STREQUAL "Debug")
# 	add_compile_options(-fsanitize=address,undefined)
# 	add_link_options(-fsanitize=address,undefined)
# endif()

include(CTest)

add_subdirectory(lib)
add_subdirectory(bin)

option(BUILD_EXAMPLES "Build examples" OFF)
if(BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(BUILD_TESTS)
	message(STATUS "Building tests")
endif()
