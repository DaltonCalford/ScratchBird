cmake_minimum_required(VERSION 3.20)
project(ScratchBird VERSION 0.0.1 LANGUAGES CXX)

# Project in planning phase
message(STATUS "ScratchBird Database Engine - Planning Phase")
message(STATUS "See ProjectPlan/ directory for specifications")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Add source directory
add_subdirectory(src)

# Tests
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()