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

# Find optional packages
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(LZ4 liblz4)
endif()

# If LZ4 not found via pkg-config, try to find it manually
if(NOT LZ4_FOUND)
    find_path(LZ4_INCLUDE_DIRS NAMES lz4.h)
    find_library(LZ4_LIBRARIES NAMES lz4)
    if(LZ4_INCLUDE_DIRS AND LZ4_LIBRARIES)
        set(LZ4_FOUND TRUE)
    endif()
endif()

# Define macro if LZ4 is available
if(LZ4_FOUND)
    add_compile_definitions(HAVE_LZ4)
    message(STATUS "LZ4 compression support: ENABLED")
else()
    message(STATUS "LZ4 compression support: DISABLED (install liblz4-dev to enable)")
endif()

# Add source directory
add_subdirectory(src)

# Tests
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()