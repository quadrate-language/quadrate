cmake_minimum_required(VERSION 3.20)
project(Quadrate LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
