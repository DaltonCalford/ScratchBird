## ScratchBird Source CMake Configuration
## Core library and executable wiring

# Core library: scratchbird_core
file(GLOB_RECURSE SCRATCHBIRD_CORE_SOURCES CONFIGURE_DEPENDS
    core/*.cpp
    catalog/*.cpp
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
    ${PROJECT_SOURCE_DIR}/include
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
target_link_libraries(scratchbird_core PUBLIC ZLIB::ZLIB)
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

# Note: scratchbird_optimizer_runtime dependency will be added after optimizer library is defined

# Parser library: scratchbird_parser
file(GLOB_RECURSE SCRATCHBIRD_PARSER_SOURCES
    parser/*.cpp
)
list(FILTER SCRATCHBIRD_PARSER_SOURCES EXCLUDE REGEX ".*/sb_parser_main\\.cpp$")

add_library(scratchbird_parser STATIC
    ${SCRATCHBIRD_PARSER_SOURCES}
)

target_include_directories(scratchbird_parser PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

set_target_properties(scratchbird_parser PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

# SBLR runtime library: scratchbird_sblr
set(SCRATCHBIRD_SBLR_COMPILER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/sblr/bytecode_generator_v2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sblr/semantic_analyzer_v2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sblr/query_compiler_v2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sblr/firebird_query_compiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sblr/mysql_query_compiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sblr/postgresql_query_compiler.cpp
)

file(GLOB_RECURSE SCRATCHBIRD_SBLR_SOURCES
    sblr/*.cpp
)
list(FILTER SCRATCHBIRD_SBLR_SOURCES EXCLUDE REGEX
    ".*/(bytecode_generator_v2|semantic_analyzer_v2|query_compiler_v2|firebird_query_compiler|mysql_query_compiler|postgresql_query_compiler)\\.cpp$"
)
list(REMOVE_ITEM SCRATCHBIRD_SBLR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/sblr/bytecode_validator.cpp
)
list(APPEND SCRATCHBIRD_SBLR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/sblr/bytecode_validator.cpp
)

add_library(scratchbird_sblr STATIC
    ${SCRATCHBIRD_SBLR_SOURCES}
)

target_include_directories(scratchbird_sblr PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(scratchbird_sblr PUBLIC
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


# Compiler library: scratchbird_compiler (parsers + semantic + bytecode)
if(SCRATCHBIRD_WITH_COMPILER)
    add_library(scratchbird_compiler STATIC
        ${SCRATCHBIRD_SBLR_COMPILER_SOURCES}
    )

    target_include_directories(scratchbird_compiler PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    )

    target_link_libraries(scratchbird_compiler PUBLIC
        scratchbird_parser
        scratchbird_sblr
    )

    set_target_properties(scratchbird_compiler PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
    )
endif()

add_library(scratchbird_link_group INTERFACE)
if(SCRATCHBIRD_WITH_COMPILER)
    target_link_libraries(scratchbird_link_group INTERFACE
        $<LINK_GROUP:RESCAN,scratchbird_core,scratchbird_sblr,scratchbird_compiler,scratchbird_parser,scratchbird_optimizer_runtime,scratchbird_optimizer_compiler,scratchbird_security>
    )
else()
    target_link_libraries(scratchbird_link_group INTERFACE
        $<LINK_GROUP:RESCAN,scratchbird_core,scratchbird_sblr,scratchbird_parser,scratchbird_optimizer_runtime,scratchbird_security>
    )
endif()
target_link_options(scratchbird_link_group INTERFACE -Wl,--no-as-needed)
if(PROJ_FOUND)
    target_link_libraries(scratchbird_link_group INTERFACE ${PROJ_LIBRARIES})
endif()
if(GEOS_FOUND)
    target_link_libraries(scratchbird_link_group INTERFACE ${GEOS_LIBRARIES})
endif()


# Optimizer runtime library: scratchbird_optimizer_runtime
set(SCRATCHBIRD_OPTIMIZER_RUNTIME_SOURCES
    optimizer/cost_model.cpp
    optimizer/statistics_manager.cpp
    optimizer/cse.cpp
    optimizer/expression_matcher.cpp
    optimizer/predicate_matcher.cpp
    optimizer/query_profiler.cpp
)

add_library(scratchbird_optimizer_runtime STATIC
    ${SCRATCHBIRD_OPTIMIZER_RUNTIME_SOURCES}
)

target_include_directories(scratchbird_optimizer_runtime PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# Link OpenSSL if available for RAND_bytes in statistics_manager.cpp
if(OPENSSL_FOUND)
    target_link_libraries(scratchbird_optimizer_runtime PUBLIC OpenSSL::Crypto)
endif()

set_target_properties(scratchbird_optimizer_runtime PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

# Optimizer compiler library: scratchbird_optimizer_compiler (parser-dependent)
if(SCRATCHBIRD_WITH_COMPILER)
    set(SCRATCHBIRD_OPTIMIZER_COMPILER_SOURCES
        optimizer/mv_rewriter.cpp
        optimizer/index_advisor.cpp
        optimizer/query_planner.cpp
        optimizer/selectivity_estimator.cpp
        optimizer/join_ordering.cpp
    )

    add_library(scratchbird_optimizer_compiler STATIC
        ${SCRATCHBIRD_OPTIMIZER_COMPILER_SOURCES}
    )

    target_include_directories(scratchbird_optimizer_compiler PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    )

    target_link_libraries(scratchbird_optimizer_compiler PUBLIC
        scratchbird_optimizer_runtime
        scratchbird_parser  # Parser dependency for AST-based optimizers
    )

    set_target_properties(scratchbird_optimizer_compiler PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
    )
endif()


# Add optimizer runtime dependency to core (after optimizer is defined)
target_link_libraries(scratchbird_core PUBLIC scratchbird_optimizer_runtime nlohmann_json::nlohmann_json)

# ==============================================================================
# Security Library: scratchbird_security (Alpha 3 Phase 3.4)
# ==============================================================================
# SSL/TLS, authentication methods, HBA configuration, audit logging
# Components: TLS context, SCRAM-SHA-256/512, certificate auth, rate limiting

file(GLOB SCRATCHBIRD_SECURITY_SOURCES
    security/*.cpp
)

add_library(scratchbird_security STATIC
    ${SCRATCHBIRD_SECURITY_SOURCES}
)

target_include_directories(scratchbird_security PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# OpenSSL required for TLS and crypto
if(OPENSSL_FOUND)
    target_include_directories(scratchbird_security PUBLIC ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(scratchbird_security PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    message(STATUS "Security library: OpenSSL found, full TLS/SSL support enabled")
else()
    message(FATAL_ERROR "OpenSSL not found: Required for security library")
endif()

# Thread support for auth state management
find_package(Threads REQUIRED)
target_link_libraries(scratchbird_security PUBLIC Threads::Threads)

set_target_properties(scratchbird_security PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "Security library: scratchbird_security (TLS, auth, HBA, audit)")

# Core needs security helpers (SCRAM/base64) but security only depends on core headers.
target_link_libraries(scratchbird_core PUBLIC scratchbird_security)

# ==============================================================================
# Network Library: scratchbird_network (Alpha 3 Phase 3.1)
# ==============================================================================
# High-performance network infrastructure for multi-protocol server
# Components: socket abstraction, event loop (epoll/kqueue), thread pool, connection management

file(GLOB SCRATCHBIRD_NETWORK_SOURCES
    network/*.cpp
)
list(FILTER SCRATCHBIRD_NETWORK_SOURCES EXCLUDE REGEX ".*/sb_listener_main\\.cpp$")

add_library(scratchbird_network STATIC
    ${SCRATCHBIRD_NETWORK_SOURCES}
)

target_include_directories(scratchbird_network PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# Thread support for thread pool
find_package(Threads REQUIRED)
target_link_libraries(scratchbird_network PUBLIC Threads::Threads scratchbird_core)

# Platform-specific linking for network
if(WIN32)
    target_link_libraries(scratchbird_network PUBLIC ws2_32)
endif()

set_target_properties(scratchbird_network PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "Network library: scratchbird_network (socket, event loop, thread pool, connections)")

# ==============================================================================
# Server Library: scratchbird_server (Local Server Architecture - Phase 1 & 3)
# ==============================================================================
# IPC infrastructure and server components for client-server communication
# Phase 1: Unix domain sockets (Linux/macOS), Named pipes (Windows), TCP localhost
# Phase 3: Session management, client handling, query execution

file(GLOB SCRATCHBIRD_SERVER_SOURCES
    server/ipc_*.cpp
    server/server_session.cpp
    server/scratchbird_server.cpp
    server/config_parser.cpp
    server/daemon.cpp
    server/service_controller.cpp
)

add_library(scratchbird_server STATIC
    ${SCRATCHBIRD_SERVER_SOURCES}
)

target_include_directories(scratchbird_server PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# Note: Dependencies are added after scratchbird_protocol is defined (circular dependency resolution)

# Platform-specific linking for server
if(WIN32)
    # Windows: Winsock2 for TCP, Advapi32 for security descriptors
    target_link_libraries(scratchbird_server PUBLIC ws2_32 advapi32)
endif()

# Thread support for multi-client handling
find_package(Threads REQUIRED)
target_link_libraries(scratchbird_server PUBLIC Threads::Threads)

# Dynamic loading for libsystemd (Phase 3.3 Service Mode)
if(NOT WIN32)
    target_link_libraries(scratchbird_server PUBLIC ${CMAKE_DL_LIBS})
endif()

set_target_properties(scratchbird_server PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "Server library: scratchbird_server (IPC + session management)")

# ==============================================================================
# Protocol Library: scratchbird_protocol (Local Server Architecture - Phase 2)
# ==============================================================================
# Wire protocol for client-server message encoding/decoding

file(GLOB_RECURSE SCRATCHBIRD_PROTOCOL_SOURCES
    protocol/*.cpp
)

add_library(scratchbird_protocol STATIC
    ${SCRATCHBIRD_PROTOCOL_SOURCES}
)

target_include_directories(scratchbird_protocol PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(scratchbird_protocol PUBLIC
    scratchbird_server
    scratchbird_network
    scratchbird_core
)

set_target_properties(scratchbird_protocol PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "Protocol library: scratchbird_protocol (wire protocol)")

# Add server dependencies (after protocol is defined to resolve circular dependency)
target_link_libraries(scratchbird_server PUBLIC
    scratchbird_core
    scratchbird_protocol
    scratchbird_sblr
)
if(SCRATCHBIRD_WITH_COMPILER)
    target_link_libraries(scratchbird_server PUBLIC scratchbird_compiler)
endif()

# ==============================================================================
# Client Library: scratchbird_client (Local Server Architecture - Phase 4)
# ==============================================================================
# Client library for connecting to ScratchBird server
# Provides Connection, ResultSet, PreparedStatement, and ConnectionPool classes

file(GLOB SCRATCHBIRD_CLIENT_SOURCES
    client/*.cpp
)

add_library(scratchbird_client STATIC
    ${SCRATCHBIRD_CLIENT_SOURCES}
)

target_include_directories(scratchbird_client PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(scratchbird_client PUBLIC
    scratchbird_protocol
    scratchbird_server
    scratchbird_network
    scratchbird_security
    scratchbird_core
)

# Thread support for connection pooling
target_link_libraries(scratchbird_client PUBLIC Threads::Threads)

set_target_properties(scratchbird_client PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "Client library: scratchbird_client (connection, result sets, pooling)")

# ==============================================================================
# Shared Client Library: libscratchbird (Alpha ODBC bridge)
# ==============================================================================
# Shared wrapper around scratchbird_client for driver integration

add_library(scratchbird_lib SHARED
    ${SCRATCHBIRD_CLIENT_SOURCES}
)

target_include_directories(scratchbird_lib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(scratchbird_lib PUBLIC
    scratchbird_protocol
    scratchbird_server
    scratchbird_network
    scratchbird_security
    scratchbird_core
    Threads::Threads
)

set_target_properties(scratchbird_lib PROPERTIES
    OUTPUT_NAME scratchbird
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "Shared client library: libscratchbird (network client wrapper)")

# ==============================================================================
# Pool Library: scratchbird_pool (Alpha 3 Phase 3.6)
# ==============================================================================
# Connection pooling with statement and result caching
# Components: PoolManager, DatabasePool, PooledConnection, StatementCache, ResultCache

file(GLOB SCRATCHBIRD_POOL_SOURCES
    pool/*.cpp
)

add_library(scratchbird_pool STATIC
    ${SCRATCHBIRD_POOL_SOURCES}
)

target_include_directories(scratchbird_pool PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# Thread support for connection pool management
find_package(Threads REQUIRED)
target_link_libraries(scratchbird_pool PUBLIC Threads::Threads scratchbird_core)

set_target_properties(scratchbird_pool PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "Pool library: scratchbird_pool (connection pooling, statement/result caching)")

# ==============================================================================
# FDW Library: scratchbird_fdw (Alpha 3 Phase 3.7)
# ==============================================================================
# Foreign Data Wrapper for connecting to remote databases
# Components: PostgreSQL, MySQL, Firebird adapters, connection pooling

file(GLOB SCRATCHBIRD_FDW_SOURCES
    fdw/*.cpp
)

add_library(scratchbird_fdw STATIC
    ${SCRATCHBIRD_FDW_SOURCES}
)

target_include_directories(scratchbird_fdw PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# OpenSSL required for MySQL password hashing and TLS
if(OPENSSL_FOUND)
    target_include_directories(scratchbird_fdw PUBLIC ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(scratchbird_fdw PUBLIC OpenSSL::Crypto)
endif()

# Thread support for connection pool management
find_package(Threads REQUIRED)
target_link_libraries(scratchbird_fdw PUBLIC Threads::Threads scratchbird_core)

set_target_properties(scratchbird_fdw PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "FDW library: scratchbird_fdw (PostgreSQL, MySQL, Firebird adapters)")

# ==============================================================================
# ODBC Library: scratchbird_odbc (Alpha 3 Phase 3.8)
# ==============================================================================
# ODBC driver for client connectivity and odbc_fdw for external database access
# Components: ODBC 3.8 API, connection strings, type mapping, catalog functions

file(GLOB SCRATCHBIRD_ODBC_SOURCES
    odbc/*.cpp
)

add_library(scratchbird_odbc STATIC
    ${SCRATCHBIRD_ODBC_SOURCES}
)

target_include_directories(scratchbird_odbc PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# Thread support for connection management
find_package(Threads REQUIRED)
target_link_libraries(scratchbird_odbc PUBLIC Threads::Threads scratchbird_core scratchbird_lib)

set_target_properties(scratchbird_odbc PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

add_library(scratchbird_odbc_driver SHARED
    ${SCRATCHBIRD_ODBC_SOURCES}
)

target_include_directories(scratchbird_odbc_driver PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(scratchbird_odbc_driver PUBLIC Threads::Threads scratchbird_core scratchbird_lib)

target_compile_definitions(scratchbird_odbc_driver PRIVATE SCRATCHBIRD_ODBC_EXPORTS)

set_target_properties(scratchbird_odbc_driver PROPERTIES
    OUTPUT_NAME scratchbird_odbc
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "ODBC library: scratchbird_odbc (ODBC 3.8 driver)")

# ==============================================================================
# Testing Library: scratchbird_testing (Alpha 3 Phase 3.12)
# ==============================================================================
# Comprehensive testing framework for protocol compliance, authentication,
# load/stress testing, security testing, and performance benchmarks
# Components: ProtocolTester, AuthTester, LoadTester, SecurityTester, BenchmarkRunner

file(GLOB SCRATCHBIRD_TESTING_SOURCES
    testing/*.cpp
)

add_library(scratchbird_testing STATIC
    ${SCRATCHBIRD_TESTING_SOURCES}
)

target_include_directories(scratchbird_testing PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# Thread support for load testing
find_package(Threads REQUIRED)
target_link_libraries(scratchbird_testing PUBLIC Threads::Threads scratchbird_core)

set_target_properties(scratchbird_testing PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "Testing library: scratchbird_testing (protocol, auth, load, security, benchmarks)")

# ==============================================================================
# sb_server Executable (Local Server Architecture - Phase 3)
# ==============================================================================
# Database server daemon

add_executable(sb_server server/sb_server_main.cpp)

target_link_libraries(sb_server PRIVATE
    scratchbird_server
    scratchbird_protocol
    scratchbird_sblr
    scratchbird_core
)
if(SCRATCHBIRD_WITH_COMPILER)
    target_link_libraries(sb_server PRIVATE scratchbird_compiler)
endif()

set_target_properties(sb_server PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

message(STATUS "Server executable: sb_server")

# ==============================================================================
# Listener Binaries (Network Listener + Parser Pool)
# ==============================================================================

add_executable(sb_listener_native network/sb_listener_main.cpp)
target_compile_definitions(sb_listener_native PRIVATE
    SB_LISTENER_PROTOCOL="scratchbird"
    SB_LISTENER_NAME="sb_listener_native"
)
target_link_libraries(sb_listener_native PRIVATE scratchbird_network scratchbird_server scratchbird_core)
set_target_properties(sb_listener_native PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_listener_pg network/sb_listener_main.cpp)
target_compile_definitions(sb_listener_pg PRIVATE
    SB_LISTENER_PROTOCOL="postgresql"
    SB_LISTENER_NAME="sb_listener_pg"
)
target_link_libraries(sb_listener_pg PRIVATE scratchbird_network scratchbird_server scratchbird_core)
set_target_properties(sb_listener_pg PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_listener_mysql network/sb_listener_main.cpp)
target_compile_definitions(sb_listener_mysql PRIVATE
    SB_LISTENER_PROTOCOL="mysql"
    SB_LISTENER_NAME="sb_listener_mysql"
)
target_link_libraries(sb_listener_mysql PRIVATE scratchbird_network scratchbird_server scratchbird_core)
set_target_properties(sb_listener_mysql PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_listener_fb network/sb_listener_main.cpp)
target_compile_definitions(sb_listener_fb PRIVATE
    SB_LISTENER_PROTOCOL="firebird"
    SB_LISTENER_NAME="sb_listener_fb"
)
target_link_libraries(sb_listener_fb PRIVATE scratchbird_network scratchbird_server scratchbird_core)
set_target_properties(sb_listener_fb PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

message(STATUS "Listener executables: sb_listener_native/pg/mysql/fb")

# ==============================================================================
# Parser Agent Binaries (Protocol Parsers)
# ==============================================================================

add_executable(sb_parser_native parser/sb_parser_main.cpp)
target_compile_definitions(sb_parser_native PRIVATE
    SB_PARSER_PROTOCOL="scratchbird"
    SB_PARSER_NAME="sb_parser_native"
)
target_link_libraries(sb_parser_native PRIVATE scratchbird_protocol scratchbird_client scratchbird_core)
set_target_properties(sb_parser_native PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_parser_pg parser/sb_parser_main.cpp)
target_compile_definitions(sb_parser_pg PRIVATE
    SB_PARSER_PROTOCOL="postgresql"
    SB_PARSER_NAME="sb_parser_pg"
)
target_link_libraries(sb_parser_pg PRIVATE scratchbird_protocol scratchbird_client scratchbird_core)
set_target_properties(sb_parser_pg PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_parser_mysql parser/sb_parser_main.cpp)
target_compile_definitions(sb_parser_mysql PRIVATE
    SB_PARSER_PROTOCOL="mysql"
    SB_PARSER_NAME="sb_parser_mysql"
)
target_link_libraries(sb_parser_mysql PRIVATE scratchbird_protocol scratchbird_client scratchbird_core)
set_target_properties(sb_parser_mysql PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(sb_parser_fb parser/sb_parser_main.cpp)
target_compile_definitions(sb_parser_fb PRIVATE
    SB_PARSER_PROTOCOL="firebird"
    SB_PARSER_NAME="sb_parser_fb"
)
target_link_libraries(sb_parser_fb PRIVATE scratchbird_protocol scratchbird_client scratchbird_core)
set_target_properties(sb_parser_fb PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

message(STATUS "Parser agent executables: sb_parser_native/pg/mysql/fb")

# ==============================================================================
# Main executable (placeholder)
add_executable(scratchbird main.cpp)

target_link_libraries(scratchbird PRIVATE scratchbird_link_group)

set_target_properties(scratchbird PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Data loader tools
add_executable(sb_charset_loader ${PROJECT_SOURCE_DIR}/tools/sb_charset_loader.cpp)

target_link_libraries(sb_charset_loader PRIVATE scratchbird_link_group)

set_target_properties(sb_charset_loader PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# ==============================================================================
# CLI Tools (CLI Administration Suite)
# ==============================================================================

# sb_isql - Interactive SQL Shell
add_executable(sb_isql cli/sb_isql.cpp)

target_link_libraries(sb_isql PRIVATE
    scratchbird_client
    scratchbird_protocol
    scratchbird_server
    scratchbird_link_group
)

set_target_properties(sb_isql PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

message(STATUS "CLI tool: sb_isql (interactive SQL shell)")

# sb_verify - Database Verification Tool
add_executable(sb_verify cli/sb_verify.cpp)

target_link_libraries(sb_verify PRIVATE
    scratchbird_link_group
)

set_target_properties(sb_verify PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

message(STATUS "CLI tool: sb_verify (database verification)")

# sb_backup - Backup/Restore Tool
add_executable(sb_backup cli/sb_backup.cpp)

target_link_libraries(sb_backup PRIVATE
    scratchbird_link_group
)

set_target_properties(sb_backup PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

message(STATUS "CLI tool: sb_backup (backup/restore)")

# sb_security - Security Administration Tool
add_executable(sb_security cli/sb_security.cpp)

target_link_libraries(sb_security PRIVATE
    scratchbird_client
    scratchbird_protocol
    scratchbird_server
    scratchbird_link_group
)

set_target_properties(sb_security PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

message(STATUS "CLI tool: sb_security (security administration)")

# sb_admin - Administration Tool
add_executable(sb_admin cli/sb_admin.cpp)

target_link_libraries(sb_admin PRIVATE
    scratchbird_client
    scratchbird_protocol
    scratchbird_server
    scratchbird_link_group
)

set_target_properties(sb_admin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

message(STATUS "CLI tool: sb_admin (administration)")

# sb_fb_isql - Firebird Interactive SQL Shell
add_executable(sb_fb_isql cli/sb_fb_isql.cpp)

target_link_libraries(sb_fb_isql PRIVATE
    scratchbird_link_group
)

set_target_properties(sb_fb_isql PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

message(STATUS "CLI tool: sb_fb_isql (Firebird SQL shell)")

# sb_pg_isql - PostgreSQL Interactive SQL Shell
add_executable(sb_pg_isql cli/sb_pg_isql.cpp)

target_link_libraries(sb_pg_isql PRIVATE
    scratchbird_fdw
    scratchbird_link_group
)

set_target_properties(sb_pg_isql PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

message(STATUS "CLI tool: sb_pg_isql (PostgreSQL SQL shell)")

# sb_my_isql - MySQL Interactive SQL Shell
add_executable(sb_my_isql cli/sb_my_isql.cpp)

target_link_libraries(sb_my_isql PRIVATE
    scratchbird_fdw
    scratchbird_link_group
)

set_target_properties(sb_my_isql PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

message(STATUS "CLI tool: sb_my_isql (MySQL SQL shell)")

# Installation (for future use)
install(TARGETS scratchbird sb_server sb_charset_loader sb_isql sb_fb_isql sb_pg_isql sb_my_isql sb_verify sb_backup sb_security sb_admin scratchbird_core scratchbird_parser scratchbird_sblr scratchbird_optimizer_runtime scratchbird_server scratchbird_protocol scratchbird_client scratchbird_lib scratchbird_pool scratchbird_fdw scratchbird_odbc scratchbird_odbc_driver scratchbird_testing
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

if(TARGET scratchbird_compiler)
    install(TARGETS scratchbird_compiler scratchbird_optimizer_compiler
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
    )
endif()
