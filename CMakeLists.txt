cmake_minimum_required(VERSION 3.20)
project(ScratchBird VERSION 0.0.1 LANGUAGES CXX)

# Project in planning phase
message(STATUS "ScratchBird Database Engine - Planning Phase")
message(STATUS "See ProjectPlan/ directory for specifications")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# Add clang-tidy and clang-format integration
# Temporarily disabled due to incompatible .clang-tidy configuration
# find_program(CLANG_TIDY clang-tidy)
# if(CLANG_TIDY)
#     set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY} -p=${CMAKE_BINARY_DIR})
#     message(STATUS "clang-tidy found, will be used for linting.")
# else()
#     message(WARNING "clang-tidy not found. Linting will be skipped.")
# endif()

find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i -style=file ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/src/core/*.cpp ${PROJECT_SOURCE_DIR}/src/parser/*.cpp ${PROJECT_SOURCE_DIR}/src/sblr/*.cpp ${PROJECT_SOURCE_DIR}/include/scratchbird/core/*.h ${PROJECT_SOURCE_DIR}/include/scratchbird/parser/*.h ${PROJECT_SOURCE_DIR}/include/scratchbird/sblr/*.h ${PROJECT_SOURCE_DIR}/tests/unit/*.cpp ${PROJECT_SOURCE_DIR}/tests/integration/*.cpp
        COMMENT "Formatting C++ source files with clang-format")
    message(STATUS "clang-format found. Use 'make format' to format the code.")
else()
    message(WARNING "clang-format not found. 'make format' target will not be available.")
endif()

# Add source directory
add_subdirectory(src)

# Tests
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()