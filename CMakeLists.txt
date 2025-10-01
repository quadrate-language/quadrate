cmake_minimum_required(VERSION 3.20)
project(Quadrate LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(CTest)

add_subdirectory(lib/diagnostic)
add_subdirectory(lib/qc)
add_subdirectory(lib/u8)
add_subdirectory(lib/quadrate)
add_subdirectory(bin/quadc)
add_subdirectory(bin/quadfmt)

if(BUILD_TESTS)
	message(STATUS "Building tests")
endif()
