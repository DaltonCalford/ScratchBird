## ScratchBird Source CMake Configuration
## Core library and executable wiring
##
## Source-only topology summary:
## - scratchbird_core + scratchbird_sblr are the canonical execution path.
## - sb_listener_* + sb_parser_* are the deployed multi-protocol front-door.
## - scratchbird_server and sb_server retain optional parser/compiler linkage
##   for engine-local tools, tests, and single-process entry points.
## - scratchbird_pool is an auxiliary pooling layer, distinct from
##   scratchbird::core::ConnectionPool in the core runtime.

# Core library: scratchbird_core
file(GLOB_RECURSE SCRATCHBIRD_CORE_SOURCES CONFIGURE_DEPENDS
    core/*.cpp
    catalog/*.cpp
    git/*.cpp
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

set(SCRATCHBIRD_INSTALL_FULL_RESOURCE_DIR_LITERAL "${SCRATCHBIRD_INSTALL_FULL_RESOURCE_DIR}")
if(WIN32)
    string(REPLACE "\\" "/" SCRATCHBIRD_INSTALL_FULL_RESOURCE_DIR_LITERAL
           "${SCRATCHBIRD_INSTALL_FULL_RESOURCE_DIR_LITERAL}")
endif()
target_compile_definitions(scratchbird_core PRIVATE
    SCRATCHBIRD_INSTALL_RESOURCE_ROOT="${SCRATCHBIRD_INSTALL_FULL_RESOURCE_DIR_LITERAL}"
)

target_include_directories(scratchbird_core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

if(SCRATCHBIRD_WINDOWS_CROSS_COMPILE)
    set(SCRATCHBIRD_MINGW_BOOST_INCLUDE "${PROJECT_SOURCE_DIR}/build/deps/mingw-boost/include")
    if(EXISTS "${SCRATCHBIRD_MINGW_BOOST_INCLUDE}/boost/multiprecision/cpp_int.hpp")
        target_include_directories(scratchbird_core PRIVATE "${SCRATCHBIRD_MINGW_BOOST_INCLUDE}")
    endif()
endif()

if(LZ4_FOUND)
    target_include_directories(scratchbird_core PUBLIC ${LZ4_INCLUDE_DIRS})
    target_link_libraries(scratchbird_core PUBLIC ${LZ4_LIBRARIES})
endif()

if(ZSTD_FOUND)
    target_include_directories(scratchbird_core PUBLIC ${ZSTD_INCLUDE_DIRS})
    target_link_libraries(scratchbird_core PUBLIC ${ZSTD_LIBRARIES})
endif()

if(GEOS_FOUND)
    target_include_directories(scratchbird_core PUBLIC ${GEOS_INCLUDE_DIRS})
    target_link_libraries(scratchbird_core PUBLIC ${GEOS_LIBRARIES})
endif()

if(PROJ_FOUND)
    target_include_directories(scratchbird_core PUBLIC ${PROJ_INCLUDE_DIRS})
    target_link_libraries(scratchbird_core PUBLIC ${PROJ_LIBRARIES})
endif()

find_package(Threads REQUIRED)
target_link_libraries(scratchbird_core PUBLIC Threads::Threads)
if(MINGW)
    target_link_libraries(scratchbird_core PUBLIC winpthread)
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
    if(WIN32)
        # MinGW static OpenSSL requires explicit Win32 system libs after libcrypto.
        target_link_libraries(OpenSSL::Crypto INTERFACE ws2_32 crypt32 iphlpapi)
        target_link_libraries(OpenSSL::SSL INTERFACE ws2_32 crypt32 iphlpapi)
    endif()
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

if(MINGW)
    target_compile_options(scratchbird_core PRIVATE -Wa,-mbig-obj)
endif()

message(STATUS
    "Core library: scratchbird_core (storage, catalog, transactions, filespaces, bootstrap resources)"
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

if(SCRATCHBIRD_WINDOWS_CROSS_COMPILE)
    if(EXISTS "${SCRATCHBIRD_MINGW_BOOST_INCLUDE}/boost/multiprecision/cpp_int.hpp")
        target_include_directories(scratchbird_parser PRIVATE "${SCRATCHBIRD_MINGW_BOOST_INCLUDE}")
    endif()
endif()

set_target_properties(scratchbird_parser PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS
    "Parser library: scratchbird_parser (SQL front-end parsers shared by parser agents and engine-local builds)"
)

# SBLR runtime library: scratchbird_sblr
set(SCRATCHBIRD_SBLR_COMPILER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/sblr/firebird_query_compiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sblr/mysql_query_compiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sblr/postgresql_query_compiler.cpp
)

file(GLOB_RECURSE SCRATCHBIRD_SBLR_SOURCES
    sblr/*.cpp
)
list(FILTER SCRATCHBIRD_SBLR_SOURCES EXCLUDE REGEX
    ".*/(firebird_query_compiler|mysql_query_compiler|postgresql_query_compiler)\\.cpp$"
)
list(REMOVE_ITEM SCRATCHBIRD_SBLR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/sblr/bytecode_validator.cpp
)
list(APPEND SCRATCHBIRD_SBLR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/sblr/bytecode_validator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/executor/parallel_executor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sblr/v3_payload_map.generated.cpp
)

