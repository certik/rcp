project(rcp)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_CXX_FLAGS "-g -Wall -Wfatal-errors")

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/src)

add_subdirectory(src)
add_subdirectory(examples)
