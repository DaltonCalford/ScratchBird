cmake_minimum_required(VERSION 3.10)
project(libcds-stub)

# Create a minimal stub library to satisfy linking requirements
add_library(cds-s STATIC empty.cpp)
add_library(cds-s_d STATIC empty.cpp)

# Create dummy binary directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

# Create empty library files
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/empty.cpp "// Empty stub for libcds")