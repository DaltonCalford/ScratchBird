cmake_minimum_required(VERSION 3.20)
project(ScratchBird VERSION 0.0.1 LANGUAGES CXX)
include(GNUInstallDirs)

set(SCRATCHBIRD_INSTALL_RESOURCE_SUBDIR "scratchbird/resources"
    CACHE STRING "Subdirectory under CMAKE_INSTALL_DATADIR used for ScratchBird runtime resources")
set(SCRATCHBIRD_INSTALL_RESOURCE_DIR "${CMAKE_INSTALL_DATADIR}/${SCRATCHBIRD_INSTALL_RESOURCE_SUBDIR}")
set(SCRATCHBIRD_INSTALL_FULL_RESOURCE_DIR
    "${CMAKE_INSTALL_FULL_DATADIR}/${SCRATCHBIRD_INSTALL_RESOURCE_SUBDIR}")
message(STATUS "ScratchBird resource install dir: ${SCRATCHBIRD_INSTALL_FULL_RESOURCE_DIR}")

# Build-topology note for source-only reviewers:
# - scratchbird_core + scratchbird_sblr are the canonical engine runtime.
# - Multi-protocol deployments use listener/parser-agent binaries to turn
#   protocol SQL text into SBLR before it reaches the engine IPC path.
# - Engine-only builds may still link compiler/parsers for local tools/tests.
message(STATUS "ScratchBird Database Engine - active implementation tree")
message(STATUS
    "Runtime topology: core+sblr engine, listener/parser front-doors, optional in-process compiler surfaces"
)

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

# Platform feature gates used by cross-OS slices and portability guards.
if(WIN32)
    set(SCRATCHBIRD_DEFAULT_SYSTEMD_RUNTIME OFF)
    set(SCRATCHBIRD_DEFAULT_FORK_RUNTIME OFF)
elseif(UNIX AND NOT APPLE)
    set(SCRATCHBIRD_DEFAULT_SYSTEMD_RUNTIME ON)
    set(SCRATCHBIRD_DEFAULT_FORK_RUNTIME ON)
else()
    set(SCRATCHBIRD_DEFAULT_SYSTEMD_RUNTIME OFF)
    set(SCRATCHBIRD_DEFAULT_FORK_RUNTIME ON)
endif()

option(SCRATCHBIRD_ENABLE_SYSTEMD_RUNTIME
    "Enable Linux systemd runtime hooks."
    ${SCRATCHBIRD_DEFAULT_SYSTEMD_RUNTIME}
)
option(SCRATCHBIRD_ENABLE_FORK_RUNTIME
    "Enable fork/wait-based runtime components."
    ${SCRATCHBIRD_DEFAULT_FORK_RUNTIME}
)

if(WIN32 AND SCRATCHBIRD_ENABLE_SYSTEMD_RUNTIME)
    message(FATAL_ERROR "SCRATCHBIRD_ENABLE_SYSTEMD_RUNTIME cannot be ON on Windows.")
endif()
if(WIN32 AND SCRATCHBIRD_ENABLE_FORK_RUNTIME)
    message(FATAL_ERROR "SCRATCHBIRD_ENABLE_FORK_RUNTIME cannot be ON on Windows.")
endif()

if(SCRATCHBIRD_ENABLE_SYSTEMD_RUNTIME)
    set(SCRATCHBIRD_FEATURE_SYSTEMD_RUNTIME_VALUE 1)
else()
    set(SCRATCHBIRD_FEATURE_SYSTEMD_RUNTIME_VALUE 0)
endif()
if(SCRATCHBIRD_ENABLE_FORK_RUNTIME)
    set(SCRATCHBIRD_FEATURE_POSIX_DAEMON_VALUE 1)
else()
    set(SCRATCHBIRD_FEATURE_POSIX_DAEMON_VALUE 0)
endif()
if(WIN32)
    set(SCRATCHBIRD_PLATFORM_WINDOWS_VALUE 1)
    set(SCRATCHBIRD_PLATFORM_POSIX_VALUE 0)
else()
    set(SCRATCHBIRD_PLATFORM_WINDOWS_VALUE 0)
    set(SCRATCHBIRD_PLATFORM_POSIX_VALUE 1)
endif()

add_compile_definitions(
    SCRATCHBIRD_PLATFORM_WINDOWS=${SCRATCHBIRD_PLATFORM_WINDOWS_VALUE}
    SCRATCHBIRD_PLATFORM_POSIX=${SCRATCHBIRD_PLATFORM_POSIX_VALUE}
    SCRATCHBIRD_FEATURE_SYSTEMD_RUNTIME=${SCRATCHBIRD_FEATURE_SYSTEMD_RUNTIME_VALUE}
    SCRATCHBIRD_FEATURE_POSIX_DAEMON=${SCRATCHBIRD_FEATURE_POSIX_DAEMON_VALUE}
)

message(STATUS
    "Runtime feature gates: systemd=${SCRATCHBIRD_ENABLE_SYSTEMD_RUNTIME}, "
    "fork=${SCRATCHBIRD_ENABLE_FORK_RUNTIME}"
)

include(${CMAKE_SOURCE_DIR}/cmake/dependencies/windows_dependency_resolution.cmake)

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

if(WIN32)
    # Normalize Win32 headers and remove ERROR macro collisions with enum values.
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        add_compile_options(/UERROR)
    else()
        add_compile_options(-UERROR)
    endif()
endif()

# Enable CTest registration
include(CTest)
enable_testing()

# Global test timeout used by CTest when no per-test timeout is set.
# Upgrade legacy default cache values to the current default while preserving user overrides.
if(DEFINED SCRATCHBIRD_CTEST_TIMEOUT
   AND (SCRATCHBIRD_CTEST_TIMEOUT STREQUAL "900" OR SCRATCHBIRD_CTEST_TIMEOUT STREQUAL "3600"))
    set(SCRATCHBIRD_CTEST_TIMEOUT 21600 CACHE STRING "Maximum test time (seconds) - default 6 hours" FORCE)
else()
    set(SCRATCHBIRD_CTEST_TIMEOUT 21600 CACHE STRING "Maximum test time (seconds) - default 6 hours")
endif()
set(CTEST_TEST_TIMEOUT ${SCRATCHBIRD_CTEST_TIMEOUT})
message(STATUS "CTest maximum test time: ${SCRATCHBIRD_CTEST_TIMEOUT}s")

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
set(SCRATCHBIRD_WINDOWS_CROSS_COMPILE FALSE)
if(CMAKE_CROSSCOMPILING AND WIN32)
    set(SCRATCHBIRD_WINDOWS_CROSS_COMPILE TRUE)
endif()

if(NOT SCRATCHBIRD_WINDOWS_CROSS_COMPILE)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(LZ4 liblz4)
        pkg_check_modules(GEOS geos)
        pkg_check_modules(PROJ proj)
        pkg_check_modules(LIBXML2 libxml-2.0)
    endif()
else()
    message(STATUS
        "Cross-compiling to Windows: optional host dependency autodiscovery "
        "(LZ4/ZSTD/GEOS/PROJ/LIBXML2) is disabled.")
endif()

# Required packages
find_package(ZLIB REQUIRED)

if(NOT SCRATCHBIRD_WINDOWS_CROSS_COMPILE)
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
else()
    set(LZ4_FOUND FALSE)
    set(ZSTD_FOUND FALSE)
    set(GEOS_FOUND FALSE)
    set(PROJ_FOUND FALSE)
    set(LIBXML2_FOUND FALSE)
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
