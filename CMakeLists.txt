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

# Compiler/parsers (engine-only builds can disable)
option(SCRATCHBIRD_WITH_COMPILER "Build SQL compiler/parsers" ON)
if(SCRATCHBIRD_WITH_COMPILER)
    add_compile_definitions(SCRATCHBIRD_WITH_COMPILER=1)
else()
    add_compile_definitions(SCRATCHBIRD_WITH_COMPILER=0)
endif()

# Fetch nlohmann/json library (header-only, no external dependencies needed)
include(FetchContent)
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)
message(STATUS "nlohmann/json library: ENABLED (FetchContent)")

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Enable CTest registration
include(CTest)
enable_testing()

# Default to parallel test runs when using the built-in test target.
include(ProcessorCount)
ProcessorCount(SCRATCHBIRD_CPU_COUNT)
set(SCRATCHBIRD_TEST_PARALLEL_LEVEL "" CACHE STRING "Default CTest parallel level (empty=auto)")
if(NOT SCRATCHBIRD_TEST_PARALLEL_LEVEL)
    if(SCRATCHBIRD_CPU_COUNT GREATER 1)
        set(SCRATCHBIRD_TEST_PARALLEL_LEVEL ${SCRATCHBIRD_CPU_COUNT})
    else()
        set(SCRATCHBIRD_TEST_PARALLEL_LEVEL 1)
    endif()
endif()
if(SCRATCHBIRD_TEST_PARALLEL_LEVEL GREATER 1)
    set(CMAKE_CTEST_ARGUMENTS
        "--parallel" "${SCRATCHBIRD_TEST_PARALLEL_LEVEL}"
        "--output-on-failure"
    )
    message(STATUS "CTest parallel level: ${SCRATCHBIRD_TEST_PARALLEL_LEVEL}")
endif()

# Find optional packages
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(LZ4 liblz4)
    pkg_check_modules(GEOS geos)
    pkg_check_modules(PROJ proj)
    pkg_check_modules(LIBXML2 libxml-2.0)
endif()

# Required packages
find_package(ZLIB REQUIRED)

# If LZ4 not found via pkg-config, try to find it manually
if(NOT LZ4_FOUND)
    find_path(LZ4_INCLUDE_DIRS NAMES lz4.h)
    find_library(LZ4_LIBRARIES NAMES lz4)
    if(LZ4_INCLUDE_DIRS AND LZ4_LIBRARIES)
        set(LZ4_FOUND TRUE)
    endif()
endif()

# If ZSTD not found via pkg-config, try to find it manually
if(NOT ZSTD_FOUND)
    find_path(ZSTD_INCLUDE_DIRS NAMES zstd.h)
    find_library(ZSTD_LIBRARIES NAMES zstd)
    if(ZSTD_INCLUDE_DIRS AND ZSTD_LIBRARIES)
        set(ZSTD_FOUND TRUE)
    endif()
endif()

# If GEOS not found via pkg-config, try to find it manually
if(NOT GEOS_FOUND)
    find_path(GEOS_INCLUDE_DIRS NAMES geos_c.h)
    find_library(GEOS_LIBRARIES NAMES geos_c)
    if(GEOS_INCLUDE_DIRS AND GEOS_LIBRARIES)
        set(GEOS_FOUND TRUE)
    endif()
endif()

# If PROJ not found via pkg-config, try to find it manually
if(NOT PROJ_FOUND)
    find_path(PROJ_INCLUDE_DIRS NAMES proj.h)
    find_library(PROJ_LIBRARIES NAMES proj)
    if(PROJ_INCLUDE_DIRS AND PROJ_LIBRARIES)
        set(PROJ_FOUND TRUE)
    endif()
endif()

# Define macro if LZ4 is available
if(LZ4_FOUND)
    add_compile_definitions(HAVE_LZ4)
    message(STATUS "LZ4 compression support: ENABLED")
else()
    message(STATUS "LZ4 compression support: DISABLED (install liblz4-dev to enable)")
endif()

if(ZSTD_FOUND)
    message(STATUS "ZSTD compression support: ENABLED")
else()
    message(STATUS "ZSTD compression support: DISABLED (install libzstd-dev to enable)")
endif()

# Define macro if GEOS is available (Phase 2 Wave 3: Spatial Functions)
if(GEOS_FOUND)
    add_compile_definitions(HAVE_GEOS)
    message(STATUS "GEOS spatial library: ENABLED (${GEOS_VERSION})")
else()
    message(STATUS "GEOS spatial library: DISABLED (install libgeos-dev to enable)")
endif()

# Define macro if PROJ is available (Phase 2 Wave 3: Coordinate Systems)
if(PROJ_FOUND)
    add_compile_definitions(HAVE_PROJ)
    message(STATUS "PROJ coordinate system library: ENABLED (${PROJ_VERSION})")
else()
    message(STATUS "PROJ coordinate system library: DISABLED (install libproj-dev to enable)")
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

# Tools
add_subdirectory(tools)
