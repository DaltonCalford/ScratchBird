## ScratchBird Source CMake Configuration
## Core library and executable wiring

# Core library: scratchbird_core
file(GLOB_RECURSE SCRATCHBIRD_CORE_SOURCES
    core/*.cpp
)

# Spatial library sources (exclude disabled_p1_15 directory)
file(GLOB_RECURSE SCRATCHBIRD_SPATIAL_SOURCES
    spatial/*.cpp
)
# Remove disabled files
list(FILTER SCRATCHBIRD_SPATIAL_SOURCES EXCLUDE REGEX ".*/disabled_p1_15/.*")

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

# Note: scratchbird_optimizer dependency will be added after optimizer library is defined

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

# Link with libxml2 for XML support (Nov 14, 2025)
if(LIBXML2_FOUND)
    target_include_directories(scratchbird_sblr PUBLIC ${LIBXML2_INCLUDE_DIRS})
    target_link_libraries(scratchbird_sblr PUBLIC ${LIBXML2_LIBRARIES})
    target_compile_definitions(scratchbird_sblr PUBLIC HAVE_LIBXML2)
    message(STATUS "libxml2 found: Full XML/XPath support enabled")
else()
    message(WARNING "libxml2 not found: XML functions will use basic string-based implementation")
endif()

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
    # Note: Not linking scratchbird_core to avoid circular dependency
    # scratchbird_core will link against scratchbird_optimizer instead
)
# Link OpenSSL if available for RAND_bytes in statistics_manager.cpp
if(OPENSSL_FOUND)
    target_link_libraries(scratchbird_optimizer PUBLIC OpenSSL::Crypto)
endif()

set_target_properties(scratchbird_optimizer PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

# Add optimizer dependency to core (after optimizer is defined)
target_link_libraries(scratchbird_core PUBLIC scratchbird_optimizer nlohmann_json::nlohmann_json)

# ==============================================================================
# Server Library: scratchbird_server (Local Server Architecture - Phase 1)
# ==============================================================================
# IPC infrastructure for client-server communication
# Supports: Unix domain sockets (Linux/macOS), Named pipes (Windows), TCP localhost

file(GLOB SCRATCHBIRD_SERVER_SOURCES
    server/*.cpp
)

add_library(scratchbird_server STATIC
    ${SCRATCHBIRD_SERVER_SOURCES}
)

target_include_directories(scratchbird_server PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(scratchbird_server PUBLIC
    scratchbird_core
)

# Platform-specific linking for server
if(WIN32)
    # Windows: Winsock2 for TCP, Advapi32 for security descriptors
    target_link_libraries(scratchbird_server PUBLIC ws2_32 advapi32)
endif()

# Thread support for multi-client handling
find_package(Threads REQUIRED)
target_link_libraries(scratchbird_server PUBLIC Threads::Threads)

set_target_properties(scratchbird_server PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "Server library: scratchbird_server (IPC infrastructure)")

# ==============================================================================
# Protocol Library: scratchbird_protocol (Local Server Architecture - Phase 2)
# ==============================================================================
# Wire protocol for client-server message encoding/decoding

file(GLOB SCRATCHBIRD_PROTOCOL_SOURCES
    protocol/*.cpp
)

add_library(scratchbird_protocol STATIC
    ${SCRATCHBIRD_PROTOCOL_SOURCES}
)

target_include_directories(scratchbird_protocol PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(scratchbird_protocol PUBLIC
    scratchbird_server
    scratchbird_core
)

set_target_properties(scratchbird_protocol PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "Protocol library: scratchbird_protocol (wire protocol)")

# ==============================================================================
# Main executable (placeholder)
add_executable(scratchbird main.cpp)

target_link_libraries(scratchbird PRIVATE scratchbird_sblr scratchbird_parser scratchbird_optimizer scratchbird_core)

set_target_properties(scratchbird PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Data loader tools
add_executable(sb_charset_loader ${CMAKE_SOURCE_DIR}/tools/sb_charset_loader.cpp)

target_link_libraries(sb_charset_loader PRIVATE scratchbird_core)

set_target_properties(sb_charset_loader PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Installation (for future use)
install(TARGETS scratchbird sb_charset_loader scratchbird_core scratchbird_parser scratchbird_sblr scratchbird_optimizer scratchbird_server scratchbird_protocol
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)