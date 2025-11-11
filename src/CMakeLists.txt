## ScratchBird Source CMake Configuration
## Core library and executable wiring

# Core library: scratchbird_core
file(GLOB_RECURSE SCRATCHBIRD_CORE_SOURCES
    core/*.cpp
)

# Spatial library sources
file(GLOB_RECURSE SCRATCHBIRD_SPATIAL_SOURCES
    spatial/*.cpp
)

# Geo library sources (SRID and PROJ integration)
file(GLOB_RECURSE SCRATCHBIRD_GEO_SOURCES
    geo/*.cpp
)

add_library(scratchbird_core STATIC
    ${SCRATCHBIRD_CORE_SOURCES}
    ${SCRATCHBIRD_SPATIAL_SOURCES}
    ${SCRATCHBIRD_GEO_SOURCES}
)

target_include_directories(scratchbird_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

if(LZ4_FOUND)
    target_include_directories(scratchbird_core PUBLIC ${LZ4_INCLUDE_DIRS})
    target_link_libraries(scratchbird_core PUBLIC ${LZ4_LIBRARIES})
endif()

if(GEOS_FOUND)
    target_include_directories(scratchbird_core PUBLIC ${GEOS_INCLUDE_DIRS})
    target_link_libraries(scratchbird_core PUBLIC ${GEOS_LIBRARIES})
endif()

if(PROJ_FOUND)
    target_include_directories(scratchbird_core PUBLIC ${PROJ_INCLUDE_DIRS})
    target_link_libraries(scratchbird_core PUBLIC ${PROJ_LIBRARIES})
endif()

# Link with libcrypt for password hashing (Security Phase 3.0)
find_library(CRYPT_LIBRARY crypt)
if(CRYPT_LIBRARY)
    target_link_libraries(scratchbird_core PUBLIC ${CRYPT_LIBRARY})
    message(STATUS "libcrypt found: Password hashing enabled")
else()
    message(WARNING "libcrypt not found: Password hashing will use fallback (insecure)")
endif()

# Link with OpenSSL for secure random number generation (Security Phase 3.0)
find_package(OpenSSL)
if(OPENSSL_FOUND)
    target_include_directories(scratchbird_core PUBLIC ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(scratchbird_core PUBLIC OpenSSL::Crypto)
    message(STATUS "OpenSSL found: Using secure random for password salts")
else()
    message(WARNING "OpenSSL not found: Using std::random_device for salts")
endif()

set_target_properties(scratchbird_core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

# Parser library: scratchbird_parser
file(GLOB_RECURSE SCRATCHBIRD_PARSER_SOURCES
    parser/*.cpp
)

add_library(scratchbird_parser STATIC
    ${SCRATCHBIRD_PARSER_SOURCES}
)

target_include_directories(scratchbird_parser PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(scratchbird_parser PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

# SBLR library: scratchbird_sblr
file(GLOB_RECURSE SCRATCHBIRD_SBLR_SOURCES
    sblr/*.cpp
)

add_library(scratchbird_sblr STATIC
    ${SCRATCHBIRD_SBLR_SOURCES}
)

target_include_directories(scratchbird_sblr PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(scratchbird_sblr PUBLIC
    scratchbird_parser
    scratchbird_core
    nlohmann_json::nlohmann_json
)

set_target_properties(scratchbird_sblr PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

# Optimizer library: scratchbird_optimizer
file(GLOB_RECURSE SCRATCHBIRD_OPTIMIZER_SOURCES
    optimizer/*.cpp
)

add_library(scratchbird_optimizer STATIC
    ${SCRATCHBIRD_OPTIMIZER_SOURCES}
)

target_include_directories(scratchbird_optimizer PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(scratchbird_optimizer PUBLIC
    scratchbird_core
)

set_target_properties(scratchbird_optimizer PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

# Main executable (placeholder)
add_executable(scratchbird main.cpp)

target_link_libraries(scratchbird PRIVATE scratchbird_core scratchbird_parser scratchbird_sblr scratchbird_optimizer)

set_target_properties(scratchbird PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Installation (for future use)
install(TARGETS scratchbird scratchbird_core scratchbird_parser scratchbird_sblr scratchbird_optimizer
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)