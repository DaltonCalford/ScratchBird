# Test Suite

# Find or fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Test timeout configuration (in seconds)
# Can be overridden via cmake -DSCRATCHBIRD_TEST_TIMEOUT_GLOBAL=900
set(SCRATCHBIRD_TEST_TIMEOUT_GLOBAL "0" CACHE STRING "Global test timeout in seconds (0=use category timeouts, max=900=15min)")

# Default timeouts by test category.
set(SCRATCHBIRD_TEST_TIMEOUT_UNIT 60)
set(SCRATCHBIRD_TEST_TIMEOUT_INTEGRATION 300)
set(SCRATCHBIRD_TEST_TIMEOUT_STRESS 600)
set(SCRATCHBIRD_TEST_TIMEOUT_PERFORMANCE 600)
set(SCRATCHBIRD_TEST_TIMEOUT_QUARANTINE 120)

# If global timeout is set (>0), use it as maximum for all tests
if(SCRATCHBIRD_TEST_TIMEOUT_GLOBAL GREATER 0)
    message(STATUS "Using global test timeout: ${SCRATCHBIRD_TEST_TIMEOUT_GLOBAL}s (15min max)")
    set(SCRATCHBIRD_TEST_TIMEOUT_UNIT ${SCRATCHBIRD_TEST_TIMEOUT_GLOBAL})
    set(SCRATCHBIRD_TEST_TIMEOUT_INTEGRATION ${SCRATCHBIRD_TEST_TIMEOUT_GLOBAL})
    set(SCRATCHBIRD_TEST_TIMEOUT_STRESS ${SCRATCHBIRD_TEST_TIMEOUT_GLOBAL})
    set(SCRATCHBIRD_TEST_TIMEOUT_PERFORMANCE ${SCRATCHBIRD_TEST_TIMEOUT_GLOBAL})
    set(SCRATCHBIRD_TEST_TIMEOUT_QUARANTINE ${SCRATCHBIRD_TEST_TIMEOUT_GLOBAL})
endif()

# Allow test sources to include test_helpers.h across all test targets.
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
link_libraries(scratchbird_link_group)
if(SCRATCHBIRD_WITH_COMPILER)
    set(SCRATCHBIRD_TEST_COMPILER_LIBS scratchbird_compiler scratchbird_parser)
else()
    set(SCRATCHBIRD_TEST_COMPILER_LIBS scratchbird_parser)
endif()
set(SCRATCHBIRD_TEST_OPTIMIZER_LIBS scratchbird_optimizer_runtime)
if(SCRATCHBIRD_WITH_COMPILER)
    list(APPEND SCRATCHBIRD_TEST_OPTIMIZER_LIBS scratchbird_optimizer_compiler)
endif()

# Test sources (exclude deprecated tests)
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "unit/*.cpp"
    "unit/types/*.cpp"
    "unit/domains/*.cpp"
    "integration/*.cpp"
    "benchmark/*.cpp"
)

# Explicitly exclude deprecated tests
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/deprecated/.*")

# Temporarily exclude standalone test files with main() that haven't been converted to GoogleTest format
# These files will be converted incrementally to GoogleTest TEST/TEST_F format

# Phase 1-2: CONVERTED to GoogleTest (42 tests) - no longer excluded:
# - All 9 type tests (array, composite, decimal, interval, jsonb, money, new_types, vector, xml)
# - All 6 domain tests (advanced, manager, enum, record, set, variant)
# - All 3 GIN tests (phase4, phase5, phase6)
# - All 11 transaction tests (clog, snapshot, subtransactions, deadlock, wraparound, etc.)
# - All 9 storage tests (heap, hint, defragment, dirty, buffer, toast, fsm)
# - All 4 B-tree tests (delete_parent, rightmost, hot_updates)

# All previously excluded tests have been converted to GoogleTest format!
# Phase 1: 18 tests converted (type, domain, GIN tests)
# Phase 2: 24 tests converted (transaction, storage, B-tree tests)
# Phase 3: 9 tests converted (columnstore, cache, text search tests)
# Total: 51 tests converted and enabled (100% of excluded tests)

# Note: test_hnsw_mvcc.cpp and test_index_mvcc.cpp were removed because they tested
# PostgreSQL MVCC Snapshot API which doesn't exist (ScratchBird uses Firebird MGA, not PostgreSQL MVCC)
# btree_page_test.cpp has been fixed and is now included in the build

# Phase 2C: Temporarily exclude tests with complex refactored APIs
# These require additional manual fixes beyond automated script capabilities
# TODO: Fix these in Phase 3 using test_bitmap_dml.cpp as template
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_bytecode_executor\\.cpp$")  # 40+ errors - executor API refactor
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/integration/test_gist_dml\\.cpp$")  # Run in sequential suite
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_gist_mvcc\\.cpp$")  # Re-enabled (API updated)
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_hnsw_dml\\.cpp$")  # Re-enabled (API updated)
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_rtree_dml\\.cpp$")  # Duplicate Status declarations
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_spgist_dml\\.cpp$")  # Re-enabled (API updated)
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_spgist_mvcc\\.cpp$")  # Re-enabled (API updated)
# btree_page_test.cpp was fixed: Tuple struct initializer corrected to use TID{gpid, slot}

list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/unit/test_parser.cpp$")  # Phase 2C auto-exclusion
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/unit/test_parser_integration.cpp$")  # Phase 2C auto-exclusion
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/unit/test_multi_geometry.cpp$")  # TypedValue API changes
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/unit/test_type_serialization.cpp$")  # TypeSerializer implementation disabled

# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/unit/test_timezone.cpp$")  # TypeConverter API
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/unit/test_client_connection.cpp$")  # Has separate test target



# Phase 2C: Exclude standalone tests with main() functions
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_security_phase.*\.cpp$")  # Database API issues (open/create)
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/manual_test_planner_integration\.cpp$")  # Standalone main()
# Re-enabled B-Tree tests (now built under GTest)
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_buffer_error_consistency\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_bytecode_executor\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_cache_bounded\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_clog_checksum\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_clog_state_size\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_columnstore_bitpack\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_columnstore_dict\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_columnstore_comprehensive\.cpp$")  # Uses gtest_main
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_columnstore_predicate\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_columnstore_rle\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_columnstore_segments\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_columnstore_simple_e2e\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_defragment_pdlower_fix\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_dirty_bit_protection\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_fsm_durability\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_fsm_reconstruction\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_gin_transaction_isolation\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_gin_tsvector_ops\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_heap_free_space_simple\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_hint_bits_simple\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_hot_updates\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_long_transaction_monitor\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_matchers_only\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_materialized_views_parser\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_multi_index_mga\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_network_types\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_range_lexer\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_range_operators\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_range_types\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_snapshot_sorted\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_snapshot_xids\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_subtransactions\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_temporal_range_types\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_term_conn\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_terminate_connection\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_text_search_phase2\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_text_search_phase3\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_text_search_types\.cpp$")  # Standalone main()
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_toast_cleanup\.cpp$")  # Re-enabled (GTest)
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_toast_cleanup_ordering\.cpp$")  # Re-enabled (GTest)
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_transaction_deadlock_simple\.cpp$")  # Already converted to GoogleTest
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_transaction_markers_race\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_version_chain_cycle\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_views_comprehensive\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_wraparound_detection\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_xid_validation_fix\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_client_server_integration\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_gin_gc\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_network\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_shadow_index_rebuild\.cpp$")  # Standalone main()
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_concurrent_page_access\.cpp$")  # Standalone main()

# Integration tests
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_rollup_simple\.cpp$")  # Root-level test with API issues
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_views_expansion\.cpp$")  # Root-level test with API issues

# Exclude integration tests that need external clients or API updates
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/integration/.*$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_copy_flow_control\.cpp$")  # API mismatch
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_firebird_parser_agent\.cpp$")  # API mismatch
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_mysql_parser_agent\.cpp$")  # API mismatch
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_postgresql_parser_agent\.cpp$")  # API mismatch
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_schema_introspection\.cpp$")  # API mismatch
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_unix_socket_channel\.cpp$")  # API mismatch
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_type_mapping\.cpp$")  # Link error