add_library(scratchbird_sblr STATIC
    ${SCRATCHBIRD_SBLR_SOURCES}
)

target_include_directories(scratchbird_sblr PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

if(SCRATCHBIRD_WINDOWS_CROSS_COMPILE)
    if(EXISTS "${SCRATCHBIRD_MINGW_BOOST_INCLUDE}/boost/multiprecision/cpp_int.hpp")
        target_include_directories(scratchbird_sblr PRIVATE "${SCRATCHBIRD_MINGW_BOOST_INCLUDE}")
    endif()
endif()

target_link_libraries(scratchbird_sblr PUBLIC
    scratchbird_core
    scratchbird_parser
    nlohmann_json::nlohmann_json
    scratchbird_llvm_jit_provider
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

if(MINGW)
    target_compile_options(scratchbird_sblr PRIVATE -Wa,-mbig-obj)
endif()

message(STATUS
    "SBLR library: scratchbird_sblr (canonical engine execution/runtime representation)"
)
if(SCRATCHBIRD_LLVM_JIT_AVAILABLE)
    message(STATUS
        "SBLR JIT provider: LLVM ${SCRATCHBIRD_LLVM_JIT_PROVIDER_VERSION} (${SCRATCHBIRD_LLVM_JIT_HOST_TRIPLE})"
    )
else()
    message(STATUS
        "SBLR JIT provider: unavailable, builds retain deterministic VM fallback"
    )
endif()

# scratchbird_core references Executor for scheduler-run procedures, so the
# static library graph must model the core <-> sblr cycle explicitly.
target_link_libraries(scratchbird_core PUBLIC scratchbird_sblr)


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
        scratchbird_optimizer_compiler
    )

    set_target_properties(scratchbird_compiler PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
    )

    message(STATUS
        "Compiler library: scratchbird_compiler (SQL/AST to SBLR bridge for parser agents and engine-local builds)"
    )
else()
    message(STATUS "Compiler library: scratchbird_compiler disabled")
endif()

add_library(scratchbird_link_group INTERFACE)
set(SCRATCHBIRD_LINK_GROUP_LIBS
    scratchbird_core
    scratchbird_sblr
    scratchbird_parser
    scratchbird_optimizer_runtime
    scratchbird_security
    scratchbird_ipc
)
if(SCRATCHBIRD_WITH_COMPILER)
    list(APPEND SCRATCHBIRD_LINK_GROUP_LIBS
        scratchbird_compiler
        scratchbird_optimizer_compiler
    )
endif()

# RESCAN link groups are not supported on all linkers (for example MSVC).
if(CMAKE_CXX_LINK_GROUP_USING_RESCAN_SUPPORTED)
    string(JOIN "," SCRATCHBIRD_LINK_GROUP_LIBS_CSV ${SCRATCHBIRD_LINK_GROUP_LIBS})
    target_link_libraries(scratchbird_link_group INTERFACE
        $<LINK_GROUP:RESCAN,${SCRATCHBIRD_LINK_GROUP_LIBS_CSV}>
    )
else()
    target_link_libraries(scratchbird_link_group INTERFACE
        ${SCRATCHBIRD_LINK_GROUP_LIBS}
    )
endif()
if(NOT WIN32)
    target_link_options(scratchbird_link_group INTERFACE -Wl,--no-as-needed)
endif()
if(PROJ_FOUND)
    target_link_libraries(scratchbird_link_group INTERFACE ${PROJ_LIBRARIES})
endif()
if(GEOS_FOUND)
    target_link_libraries(scratchbird_link_group INTERFACE ${GEOS_LIBRARIES})
endif()
if(ZSTD_FOUND)
    target_link_libraries(scratchbird_link_group INTERFACE ${ZSTD_LIBRARIES})
endif()
if(OPENSSL_FOUND)
    target_link_libraries(scratchbird_link_group INTERFACE OpenSSL::SSL OpenSSL::Crypto)
endif()


# Optimizer runtime library: scratchbird_optimizer_runtime
set(SCRATCHBIRD_OPTIMIZER_RUNTIME_SOURCES
    optimizer/cost_model.cpp
    optimizer/multi_track_classifier.cpp
    optimizer/plan_payload.cpp
    optimizer/vnext_plan_cache.cpp
    optimizer/vnext_plan_selection.cpp
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

target_link_libraries(scratchbird_optimizer_runtime PUBLIC
    nlohmann_json::nlohmann_json
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
        optimizer/index_family_lowering.cpp
        optimizer/query_planner.cpp
        optimizer/selectivity_estimator.cpp
        optimizer/join_ordering.cpp
        optimizer/v3_semantic_analyzer.cpp
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

    # The SBLR optimizer support bridge invokes QueryPlanner directly.
    target_link_libraries(scratchbird_sblr PUBLIC scratchbird_optimizer_compiler)

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
# Security Library: scratchbird_security
# ==============================================================================
# SSL/TLS, authentication methods, HBA configuration, audit logging
# Components: TLS context, SCRAM-SHA-256/512, certificate auth, rate limiting

file(GLOB SCRATCHBIRD_SECURITY_SOURCES
    security/*.cpp
    security/providers/*.cpp
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
target_link_libraries(scratchbird_security PUBLIC Threads::Threads nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(scratchbird_security PUBLIC ws2_32)
endif()

set_target_properties(scratchbird_security PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "Security library: scratchbird_security (TLS, auth, HBA, audit)")

# Auth plugin stubs (phase-1 deterministic module layout)
option(SCRATCHBIRD_BUILD_AUTH_PLUGIN_STUBS
       "Build phase-1 auth plugin shared-module stubs"
       ON)
if(SCRATCHBIRD_BUILD_AUTH_PLUGIN_STUBS)
    add_subdirectory(security/plugins)
endif()

# Core needs security helpers (SCRAM/base64) but security only depends on core headers.
target_link_libraries(scratchbird_core PUBLIC scratchbird_security)

# ==============================================================================
# Network Library: scratchbird_network
# ==============================================================================
# Shared network infrastructure for listeners, adapters, and socket runtimes
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
set(SCRATCHBIRD_PLATFORM_NETWORK_LIBS "")
if(WIN32)
    list(APPEND SCRATCHBIRD_PLATFORM_NETWORK_LIBS ws2_32)
endif()
target_link_libraries(scratchbird_network PUBLIC
    Threads::Threads
    scratchbird_core
    ${SCRATCHBIRD_PLATFORM_NETWORK_LIBS}
)

set_target_properties(scratchbird_network PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "Network library: scratchbird_network (socket, event loop, thread pool, connections)")

# ==============================================================================
# Server Library: scratchbird_server
# ==============================================================================
# Engine IPC infrastructure, session management, and lifecycle/runtime control

file(GLOB SCRATCHBIRD_SERVER_SOURCES
    server/ipc_*.cpp
    server/server_session.cpp
    server/scratchbird_server.cpp
    server/config_parser.cpp
    server/daemon.cpp
    server/service_controller.cpp
    server/windows_service.cpp
)

add_library(scratchbird_server STATIC
    ${SCRATCHBIRD_SERVER_SOURCES}
)

target_include_directories(scratchbird_server PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# Note: Dependencies are added after scratchbird_protocol is defined (circular dependency resolution)

# Thread support for multi-client handling
find_package(Threads REQUIRED)
set(SCRATCHBIRD_PLATFORM_SERVER_LIBS "")
if(WIN32)
    # Windows: Winsock2 for TCP, Advapi32 for security descriptors
    list(APPEND SCRATCHBIRD_PLATFORM_SERVER_LIBS ws2_32 advapi32)
elseif(SCRATCHBIRD_ENABLE_SYSTEMD_RUNTIME)
    # POSIX: dynamic loading used for optional systemd runtime hooks.
    list(APPEND SCRATCHBIRD_PLATFORM_SERVER_LIBS ${CMAKE_DL_LIBS})
endif()
target_link_libraries(scratchbird_server PUBLIC
    Threads::Threads
    ${SCRATCHBIRD_PLATFORM_SERVER_LIBS}
)

set_target_properties(scratchbird_server PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS
    "Server library: scratchbird_server (engine session manager + IPC execution bridge)"
)

# ==============================================================================
# Protocol Library: scratchbird_protocol
# ==============================================================================
# Wire codecs, native protocol helpers, and protocol-surface client utilities

file(GLOB_RECURSE SCRATCHBIRD_PROTOCOL_SOURCES
    protocol/*.cpp
)
list(APPEND SCRATCHBIRD_PROTOCOL_SOURCES
    client/connection.cpp
    client/sql_helpers.cpp
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

message(STATUS
    "Protocol library: scratchbird_protocol (wire codecs and client helpers for listeners, parser agents, and local clients)"
)

# Add server dependencies (after protocol is defined to resolve circular dependency)
target_link_libraries(scratchbird_server PUBLIC
    scratchbird_core
    scratchbird_protocol
    scratchbird_sblr
)
# scratchbird_server keeps an optional compiler/parser link for engine-local
# entry points and tests. In the deployed multi-protocol topology, SQL text is
# parsed by sb_parser_* agents and the engine IPC path consumes SBLR payloads.
if(SCRATCHBIRD_WITH_COMPILER)
    target_link_libraries(scratchbird_server PUBLIC scratchbird_compiler scratchbird_parser)
else()
    target_link_libraries(scratchbird_server PUBLIC scratchbird_parser)
endif()

# ==============================================================================
# Pool Library: scratchbird_pool
# ==============================================================================
# Experimental auxiliary pool/cache library.
# Components: PoolManager, DatabasePool, PooledConnection, StatementCache,
# ResultCache
#
# This library remains buildable for tests and cache primitives, but it is not
# the supported product/runtime connection path and is therefore not installed
# with the product target set.

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

message(STATUS
    "Pool library: scratchbird_pool (experimental auxiliary pool/cache library; not installed as a product runtime surface)"
)

# ==============================================================================
# FDW Library: scratchbird_fdw
# ==============================================================================
# Foreign-data-wrapper style access paths for remote databases
# Components: PostgreSQL, MySQL, Firebird adapters, connection pooling

option(SCRATCHBIRD_ENABLE_FDW "Build FDW adapters library" ON)
option(SCRATCHBIRD_ENABLE_UDR "Build UDR connector library" ON)
option(SCRATCHBIRD_ENABLE_IPC_RUNTIME "Build IPC runtime/static library" ON)
option(SCRATCHBIRD_ENABLE_TESTING_LIB "Build testing helper library" ON)

if(WIN32)
    if(SCRATCHBIRD_ENABLE_FDW)
        message(STATUS
            "FDW library disabled on Windows toolchains pending portability closure.")
        set(SCRATCHBIRD_ENABLE_FDW OFF)
    endif()
    if(SCRATCHBIRD_ENABLE_UDR)
        message(STATUS
            "UDR library disabled on Windows toolchains pending portability closure.")
        set(SCRATCHBIRD_ENABLE_UDR OFF)
    endif()
    if(SCRATCHBIRD_ENABLE_IPC_RUNTIME)
        message(STATUS
            "IPC runtime library disabled on Windows toolchains pending AF_UNIX portability closure.")
        set(SCRATCHBIRD_ENABLE_IPC_RUNTIME OFF)
    endif()
    if(SCRATCHBIRD_ENABLE_TESTING_LIB)
        message(STATUS
            "Testing library disabled on Windows toolchains pending socket portability closure.")
        set(SCRATCHBIRD_ENABLE_TESTING_LIB OFF)
    endif()
endif()

if(SCRATCHBIRD_ENABLE_FDW)
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
endif()

# ==============================================================================
# UDR Library: scratchbird_udr (Section D: Engine Tools + Bridge UDRs)
# ==============================================================================
# User Defined Routine runtime/tooling layer.
# Components:
# - language/runtime helpers used by in-engine tools and EXECUTE STATEMENT-style
#   SQL->SBLR compilation flows
# - outbound bridge connectors for PostgreSQL/MySQL/Firebird/ScratchBird
# - cluster/server-to-server bridge support built on the same connector model

if(SCRATCHBIRD_ENABLE_UDR)
    file(GLOB SCRATCHBIRD_UDR_SOURCES
        udr/*.cpp
    )

    add_library(scratchbird_udr STATIC
        ${SCRATCHBIRD_UDR_SOURCES}
    )

    # OpenSSL required for PostgreSQL MD5 auth and TLS
    if(OPENSSL_FOUND)
        target_include_directories(scratchbird_udr PUBLIC ${OPENSSL_INCLUDE_DIR})
        target_link_libraries(scratchbird_udr PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    endif()

    target_include_directories(scratchbird_udr PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    )

    # Thread support for connection pool
    find_package(Threads REQUIRED)
    target_link_libraries(scratchbird_udr PUBLIC Threads::Threads scratchbird_core)

    set_target_properties(scratchbird_udr PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
    )

    message(STATUS
        "UDR library: scratchbird_udr (language/runtime tools, SQL->SBLR helpers, and outbound bridge connectors)"
    )
endif()
if(TARGET scratchbird_udr)
    if(TARGET scratchbird_fdw)
        target_link_libraries(scratchbird_fdw PUBLIC scratchbird_udr)
    endif()
    # Keep UDR after the core archive link-group so static resolution can satisfy
    # SBLR symbols that depend on UDR helper implementations.
    target_link_libraries(scratchbird_link_group INTERFACE scratchbird_udr)
endif()

# ==============================================================================
# IPC Library: scratchbird_ipc (Section E: IPC + SBWP v1.1)
# ==============================================================================
# Inter-process communication infrastructure for engine-parser communication
# Components: IPC contract v1.1, message framing, session management, IPC server

file(GLOB SCRATCHBIRD_IPC_SOURCES CONFIGURE_DEPENDS
    ipc/*.cpp
    ipc/external_agents/*.cpp
)
if(WIN32)
    # unix_socket_channel.cpp is POSIX-only; use the Windows stub implementation.
    list(FILTER SCRATCHBIRD_IPC_SOURCES EXCLUDE REGEX ".*/unix_socket_channel\\.cpp$")
endif()

if(SCRATCHBIRD_ENABLE_IPC_RUNTIME)
    add_library(scratchbird_ipc STATIC
        ${SCRATCHBIRD_IPC_SOURCES}
    )

    target_include_directories(scratchbird_ipc PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    )

    # Thread support for session management
    find_package(Threads REQUIRED)
    target_link_libraries(scratchbird_ipc PUBLIC Threads::Threads scratchbird_core)

    set_target_properties(scratchbird_ipc PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
    )

    message(STATUS "IPC library: scratchbird_ipc (IPC Contract v1.1 + SBWP Server)")
else()
    add_library(scratchbird_ipc INTERFACE)
    target_include_directories(scratchbird_ipc INTERFACE
        ${PROJECT_SOURCE_DIR}/include
    )
    message(STATUS "IPC library: scratchbird_ipc disabled (interface stub target)")
endif()

# ==============================================================================
# Testing Library: scratchbird_testing
# ==============================================================================
# Comprehensive testing framework for protocol compliance, authentication,
# load/stress testing, security testing, and performance benchmarks
# Components: ProtocolTester, AuthTester, LoadTester, SecurityTester, BenchmarkRunner

file(GLOB SCRATCHBIRD_TESTING_SOURCES
    testing/*.cpp
)
if(WIN32)
    # POSIX socket-only testers are excluded from native Windows evidence builds.
    list(FILTER SCRATCHBIRD_TESTING_SOURCES EXCLUDE REGEX ".*/AuthTester\\.cpp$")
    list(FILTER SCRATCHBIRD_TESTING_SOURCES EXCLUDE REGEX ".*/ProtocolTester\\.cpp$")
endif()

if(SCRATCHBIRD_ENABLE_TESTING_LIB)
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
else()
    add_library(scratchbird_testing INTERFACE)
    target_include_directories(scratchbird_testing INTERFACE
        ${PROJECT_SOURCE_DIR}/include
    )
    message(STATUS "Testing library: scratchbird_testing disabled (interface stub target)")
endif()

# ==============================================================================
# sb_server Executable
# ==============================================================================
# Database server service controller entry point

add_executable(sb_server server/sb_server_main.cpp)

target_link_libraries(sb_server PRIVATE
    scratchbird_server
    scratchbird_protocol
    scratchbird_sblr
    scratchbird_core
    scratchbird_link_group
)
if(TARGET scratchbird_udr)
    target_link_libraries(sb_server PRIVATE scratchbird_udr)
endif()
if(SCRATCHBIRD_WITH_COMPILER)
    target_link_libraries(sb_server PRIVATE scratchbird_compiler scratchbird_parser)
else()
    target_link_libraries(sb_server PRIVATE scratchbird_parser)
endif()

set_target_properties(sb_server PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

message(STATUS
    "Server executable: sb_server (service controller with optional engine-local compiler surfaces)"
)

# ==============================================================================
# sb_manager Executable (Manager Proxy Front-Door Scaffold)
# ==============================================================================

add_executable(sb_manager server/sb_manager_main.cpp)
target_link_libraries(sb_manager PRIVATE
    scratchbird_network
    scratchbird_protocol
    scratchbird_core
)

set_target_properties(sb_manager PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

message(STATUS "Manager executable: sb_manager")

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

message(STATUS
    "Listener executables: sb_listener_native/pg/mysql/fb (protocol front-doors that hand off to parser agents)"
)

# ==============================================================================
# Parser Agent Binaries (Protocol Parsers)
# ==============================================================================

add_executable(sb_parser_native parser/sb_parser_main.cpp)
target_compile_definitions(sb_parser_native PRIVATE
    SB_PARSER_PROTOCOL="scratchbird"
    SB_PARSER_NAME="sb_parser_native"
)
target_link_libraries(sb_parser_native PRIVATE scratchbird_protocol scratchbird_core scratchbird_parser scratchbird_link_group)
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
target_link_libraries(sb_parser_pg PRIVATE scratchbird_protocol scratchbird_core scratchbird_parser scratchbird_link_group)
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
target_link_libraries(sb_parser_mysql PRIVATE scratchbird_protocol scratchbird_core scratchbird_parser scratchbird_link_group)
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
target_link_libraries(sb_parser_fb PRIVATE scratchbird_protocol scratchbird_core scratchbird_parser scratchbird_link_group)
set_target_properties(sb_parser_fb PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

message(STATUS
    "Parser agent executables: sb_parser_native/pg/mysql/fb (protocol-specific SQL compilers producing SBLR for the engine)"
)

# ==============================================================================
# Main executable (developer CLI utility)
add_executable(scratchbird main.cpp)

target_link_libraries(scratchbird PRIVATE scratchbird_link_group)
if(TARGET scratchbird_udr)
    target_link_libraries(scratchbird PRIVATE scratchbird_udr)
endif()

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

# Installation (for future use)
set(SCRATCHBIRD_INSTALL_TARGETS
    scratchbird
    sb_server
    sb_manager
    sb_charset_loader
    scratchbird_core
    scratchbird_parser
    scratchbird_sblr
    scratchbird_optimizer_runtime
    scratchbird_server
    scratchbird_protocol
    scratchbird_testing
)
if(TARGET scratchbird_fdw)
    list(APPEND SCRATCHBIRD_INSTALL_TARGETS scratchbird_fdw)
endif()
if(TARGET scratchbird_udr)
    list(APPEND SCRATCHBIRD_INSTALL_TARGETS scratchbird_udr)
endif()

install(TARGETS ${SCRATCHBIRD_INSTALL_TARGETS}
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/resources/"
    DESTINATION "${SCRATCHBIRD_INSTALL_RESOURCE_DIR}"
    PATTERN "__pycache__" EXCLUDE
    PATTERN "*.pyc" EXCLUDE
    PATTERN "*.pyo" EXCLUDE
)

if(TARGET scratchbird_compiler)
    install(TARGETS scratchbird_compiler scratchbird_optimizer_compiler
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
    )
endif()