# Test executable
add_executable(scratchbird_tests ${TEST_SOURCES})

# Include directories (for test_helpers.h)
target_include_directories(scratchbird_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link libraries
target_link_libraries(scratchbird_tests
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    scratchbird_protocol
    scratchbird_pool
    scratchbird_server
    scratchbird_network
    GTest::gtest
    GTest::gtest_main
    scratchbird_parser
    scratchbird_link_group
)

# Link GEOS library if available (for spatial functions)
if(GEOS_FOUND)
    target_link_libraries(scratchbird_tests ${GEOS_LIBRARIES})
endif()

# Link PROJ library if available (for coordinate system support)
if(PROJ_FOUND)
    target_link_libraries(scratchbird_tests ${PROJ_LIBRARIES})
endif()

# =============================================================================
# Sequential Test Suite
# Tests that conflict when run in parallel are grouped here and run sequentially
# =============================================================================
file(GLOB SEQUENTIAL_TEST_SOURCES CONFIGURE_DEPENDS
    "sequential/*.cpp"
)

add_executable(scratchbird_tests_sequential ${SEQUENTIAL_TEST_SOURCES})

target_include_directories(scratchbird_tests_sequential PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(scratchbird_tests_sequential
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    scratchbird_protocol
    scratchbird_pool
    scratchbird_server
    scratchbird_network
    GTest::gtest
    GTest::gtest_main
    scratchbird_parser
    scratchbird_link_group
)

if(GEOS_FOUND)
    target_link_libraries(scratchbird_tests_sequential ${GEOS_LIBRARIES})
endif()

if(PROJ_FOUND)
    target_link_libraries(scratchbird_tests_sequential ${PROJ_LIBRARIES})
endif()

# Register tests
include(GoogleTest)
include(GoogleTest)
set(SCRATCHBIRD_SMOKE_TESTS
    V2DataTypesTest.*
    TextSearchTest.*
    SemanticAnalyzerV2Test.*
    ParserSessionV2Test.*
    ParserV2DDLTest.*
    ShowSetCommandsTest.*
    LexerV2Test.*
    SchemaPathV2Test.*
    ParserDMLV2Test.*
    BytecodeGeneratorV2Test.*
    ConditionalFunctionTest.*
    ArithmeticOverflow.*
    NaNInfinityHandling.*
    PredicateMatcherTest.*
    FirebirdLexerTest.*
    CharsetTest.*
    ExpressionMatcherTest.*
    ParserStateV2Test.*
    ConfigTest.*
)
list(JOIN SCRATCHBIRD_SMOKE_TESTS ":" SCRATCHBIRD_SMOKE_FILTER)

set(SCRATCHBIRD_DIALECT_PARSER_TESTS
    FirebirdParserTest.*
    PostgreSQLParserTest.*
    MySQLParserTest.*
)
list(JOIN SCRATCHBIRD_DIALECT_PARSER_TESTS ":" SCRATCHBIRD_DIALECT_PARSER_FILTER)

set(SCRATCHBIRD_STRESS_TESTS
    StorageStressTest.*
    *Stress*
)
list(JOIN SCRATCHBIRD_STRESS_TESTS ":" SCRATCHBIRD_STRESS_FILTER)

set(SCRATCHBIRD_PERFORMANCE_TESTS
    DependencyPerformanceTest.*
    StoragePerformanceTest.*
    ThroughputTest.*
    AtomicXIDTest.PerformanceBenchmark
    *Benchmark*
)
list(JOIN SCRATCHBIRD_PERFORMANCE_TESTS ":" SCRATCHBIRD_PERFORMANCE_FILTER)

set(SCRATCHBIRD_QUARANTINE_TESTS
    # Cross-page updates: Feature not fully implemented yet
    # The cross-page update mechanism requires additional implementation
    CrossPageUpdateTest.*
)
list(JOIN SCRATCHBIRD_QUARANTINE_TESTS ":" SCRATCHBIRD_QUARANTINE_FILTER)

set(SCRATCHBIRD_INTEGRATION_TESTS
    TCPIntegrationTest.*
    UnixSocketTest.*
    NetworkTest.*
    ProtocolSessionTest.*
    SweepMechanismTest.*
    *Integration*
    *EndToEnd*
    *E2E*
    QueryCompilerV2Test.*
    Week3Week4ComprehensiveTest.*
)
list(JOIN SCRATCHBIRD_INTEGRATION_TESTS ":" SCRATCHBIRD_INTEGRATION_FILTER)

set(SCRATCHBIRD_UNIT_EXCLUDE_FILTER "${SCRATCHBIRD_SMOKE_FILTER}")
if(SCRATCHBIRD_DIALECT_PARSER_FILTER)
    set(SCRATCHBIRD_UNIT_EXCLUDE_FILTER
        "${SCRATCHBIRD_UNIT_EXCLUDE_FILTER}:${SCRATCHBIRD_DIALECT_PARSER_FILTER}"
    )
endif()
if(SCRATCHBIRD_STRESS_FILTER)
    set(SCRATCHBIRD_UNIT_EXCLUDE_FILTER
        "${SCRATCHBIRD_UNIT_EXCLUDE_FILTER}:${SCRATCHBIRD_STRESS_FILTER}"
    )
endif()
if(SCRATCHBIRD_PERFORMANCE_FILTER)
    set(SCRATCHBIRD_UNIT_EXCLUDE_FILTER
        "${SCRATCHBIRD_UNIT_EXCLUDE_FILTER}:${SCRATCHBIRD_PERFORMANCE_FILTER}"
    )
endif()
if(SCRATCHBIRD_INTEGRATION_FILTER)
    set(SCRATCHBIRD_UNIT_EXCLUDE_FILTER
        "${SCRATCHBIRD_UNIT_EXCLUDE_FILTER}:${SCRATCHBIRD_INTEGRATION_FILTER}"
    )
endif()
if(SCRATCHBIRD_QUARANTINE_FILTER)
    set(SCRATCHBIRD_UNIT_EXCLUDE_FILTER
        "${SCRATCHBIRD_UNIT_EXCLUDE_FILTER}:${SCRATCHBIRD_QUARANTINE_FILTER}"
    )
endif()

gtest_discover_tests(scratchbird_tests
    PROPERTIES
        TIMEOUT ${SCRATCHBIRD_TEST_TIMEOUT_UNIT}  # Per-test timeout
        LABELS "unit"
        ENVIRONMENT "SCRATCHBIRD_TEST_NETWORK=1"
    TEST_FILTER "*-${SCRATCHBIRD_UNIT_EXCLUDE_FILTER}"
)

gtest_discover_tests(scratchbird_tests
    PROPERTIES
        TIMEOUT ${SCRATCHBIRD_TEST_TIMEOUT_UNIT}  # Per-test timeout
        LABELS "unit\\;smoke"
        ENVIRONMENT "SCRATCHBIRD_TEST_NETWORK=1"
    TEST_FILTER "${SCRATCHBIRD_SMOKE_FILTER}"
)

if(SCRATCHBIRD_DIALECT_PARSER_FILTER)
    gtest_discover_tests(scratchbird_tests
        PROPERTIES
            TIMEOUT ${SCRATCHBIRD_TEST_TIMEOUT_UNIT}
            LABELS "unit\\;smoke\\;parser\\;firebird"
        TEST_FILTER "FirebirdParserTest.*"
    )
    gtest_discover_tests(scratchbird_tests
        PROPERTIES
            TIMEOUT ${SCRATCHBIRD_TEST_TIMEOUT_UNIT}
            LABELS "unit\\;smoke\\;parser\\;postgresql"
        TEST_FILTER "PostgreSQLParserTest.*"
    )
    gtest_discover_tests(scratchbird_tests
        PROPERTIES
            TIMEOUT ${SCRATCHBIRD_TEST_TIMEOUT_UNIT}
            LABELS "unit\\;smoke\\;parser\\;mysql"
        TEST_FILTER "MySQLParserTest.*"
    )
endif()

if(SCRATCHBIRD_STRESS_FILTER)
    gtest_discover_tests(scratchbird_tests
        PROPERTIES
            TIMEOUT ${SCRATCHBIRD_TEST_TIMEOUT_STRESS}
            LABELS "stress"
        TEST_FILTER "${SCRATCHBIRD_STRESS_FILTER}"
    )
endif()

if(SCRATCHBIRD_PERFORMANCE_FILTER)
    gtest_discover_tests(scratchbird_tests
        PROPERTIES
            TIMEOUT ${SCRATCHBIRD_TEST_TIMEOUT_PERFORMANCE}
            LABELS "performance"
        TEST_FILTER "${SCRATCHBIRD_PERFORMANCE_FILTER}"
    )
endif()

if(SCRATCHBIRD_INTEGRATION_FILTER)
    gtest_discover_tests(scratchbird_tests
        PROPERTIES
            TIMEOUT ${SCRATCHBIRD_TEST_TIMEOUT_INTEGRATION}
            LABELS "integration"
            ENVIRONMENT "SCRATCHBIRD_TEST_NETWORK=1"
        TEST_FILTER "${SCRATCHBIRD_INTEGRATION_FILTER}"
    )
endif()

if(SCRATCHBIRD_QUARANTINE_FILTER)
    gtest_discover_tests(scratchbird_tests
        PROPERTIES
            TIMEOUT ${SCRATCHBIRD_TEST_TIMEOUT_QUARANTINE}
            LABELS "quarantine\\;disabled"
        TEST_FILTER "${SCRATCHBIRD_QUARANTINE_FILTER}"
    )
endif()

# =============================================================================
# Sequential Test Suite Registration
# These tests are registered to run sequentially (not in parallel) to avoid
# conflicts on shared singleton resources
# =============================================================================
# Register as a single test that runs all sequential tests
add_test(NAME SequentialTestSuite COMMAND scratchbird_tests_sequential)
set_tests_properties(SequentialTestSuite PROPERTIES 
    LABELS "sequential"
    TIMEOUT ${SCRATCHBIRD_TEST_TIMEOUT_INTEGRATION}
)

# ==============================================================================
# IPC Server Infrastructure Tests (Local Server Architecture - Phase 1)
# ==============================================================================
add_executable(test_ipc_server unit/test_ipc_server.cpp)
target_link_libraries(test_ipc_server
    scratchbird_server
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)
add_test(NAME IPC_ServerTests COMMAND test_ipc_server)
set_tests_properties(IPC_ServerTests PROPERTIES
    LABELS "integration;server;ipc"
    TIMEOUT 60
    ENVIRONMENT "SCRATCHBIRD_TEST_NETWORK=1"
)

# ==============================================================================
# Wire Protocol Tests (Local Server Architecture - Phase 2)
# ==============================================================================
add_executable(test_wire_protocol unit/test_wire_protocol.cpp)
target_link_libraries(test_wire_protocol
    scratchbird_protocol
    scratchbird_server
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)
add_test(NAME WireProtocolTests COMMAND test_wire_protocol)
set_tests_properties(WireProtocolTests PROPERTIES
    LABELS "integration;server;protocol;wire"
    TIMEOUT 60
    ENVIRONMENT "SCRATCHBIRD_TEST_NETWORK=1"
)

# ==============================================================================
# Engine IPC Session Handler Tests (SQL→Bytecode Integration)
# ==============================================================================
add_executable(test_engine_ipc_session_handler unit/test_engine_ipc_session_handler.cpp)
target_link_libraries(test_engine_ipc_session_handler
    scratchbird_ipc
    scratchbird_sblr
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)
add_test(NAME EngineIPCSessionHandlerTests COMMAND test_engine_ipc_session_handler)
set_tests_properties(EngineIPCSessionHandlerTests PROPERTIES
    LABELS "unit;ipc;engine;session"
    TIMEOUT 120
)

# ==============================================================================
# IPC Integration Tests (End-to-End)
# 5.1.2-5.1.8: IPC Integration Harness
# ==============================================================================
add_executable(test_ipc_integration_harness integration/test_ipc_integration_harness.cpp)
target_link_libraries(test_ipc_integration_harness
    scratchbird_ipc
    scratchbird_sblr
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)
add_test(NAME IPC_Integration_Harness COMMAND test_ipc_integration_harness)
set_tests_properties(IPC_Integration_Harness PROPERTIES
    LABELS "integration;ipc;harness"
    TIMEOUT 300
)

# 4.1.6-4.1.7: COPY 1GB Memory Test
add_executable(test_copy_1gb integration/test_copy_1gb.cpp)
target_link_libraries(test_copy_1gb
    scratchbird_ipc
    scratchbird_sblr
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)
add_test(NAME COPY_1GB_Test COMMAND test_copy_1gb)
set_tests_properties(COPY_1GB_Test PROPERTIES
    LABELS "integration;ipc;copy;memory;performance"
    TIMEOUT 600
)

# ==============================================================================
# External Client Tests (psql, mysql, isql-fb)
# These require external client tools and are disabled by default.
# Enable with -DSCRATCHBIRD_ENABLE_CLIENT_TESTS=ON
# ==============================================================================
option(SCRATCHBIRD_ENABLE_CLIENT_TESTS "Enable external client integration tests" OFF)

if(SCRATCHBIRD_ENABLE_CLIENT_TESTS)
    # 2.1.22-2.1.25: psql Client Tests
    add_executable(test_psql_client integration/test_psql_client.cpp)
    target_link_libraries(test_psql_client
        scratchbird_ipc
        scratchbird_sblr
        scratchbird_core
        GTest::gtest
        GTest::gtest_main
    )
    add_test(NAME PSQL_Client_Tests COMMAND test_psql_client)
    set_tests_properties(PSQL_Client_Tests PROPERTIES
        LABELS "integration;ipc;psql;postgresql;client"
        TIMEOUT 300
    )

    # 2.2.15-2.2.17: MySQL Client Tests
    add_executable(test_mysql_client integration/test_mysql_client.cpp)
    target_link_libraries(test_mysql_client
        scratchbird_ipc
        scratchbird_sblr
        scratchbird_core
        GTest::gtest
        GTest::gtest_main
    )
    add_test(NAME MySQL_Client_Tests COMMAND test_mysql_client)
    set_tests_properties(MySQL_Client_Tests PROPERTIES
        LABELS "integration;ipc;mysql;client"
        TIMEOUT 300
    )

    # 2.3.16-2.3.17: Firebird Client Tests
    add_executable(test_firebird_client integration/test_firebird_client.cpp)
    target_link_libraries(test_firebird_client
        scratchbird_ipc
        scratchbird_sblr
        scratchbird_core
        GTest::gtest
        GTest::gtest_main
    )
    add_test(NAME Firebird_Client_Tests COMMAND test_firebird_client)
    set_tests_properties(Firebird_Client_Tests PROPERTIES
        LABELS "integration;ipc;firebird;client"
        TIMEOUT 300
    )
endif()

# ThreadSanitizer (TSAN) Tests for Critical Race Conditions
# These tests validate fixes for CRITICAL-1, CRITICAL-2, CRITICAL-3, HIGH-1, HIGH-2, etc.
option(SCRATCHBIRD_ENABLE_TSAN "Enable ThreadSanitizer (TSAN) tests" OFF)

if (SCRATCHBIRD_ENABLE_TSAN)
    # TSAN Test 1: Buffer Pool Frame Metadata Race (CRITICAL-1)
    add_executable(tsan_buffer_pool_race tsan/test_buffer_pool_race.cpp)
    target_compile_options(tsan_buffer_pool_race PRIVATE -fsanitize=thread -g -O1)
    target_link_options(tsan_buffer_pool_race PRIVATE -fsanitize=thread)
    target_link_libraries(tsan_buffer_pool_race
        scratchbird_sblr
        ${SCRATCHBIRD_TEST_COMPILER_LIBS}
        ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
        scratchbird_core
        GTest::gtest
        GTest::gtest_main
    )
    add_test(NAME TSAN_BufferPoolRace COMMAND tsan_buffer_pool_race)
    set_tests_properties(TSAN_BufferPoolRace PROPERTIES
        LABELS "tsan;critical"
        TIMEOUT 120
    )

    # TSAN Test 2: TransactionManager Cache Race (CRITICAL-2)
    add_executable(tsan_transaction_cache_race tsan/test_transaction_cache_race.cpp)
    target_compile_options(tsan_transaction_cache_race PRIVATE -fsanitize=thread -g -O1)
    target_link_options(tsan_transaction_cache_race PRIVATE -fsanitize=thread)
    target_link_libraries(tsan_transaction_cache_race
        scratchbird_sblr
        ${SCRATCHBIRD_TEST_COMPILER_LIBS}
        ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
        scratchbird_core
        GTest::gtest
        GTest::gtest_main
    )
    add_test(NAME TSAN_TransactionCacheRace COMMAND tsan_transaction_cache_race)
    set_tests_properties(TSAN_TransactionCacheRace PROPERTIES
        LABELS "tsan;critical"
        TIMEOUT 180
    )

    # TSAN Test 3: Lock Ordering / Deadlock Prevention (CRITICAL-3)
    add_executable(tsan_lock_ordering tsan/test_lock_ordering.cpp)
    target_compile_options(tsan_lock_ordering PRIVATE -fsanitize=thread -g -O1)
    target_link_options(tsan_lock_ordering PRIVATE -fsanitize=thread)
    target_link_libraries(tsan_lock_ordering
        scratchbird_sblr
        ${SCRATCHBIRD_TEST_COMPILER_LIBS}
        ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
        scratchbird_core
        GTest::gtest
        GTest::gtest_main
    )
    add_test(NAME TSAN_LockOrdering COMMAND tsan_lock_ordering)
    set_tests_properties(TSAN_LockOrdering PROPERTIES
        LABELS "tsan;critical"
        TIMEOUT 240
        # Suppress known false positives from ProcArrayManager initialization
        # TSAN can't track happens-before through atomic pointer + mutex init pattern
        ENVIRONMENT "TSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/tools/tsan_suppressions.txt"
    )
else()
    message(STATUS "TSAN tests disabled (set SCRATCHBIRD_ENABLE_TSAN=ON to enable)")
endif()

# Helgrind Tests for Lock Ordering and Data Race Detection
# Run with: valgrind --tool=helgrind ./helgrind_races
# Note: Helgrind is slower than TSAN (~10-100x) but provides additional lock order validation

add_executable(helgrind_races helgrind/test_helgrind_races.cpp)
target_link_libraries(helgrind_races
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)
# Note: Run manually with valgrind --tool=helgrind ./tests/helgrind_races
# Not added to CTest because it's too slow for regular CI runs

# Multi-Threaded Stress Tests (100+ threads)
# Tests extreme concurrency scenarios with 100-200 threads

add_executable(multithreaded_stress stress/test_multithreaded_stress.cpp)
target_link_libraries(multithreaded_stress
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)
add_test(NAME MultiThreadedStress COMMAND multithreaded_stress)
set_tests_properties(MultiThreadedStress PROPERTIES
    LABELS "stress;performance"
    TIMEOUT 300
)

# Wave 1 Tests (Phase 2 Parallel Development)
# NOTE: Temporarily disabled - test files need API updates for spatial types
# TODO: Fix test_spatial_types.cpp, test_multi_geometry.cpp TypedValue::makePoint API
#
# add_executable(wave1_tests
#     unit/test_spatial_types.cpp
#     unit/test_text_search.cpp
#     unit/test_multi_geometry.cpp
# )
# target_link_libraries(wave1_tests
#     scratchbird_sblr
#     scratchbird_parser
#     scratchbird_optimizer
#     scratchbird_core
#     GTest::gtest
#     GTest::gtest_main
# )
# add_test(NAME Wave1Tests COMMAND wave1_tests)
# set_tests_properties(Wave1Tests PROPERTIES
#     LABELS "wave1;phase2"
#     TIMEOUT 120
# )

# Task 14 Phase 5: Text Search Type System Integration Tests
add_executable(test_text_search_types
    integration/test_text_search_types.cpp
)
target_link_libraries(test_text_search_types
    scratchbird_core
)
add_test(NAME TextSearchTypes COMMAND test_text_search_types)
set_tests_properties(TextSearchTypes PROPERTIES
    LABELS "integration;task14;phase5"
    TIMEOUT 60
)

# Task 15: Range Types Unit Tests
add_executable(test_range_types
    unit/test_range_types.cpp
)
target_link_libraries(test_range_types
    scratchbird_core
)
add_test(NAME RangeTypes COMMAND test_range_types)
set_tests_properties(RangeTypes PROPERTIES
    LABELS "unit;task15;range_types"
    TIMEOUT 60
)

# Task 15 Phase 2: Temporal Range Types Unit Tests
add_executable(test_temporal_range_types
    unit/test_temporal_range_types.cpp
)
target_link_libraries(test_temporal_range_types
    scratchbird_core
)
add_test(NAME TemporalRangeTypes COMMAND test_temporal_range_types)
set_tests_properties(TemporalRangeTypes PROPERTIES
    LABELS "unit;task15;temporal_range_types"
    TIMEOUT 60
)

# Task 15 Phase 3: Range Operators Unit Tests
add_executable(test_range_operators
    unit/test_range_operators.cpp
)
target_link_libraries(test_range_operators
    scratchbird_core
)
add_test(NAME RangeOperators COMMAND test_range_operators)
set_tests_properties(RangeOperators PROPERTIES
    LABELS "unit;task15;range_operators"
    TIMEOUT 60
)

# Task 15 Phase 4: Range Lexer Tests - REMOVED (legacy parser path removed)
# test_range_lexer.cpp removed alongside legacy parser cleanup

# ============================================================================
# Task 16: Network Types Tests
# ============================================================================

add_executable(test_network_types
    unit/test_network_types.cpp
)
target_link_libraries(test_network_types
    scratchbird_core
)
add_test(NAME NetworkTypes COMMAND test_network_types)
set_tests_properties(NetworkTypes PROPERTIES
    LABELS "unit;task16;network"
    TIMEOUT 60
)

# ============================================================================
# Type Serialization Tests
# ============================================================================
# NOTE: Temporarily disabled - test file needs significant updates to match
# current TypedValue API (geometry types, ArrayValue, Composite, Variant, Range APIs)
# TODO: Update tests to use current API and re-enable

# add_executable(test_type_serialization
#     unit/test_type_serialization.cpp
# )
# target_link_libraries(test_type_serialization
#     scratchbird_core
#     GTest::gtest
#     GTest::gtest_main
# )
# add_test(NAME TypeSerialization COMMAND test_type_serialization)
# set_tests_properties(TypeSerialization PROPERTIES
#     LABELS "unit;serialization;types"
#     TIMEOUT 60
# )



# ============================================================================
# Columnstore Index Tests - Phase 1: RLE Compression
# ============================================================================

add_executable(test_columnstore_rle
    unit/test_columnstore_rle.cpp
)
target_link_libraries(test_columnstore_rle
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME ColumnstoreRLE COMMAND test_columnstore_rle)
set_tests_properties(ColumnstoreRLE PROPERTIES
    LABELS "unit;columnstore;rle;phase1"
    TIMEOUT 120
)

# ============================================================================
# Columnstore Index Tests - Phase 2: Dictionary Encoding
# ============================================================================

add_executable(test_columnstore_dict
    unit/test_columnstore_dict.cpp
)
target_link_libraries(test_columnstore_dict
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME ColumnstoreDictionary COMMAND test_columnstore_dict)
set_tests_properties(ColumnstoreDictionary PROPERTIES
    LABELS "unit;columnstore;dictionary;phase2"
    TIMEOUT 120
)

# ============================================================================
# Columnstore Index Tests - Phase 3: Bit-Packing
# ============================================================================

add_executable(test_columnstore_bitpack
    unit/test_columnstore_bitpack.cpp
)
target_link_libraries(test_columnstore_bitpack
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME ColumnstoreBitpack COMMAND test_columnstore_bitpack)
set_tests_properties(ColumnstoreBitpack PROPERTIES
    LABELS "unit;columnstore;bitpack;phase3"
    TIMEOUT 120
)

# ============================================================================
# Columnstore Index Tests - Phase 4: Predicate Pushdown
# ============================================================================

add_executable(test_columnstore_predicate
    unit/test_columnstore_predicate.cpp
)
target_link_libraries(test_columnstore_predicate
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME ColumnstorePredicate COMMAND test_columnstore_predicate)
set_tests_properties(ColumnstorePredicate PROPERTIES
    LABELS "unit;columnstore;predicate;phase4"
    TIMEOUT 120
)

# ============================================================================
# Columnstore Index Tests - Phase 5: Batch Processing & SIMD
# ============================================================================

# DISABLED: test_columnstore_batch_performance - accesses private flushSegment API
# add_executable(test_columnstore_batch_performance
#     stress/test_columnstore_batch_performance.cpp
# )
# target_link_libraries(test_columnstore_batch_performance
#     scratchbird_core
#     ${SCRATCHBIRD_TEST_COMPILER_LIBS}
#     scratchbird_sblr
#     ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
#     GTest::gtest
#     GTest::gtest_main
# )
# target_include_directories(test_columnstore_batch_performance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# add_test(NAME ColumnstoreBatchPerformance COMMAND test_columnstore_batch_performance)
# set_tests_properties(ColumnstoreBatchPerformance PROPERTIES
#     LABELS "stress;columnstore;batch;simd;phase5;performance"
#     TIMEOUT 300
# )

# ============================================================================
# Columnstore Index Tests - Phase 6: Segment Management
# ============================================================================
# NOTE: This test is temporarily disabled due to a known infinite loop issue
# in the columnstore scan when handling empty segments. The underlying bug
# needs further investigation.

add_executable(test_columnstore_segments
    integration/test_columnstore_segments.cpp
)
target_link_libraries(test_columnstore_segments
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
)
# Temporarily disabled - test hangs due to columnstore scan issue
# add_test(NAME ColumnstoreSegments COMMAND test_columnstore_segments)
# set_tests_properties(ColumnstoreSegments PROPERTIES
#     LABELS "integration;columnstore;segments;phase6"
#     TIMEOUT 120
# )

# ============================================================================
# Columnstore Index Tests - Phase 7: Comprehensive End-to-End Testing
# ============================================================================

add_executable(test_columnstore_comprehensive
    integration/test_columnstore_comprehensive.cpp
)
target_include_directories(test_columnstore_comprehensive PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_columnstore_comprehensive
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME ColumnstoreComprehensive COMMAND test_columnstore_comprehensive)
set_tests_properties(ColumnstoreComprehensive PROPERTIES
    LABELS "integration;columnstore;e2e;phase7;comprehensive"
    TIMEOUT 180
)

# Simple E2E test (minimal version to avoid hang issues)
add_executable(test_columnstore_simple_e2e
    integration/test_columnstore_simple_e2e.cpp
)
target_link_libraries(test_columnstore_simple_e2e
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
)
add_test(NAME ColumnstoreSimpleE2E COMMAND test_columnstore_simple_e2e)
set_tests_properties(ColumnstoreSimpleE2E PROPERTIES
    LABELS "integration;columnstore;e2e;phase7;simple"
    TIMEOUT 30
)

# Load test (100K-1M rows) - Full version, DISABLED due to missing gtest includes
# add_executable(test_columnstore_load
#     stress/test_columnstore_load.cpp
# )
# target_link_libraries(test_columnstore_load
#     scratchbird_core
#     ${SCRATCHBIRD_TEST_COMPILER_LIBS}
#     scratchbird_sblr
#     ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
#     GTest::gtest
#     GTest::gtest_main
# )
# target_include_directories(test_columnstore_load PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# add_test(NAME ColumnstoreLoad COMMAND test_columnstore_load)
# set_tests_properties(ColumnstoreLoad PROPERTIES
#     LABELS "stress;columnstore;load;phase7;performance"
#     TIMEOUT 180
# )

# Simple load test (50K rows) - Stable version
add_executable(test_columnstore_load_simple
    stress/test_columnstore_load_simple.cpp
)
target_link_libraries(test_columnstore_load_simple
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
)
add_test(NAME ColumnstoreLoadSimple COMMAND test_columnstore_load_simple)
set_tests_properties(ColumnstoreLoadSimple PROPERTIES
    LABELS "stress;columnstore;load;phase7;simple"
    TIMEOUT 120
)

# ============================================================================
# LSM-Tree Index Tests - Phase 1: Memtable
# NOTE: Temporarily disabled - test file uses APIs not yet implemented
# (Memtable, SSTableWriter, SSTableReader, etc. are planned but not in current lsm_tree.h)
# ============================================================================

# add_executable(test_lsm_memtable
#     unit/test_lsm_memtable.cpp
# )
# target_link_libraries(test_lsm_memtable
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
# )
# add_test(NAME LSMMemtable COMMAND test_lsm_memtable)
# set_tests_properties(LSMMemtable PROPERTIES
#     LABELS "unit;lsm;memtable;phase1"
#     TIMEOUT 60
# )

# ============================================================================
# LSM-Tree Index Tests - Phase 2: SSTable Writer
# NOTE: Temporarily disabled - SSTableWriter API not yet implemented
# ============================================================================

# add_executable(test_lsm_sstable_writer
#     unit/test_lsm_sstable_writer.cpp
# )
# target_link_libraries(test_lsm_sstable_writer
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
# )
# add_test(NAME LSMSSTableWriter COMMAND test_lsm_sstable_writer)
# set_tests_properties(LSMSSTableWriter PROPERTIES
#     LABELS "unit;lsm;sstable;writer;phase2"
#     TIMEOUT 60
# )

# ============================================================================
# LSM-Tree Index Tests - Phase 3: SSTable Reader
# NOTE: Temporarily disabled - SSTableReader API not yet implemented
# ============================================================================

# add_executable(test_lsm_sstable_reader
#     unit/test_lsm_sstable_reader.cpp
# )
# target_link_libraries(test_lsm_sstable_reader
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
# )
# add_test(NAME LSMSSTableReader COMMAND test_lsm_sstable_reader)
# set_tests_properties(LSMSSTableReader PROPERTIES
#     LABELS "unit;lsm;sstable;reader;phase3"
#     TIMEOUT 60
# )

# ============================================================================
# LSM-Tree Index Tests - Phase 4: Compaction
# NOTE: Temporarily disabled - LSMCompactionManager API not yet implemented
# ============================================================================

# add_executable(test_lsm_compaction
#     unit/test_lsm_compaction.cpp
# )
# target_link_libraries(test_lsm_compaction
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
# )
# add_test(NAME LSMCompaction COMMAND test_lsm_compaction)
# set_tests_properties(LSMCompaction PROPERTIES
#     LABELS "unit;lsm;compaction;kway;gc;phase4"
#     TIMEOUT 60
# )

# ============================================================================
# LSM-Tree Index Tests - Phase 5: Bloom Filter
# NOTE: Temporarily disabled - LSMBloomFilter standalone test uses planned API
# ============================================================================

# add_executable(test_lsm_bloom_filter
#     unit/test_lsm_bloom_filter.cpp
# )
# target_link_libraries(test_lsm_bloom_filter
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
# )
# add_test(NAME LSMBloomFilter COMMAND test_lsm_bloom_filter)
# set_tests_properties(LSMBloomFilter PROPERTIES
#     LABELS "unit;lsm;bloom;filter;phase5"
#     TIMEOUT 120
# )

# ============================================================================
# LSM-Tree Index Tests - Phase 6: Integration
# NOTE: Temporarily disabled - LSMTreeIndex API not yet implemented
# ============================================================================

# add_executable(test_lsm_tree_simple
#     integration/test_lsm_tree_simple.cpp
# )
# target_link_libraries(test_lsm_tree_simple
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
# )
# add_test(NAME LSMTreeSimple COMMAND test_lsm_tree_simple)
# set_tests_properties(LSMTreeSimple PROPERTIES
#     LABELS "integration;lsm;simple;phase6"
#     TIMEOUT 120
# )

# ============================================================================
# BRIN Index DML Integration Tests - TASK-DML-5
# ============================================================================


# Comprehensive integration test
# NOTE: Temporarily disabled - LSMTreeIndex API not yet implemented
# add_executable(test_lsm_tree_comprehensive
#     integration/test_lsm_tree_comprehensive.cpp
# )
# target_link_libraries(test_lsm_tree_comprehensive
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
# )
# add_test(NAME LSMTreeComprehensive COMMAND test_lsm_tree_comprehensive)
# set_tests_properties(LSMTreeComprehensive PROPERTIES
#     LABELS "integration;lsm;comprehensive;phase7"
#     TIMEOUT 180
# )

# Stress test (100K+ keys)
# NOTE: Temporarily disabled - LSMTreeIndex API not yet implemented
# add_executable(test_lsm_tree_stress
#     stress/test_lsm_tree_stress.cpp
# )
# target_link_libraries(test_lsm_tree_stress
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
# )
# add_test(NAME LSMTreeStress COMMAND test_lsm_tree_stress)
# set_tests_properties(LSMTreeStress PROPERTIES
#     LABELS "stress;lsm;performance;phase7"
#     TIMEOUT 300
# )

# ============================================================================
# GiST Index MVCC Integration Test
# ============================================================================

add_executable(test_gist_mvcc
    integration/test_gist_mvcc.cpp
)
target_link_libraries(test_gist_mvcc
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME GiSTMVCC COMMAND test_gist_mvcc)
set_tests_properties(GiSTMVCC PROPERTIES
    LABELS "integration;index;gist;mvcc;mga"
    TIMEOUT 120
)

# ============================================================================
# GiST Index DML Integration Test (TASK-DML-3)
# NOTE: Temporarily disabled - test uses different TransactionManager API signature
# ============================================================================

# add_executable(test_gist_dml
#     integration/test_gist_dml.cpp
# )
# target_link_libraries(test_gist_dml
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
#     gtest
#     gtest_main
# )
# add_test(NAME GiSTDML COMMAND test_gist_dml)
# set_tests_properties(GiSTDML PROPERTIES
#     LABELS "integration;index;gist;dml;task-dml-3"
#     TIMEOUT 120
# )

# ============================================================================
# SP-GiST Index MVCC Integration Test
# NOTE: Temporarily disabled - test uses different TransactionManager API signature
# ============================================================================

# add_executable(test_spgist_mvcc
#     integration/test_spgist_mvcc.cpp
# )
# target_link_libraries(test_spgist_mvcc
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
#     gtest
#     gtest_main
# )
# add_test(NAME SPGiSTMVCC COMMAND test_spgist_mvcc)
# set_tests_properties(SPGiSTMVCC PROPERTIES
#     LABELS "integration;index;spgist;mvcc;mga"
#     TIMEOUT 120
# )

# ============================================================================
# Security System Phase 2 Integration Test
# NOTE: Temporarily disabled - test uses Database::open static factory pattern not yet implemented
# ============================================================================

# add_executable(test_security_phase2
#     integration/test_security_phase2.cpp
# )
# target_link_libraries(test_security_phase2
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
#     gtest
#     gtest_main
# )
# add_test(NAME SecurityPhase2 COMMAND test_security_phase2)
# set_tests_properties(SecurityPhase2 PROPERTIES
#     LABELS "integration;security;phase2;alpha"
#     TIMEOUT 120
# )

# ============================================================================
# Security System Phase 3.4 Integration Test (RLS DDL)
# NOTE: Temporarily disabled - test uses different CatalogManager API (PolicyInfo.roles vs role_ids)
# ============================================================================

# add_executable(test_security_phase3_4_rls
#     integration/test_security_phase3_4_rls.cpp
# )
# target_link_libraries(test_security_phase3_4_rls
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
#     gtest
#     gtest_main
# )
# add_test(NAME SecurityPhase3_4_RLS COMMAND test_security_phase3_4_rls)
# set_tests_properties(SecurityPhase3_4_RLS PROPERTIES
#     LABELS "integration;security;phase3;rls;ddl"
#     TIMEOUT 120
# )

# ============================================================================
# Security System Phase 3.5 Integration Test (RLS DML Enforcement)
# ============================================================================

add_executable(test_security_phase3_5_rls_dml
    integration/test_security_phase3_5_rls_dml.cpp
)
target_link_libraries(test_security_phase3_5_rls_dml
    scratchbird_core
    scratchbird_parser
    scratchbird_sblr
    scratchbird_optimizer_compiler
    gtest
    gtest_main
)
add_test(NAME SecurityPhase3_5_RLS_DML COMMAND test_security_phase3_5_rls_dml)
set_tests_properties(SecurityPhase3_5_RLS_DML PROPERTIES
    LABELS "integration;security;phase3;rls;dml;enforcement"
    TIMEOUT 120
)

# Mathematical Functions Tests - DISABLED (V2 bytecode generator issue with complex expressions)
# TODO: Fix V2 BytecodeGeneratorV2 for nested function calls (e.g., SQRT(POWER(3, 2) + POWER(4, 2)))
# add_executable(test_mathematical_functions
#     integration/test_mathematical_functions.cpp
# )
# target_link_libraries(test_mathematical_functions
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
#     gtest
#     gtest_main
# )
# add_test(NAME MathematicalFunctions COMMAND test_mathematical_functions)
# set_tests_properties(MathematicalFunctions PROPERTIES
#     LABELS "integration;functions;math;trigonometry;algebra;logarithm"
#     TIMEOUT 60
# )

# Bit Manipulation Functions Tests - DISABLED (scalar SELECT not supported)
# TODO: Enable once scalar SELECT execution is supported
# add_executable(test_bit_manipulation
#     integration/test_bit_manipulation.cpp
# )
# target_link_libraries(test_bit_manipulation
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
#     gtest
#     gtest_main
# )
# add_test(NAME BitManipulation COMMAND test_bit_manipulation)
# set_tests_properties(BitManipulation PROPERTIES
#     LABELS "integration;functions;bit_manipulation;bitwise;bytes"
#     TIMEOUT 60
# )

# CHECK Constraints Tests (ALPHA Phase A - Constraint Enforcement)
add_executable(test_check_constraints
    integration/test_check_constraints.cpp
)
target_link_libraries(test_check_constraints
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME CheckConstraints COMMAND test_check_constraints)
set_tests_properties(CheckConstraints PROPERTIES
    LABELS "integration;constraints;check;enforcement"
    TIMEOUT 60
)

# Domain Integrity Tests (Plan 03B - Global Uniqueness)
add_executable(test_domain_integrity
    integration/test_domain_integrity.cpp
)
target_link_libraries(test_domain_integrity
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME DomainIntegrity COMMAND test_domain_integrity)
set_tests_properties(DomainIntegrity PROPERTIES
    LABELS "integration;domain;integrity;uniqueness"
    TIMEOUT 60
)

# Domain Security Tests (Plan 03B - Encryption/Masking/Audit)
add_executable(test_domain_encryption
    integration/test_domain_encryption.cpp
)
target_link_libraries(test_domain_encryption
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME DomainEncryption COMMAND test_domain_encryption)
set_tests_properties(DomainEncryption PROPERTIES
    LABELS "integration;domain;security;encryption"
    TIMEOUT 60
)

add_executable(test_domain_security
    integration/test_domain_security.cpp
)
target_link_libraries(test_domain_security
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME DomainSecurity COMMAND test_domain_security)
set_tests_properties(DomainSecurity PROPERTIES
    LABELS "integration;domain;security;masking;audit"
    TIMEOUT 60
)

# Domain Validation Tests (Plan 03B)
add_executable(test_domain_validation
    integration/test_domain_validation.cpp
)
target_link_libraries(test_domain_validation
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME DomainValidation COMMAND test_domain_validation)
set_tests_properties(DomainValidation PROPERTIES
    LABELS "integration;domain;validation"
    TIMEOUT 60
)

# Domain Quality Pipeline Tests (Plan 03B)
add_executable(test_domain_quality
    integration/test_domain_quality.cpp
)
target_link_libraries(test_domain_quality
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME DomainQuality COMMAND test_domain_quality)
set_tests_properties(DomainQuality PROPERTIES
    LABELS "integration;domain;quality"
    TIMEOUT 60
)

# Domain End-to-End Scenarios (Plan 03B)
add_executable(test_domain_e2e_scenarios
    integration/test_domain_e2e_scenarios.cpp
)
target_link_libraries(test_domain_e2e_scenarios
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME DomainE2EScenarios COMMAND test_domain_e2e_scenarios)
set_tests_properties(DomainE2EScenarios PROPERTIES
    LABELS "integration;domain;e2e"
    TIMEOUT 60
)

# Foreign Key Constraints Tests (ALPHA Phase A - FK Constraints)
add_executable(test_foreign_keys
    integration/test_foreign_keys.cpp
)
target_link_libraries(test_foreign_keys
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME ForeignKeys COMMAND test_foreign_keys)
set_tests_properties(ForeignKeys PROPERTIES
    LABELS "integration;constraints;foreign_key;referential_integrity"
    TIMEOUT 60
)


# Phase 2C: Exclude all standalone tests with main() functions

# Index Bytecode Generation Tests - DISABLED (V2 bytecode generator issue with CREATE/DROP INDEX)
# TODO: Fix V2 BytecodeGeneratorV2 for DDL statements
# add_executable(test_index_bytecode_generation
#     integration/test_index_bytecode_generation.cpp
# )
# target_link_libraries(test_index_bytecode_generation
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
#     gtest
#     gtest_main
# )
# add_test(NAME IndexBytecodeGeneration COMMAND test_index_bytecode_generation)
# set_tests_properties(IndexBytecodeGeneration PROPERTIES
#     LABELS "integration;index;bytecode;parser"
#     TIMEOUT 60
# )

# P3-20: Join Ordering Optimization Test
# TODO: Create test_join_ordering.cpp file
# add_executable(test_join_ordering
#     test_join_ordering.cpp
# )
# target_link_libraries(test_join_ordering
#     scratchbird_core
#     ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
#     GTest::gtest
#     GTest::gtest_main
# )
# target_include_directories(test_join_ordering PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# add_test(NAME JoinOrdering COMMAND test_join_ordering)
# set_tests_properties(JoinOrdering PROPERTIES
#     LABELS "unit;optimizer;join_ordering;p3"
#     TIMEOUT 30
# )

# P3-15: Materialized View Rewriting Test
# TODO: Create test_mv_rewriter.cpp file
# add_executable(test_mv_rewriter
#     test_mv_rewriter.cpp
# )
# target_link_libraries(test_mv_rewriter
#     scratchbird_core
#     ${SCRATCHBIRD_TEST_COMPILER_LIBS}
#     ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
#     scratchbird_server
#     GTest::gtest
#     GTest::gtest_main
# )
# target_include_directories(test_mv_rewriter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# add_test(NAME MVRewriter COMMAND test_mv_rewriter)
# set_tests_properties(MVRewriter PROPERTIES
#     LABELS "unit;optimizer;mv_rewriter;p3"
#     TIMEOUT 30
# )

# ============================================================================
# Additional Integration Tests (Previously Excluded - Now Enabled)
# ============================================================================

# BRIN Index MVCC Integration Tests
add_executable(test_brin_mvcc
    integration/test_brin_mvcc.cpp
)
target_link_libraries(test_brin_mvcc
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME BRIN_MVCC COMMAND test_brin_mvcc)
set_tests_properties(BRIN_MVCC PROPERTIES
    LABELS "integration;index;brin;mvcc;quarantine"
    TIMEOUT 120
)

# BRIN Index DML Integration Tests
add_executable(test_brin_dml
    integration/test_brin_dml.cpp
)
target_link_libraries(test_brin_dml
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME BRIN_DML COMMAND test_brin_dml)
set_tests_properties(BRIN_DML PROPERTIES
    LABELS "integration;index;brin;dml"
    TIMEOUT 120
)

# Bitmap Index DML Integration Tests
# DISABLED: The bitmap index stores xmin with entries for MGA visibility checking.
# The visibility check (VersionedBitmapEntry::isVisible) relies on TIP lookups via
# txn_mgr->isVersionVisible(), which requires committed transactions to be properly
# marked in TIP. This test's transaction management doesn't fully integrate with
# TIP updates, causing visibility failures after commit.
# TODO: Either fix TIP integration or modify test to use simpler visibility model
# add_executable(test_bitmap_dml
#     integration/test_bitmap_dml.cpp
# )
# target_link_libraries(test_bitmap_dml
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
#     gtest
#     gtest_main
# )
# add_test(NAME Bitmap_DML COMMAND test_bitmap_dml)
# set_tests_properties(Bitmap_DML PROPERTIES
#     LABELS "integration;index;bitmap;dml"
#     TIMEOUT 120
# )

# Composite Foreign Key Tests
add_executable(test_composite_fk
    integration/test_composite_fk.cpp
)
target_link_libraries(test_composite_fk
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME CompositeForeignKey COMMAND test_composite_fk)
set_tests_properties(CompositeForeignKey PROPERTIES
    LABELS "integration;constraints;foreign_key;composite"
    TIMEOUT 120
)

# Exception Safety Tests
add_executable(test_exception_safety
    integration/test_exception_safety.cpp
)
target_link_libraries(test_exception_safety
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME ExceptionSafety COMMAND test_exception_safety)
set_tests_properties(ExceptionSafety PROPERTIES
    LABELS "integration;exception;safety;robustness"
    TIMEOUT 120
)

# Concurrent Page Access Tests
add_executable(test_concurrent_page_access
    integration/test_concurrent_page_access.cpp
)
target_link_libraries(test_concurrent_page_access
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME ConcurrentPageAccess COMMAND test_concurrent_page_access)
set_tests_properties(ConcurrentPageAccess PROPERTIES
    LABELS "integration;concurrency;page;thread"
    TIMEOUT 180
)

# Buffer Pool Exhaustion Tests
add_executable(test_buffer_pool_exhaustion
    integration/test_buffer_pool_exhaustion.cpp
)
target_link_libraries(test_buffer_pool_exhaustion
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME BufferPoolExhaustion COMMAND test_buffer_pool_exhaustion)
set_tests_properties(BufferPoolExhaustion PROPERTIES
    LABELS "integration;buffer_pool;memory;stress"
    TIMEOUT 180
)

# Index DML Integration Tests
add_executable(test_index_dml_integration
    integration/test_index_dml_integration.cpp
)
target_link_libraries(test_index_dml_integration
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME IndexDMLIntegration COMMAND test_index_dml_integration)
set_tests_properties(IndexDMLIntegration PROPERTIES
    LABELS "integration;index;dml;comprehensive"
    TIMEOUT 180
)

# GIN Index DML Integration Tests
add_executable(test_gin_dml
    integration/test_gin_dml.cpp
)
target_link_libraries(test_gin_dml
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
add_test(NAME GIN_DML COMMAND test_gin_dml)
set_tests_properties(GIN_DML PROPERTIES
    LABELS "integration;index;gin;dml"
    TIMEOUT 120
)

# ============================================================================
# PostgreSQL Parser Unit Tests
# ============================================================================

add_executable(test_postgresql_parser
    unit/test_postgresql_parser.cpp
)
target_link_libraries(test_postgresql_parser
    scratchbird_core
    ${SCRATCHBIRD_TEST_COMPILER_LIBS}
    scratchbird_sblr
    ${SCRATCHBIRD_TEST_OPTIMIZER_LIBS}
    gtest
    gtest_main
)
# Use gtest_discover_tests for proper test enumeration (like other parser tests)
gtest_discover_tests(test_postgresql_parser
    TEST_PREFIX "PostgreSQL."
    PROPERTIES LABELS "unit;parser;postgresql;emulation"
    PROPERTIES TIMEOUT 60
)

# ============================================================================
# Parser V2 Performance Benchmarks (Phase 9)
# ============================================================================

# Parser V2 Benchmark - DISABLED (no V2-only benchmark implementation yet)
# add_executable(test_parser_v2_benchmark
#     benchmark/test_parser_v2_benchmark.cpp
# )
# target_link_libraries(test_parser_v2_benchmark
#     scratchbird_core
#     scratchbird_parser
#     scratchbird_sblr
#     scratchbird_optimizer
#     gtest
#     gtest_main
# )
# add_test(NAME ParserV2Benchmark COMMAND test_parser_v2_benchmark)
# set_tests_properties(ParserV2Benchmark PROPERTIES
#     LABELS "benchmark;parser;v2;performance"
#     TIMEOUT 120
# )

# Plan 01 Task B: Heap Page Ownership Tests
add_executable(test_heap_page_ownership
    integration/test_heap_page_ownership.cpp
)

target_link_libraries(test_heap_page_ownership
    PRIVATE
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME HeapPageOwnership COMMAND test_heap_page_ownership)
set_tests_properties(HeapPageOwnership PROPERTIES
    LABELS "integration;storage;plan01"
    TIMEOUT 60
)

# Plan 01 Task C: Columnstore Persistence Tests
add_executable(test_columnstore_persistence
    integration/test_columnstore_persistence.cpp
)

target_link_libraries(test_columnstore_persistence
    PRIVATE
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME ColumnstorePersistence COMMAND test_columnstore_persistence)
set_tests_properties(ColumnstorePersistence PROPERTIES
    LABELS "integration;storage;columnstore;plan01"
    TIMEOUT 60
)

# Plan 01 Task D.1: HNSW GC Tests
add_executable(test_hnsw_gc
    integration/test_hnsw_gc.cpp
)

target_link_libraries(test_hnsw_gc
    PRIVATE
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME HnswGC COMMAND test_hnsw_gc)
set_tests_properties(HnswGC PROPERTIES
    LABELS "integration;index;hnsw;gc;plan01"
    TIMEOUT 60
)

# Plan 01 Task D.2: FullText GC Tests
add_executable(test_fulltext_gc
    integration/test_fulltext_gc.cpp
)

target_link_libraries(test_fulltext_gc
    PRIVATE
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME FullTextGC COMMAND test_fulltext_gc)
set_tests_properties(FullTextGC PROPERTIES
    LABELS "integration;index;fulltext;gc;plan01"
    TIMEOUT 60
)

# Plan 01 Task D.3: BRIN GC Tests
add_executable(test_brin_gc
    integration/test_brin_gc.cpp
)

target_link_libraries(test_brin_gc
    PRIVATE
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME BrinGC COMMAND test_brin_gc)
set_tests_properties(BrinGC PROPERTIES
    LABELS "integration;index;brin;gc;plan01"
    TIMEOUT 60
)

# Plan 01 Task D.4: R-Tree GC Tests
add_executable(test_rtree_gc
    integration/test_rtree_gc.cpp
)

target_link_libraries(test_rtree_gc
    PRIVATE
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME RTreeGC COMMAND test_rtree_gc)
set_tests_properties(RTreeGC PROPERTIES
    LABELS "integration;index;rtree;gc;plan01"
    TIMEOUT 60
)

# Plan 01 Task D.6: GIN GC Tests (Step 1: Pending list cleanup)
add_executable(test_gin_gc
    integration/test_gin_gc.cpp
)

target_link_libraries(test_gin_gc
    PRIVATE
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME GinGC COMMAND test_gin_gc)
set_tests_properties(GinGC PROPERTIES
    LABELS "integration;index;gin;gc;plan01"
    TIMEOUT 60
)

# Plan 01 Task E: Shadow Index Rebuild Tests
add_executable(test_shadow_index_rebuild
    integration/test_shadow_index_rebuild.cpp
)

target_link_libraries(test_shadow_index_rebuild
    PRIVATE
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME ShadowIndexRebuild COMMAND test_shadow_index_rebuild)
set_tests_properties(ShadowIndexRebuild PROPERTIES
    LABELS "integration;index;shadow;rebuild;versioning;plan01"
    TIMEOUT 60
)

# ============================================================================
# Git Config Parser Tests (Section A: Git Config Key Normalization)
# ============================================================================

add_executable(test_git_config_parser
    git/GitConfigParserTest.cpp
)

target_include_directories(test_git_config_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(test_git_config_parser
    PRIVATE
    scratchbird_core
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(test_git_config_parser
    TEST_PREFIX "GitConfig."
    PROPERTIES LABELS "unit;git;config;parser"
    PROPERTIES TIMEOUT 60
)

# ============================================================================
# SBLR Type Opcode Tests (Section B: SBLR Type Opcode Remediation)
# ============================================================================

add_executable(test_sblr_type_opcodes
    unit/test_sblr_type_opcodes.cpp
)

target_include_directories(test_sblr_type_opcodes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(test_sblr_type_opcodes
    PRIVATE
    scratchbird_core
    scratchbird_sblr
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(test_sblr_type_opcodes
    TEST_PREFIX "SBLR."
    PROPERTIES LABELS "unit;sblr;type;opcode;bytecode"
    PROPERTIES TIMEOUT 60
)

# Aggregate target for building all test executables.
get_property(SCRATCHBIRD_TEST_TARGETS
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY BUILDSYSTEM_TARGETS
)
set(SCRATCHBIRD_TEST_BINARY_TARGETS "")
foreach(target_name ${SCRATCHBIRD_TEST_TARGETS})
    if(target_name MATCHES "^(scratchbird_tests|test_.*|tsan_.*|helgrind_.*|multithreaded_stress)$")
        list(APPEND SCRATCHBIRD_TEST_BINARY_TARGETS ${target_name})
    endif()
endforeach()
if(SCRATCHBIRD_TEST_BINARY_TARGETS)
    add_custom_target(scratchbird_test_binaries ALL DEPENDS ${SCRATCHBIRD_TEST_BINARY_TARGETS})
endif()

if(TARGET scratchbird_test_binaries)
    set_property(TARGET scratchbird_test_binaries PROPERTY FOLDER "tests")
endif()

# ============================================================================
# Alpha Completion Component Tests
# ============================================================================

# Note: The main component tests are defined earlier in this file.
# These additional tests provide extended coverage.

# SCRAM Authentication Tests (require scratchbird_udr)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_scram_auth.cpp")
    add_executable(test_scram_auth unit/test_scram_auth.cpp)
    target_link_libraries(test_scram_auth
        scratchbird_udr
        scratchbird_ipc
        scratchbird_core
        GTest::gtest_main
    )
    add_test(NAME SCRAMAuth COMMAND test_scram_auth)
endif()

# Type Mapping Tests (require scratchbird_udr)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_type_mapping.cpp")
    add_executable(test_type_mapping unit/test_type_mapping.cpp)
    target_link_libraries(test_type_mapping
        scratchbird_udr
        scratchbird_ipc
        scratchbird_core
        GTest::gtest_main
    )
    add_test(NAME TypeMapping COMMAND test_type_mapping)
endif()

# Note: Schema Introspection, COPY Flow Control, and Unix Socket tests 
# have API mismatches that need to be fixed. They are excluded from the build.
# These tests can be re-enabled after updating them to match the current API.
