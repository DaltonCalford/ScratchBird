cmake_minimum_required(VERSION 3.20)
project(ScratchBird LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compile_commands.json for tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(SCRATCHBIRD_BUILD_TESTS "Build unit tests" ON)
option(SCRATCHBIRD_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

# Warnings
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  if(SCRATCHBIRD_WARNINGS_AS_ERRORS)
    add_compile_options(-Werror)
  endif()
endif()

# Include directories
include_directories(include)

# Find OpenSSL for authentication
find_package(OpenSSL REQUIRED)

# ScratchBird Engine Library
add_library(
  scratchbird_engine STATIC
  src/engine/alloc.cpp
  src/engine/btree_page.cpp
  src/engine/btree_v1.cpp
  src/engine/capi.cpp
  src/engine/catalog_bootstrap.cpp
  src/engine/catalog_manager.cpp
  src/engine/catalog_mem.cpp
  src/engine/config.cpp
  src/engine/engine.cpp
  src/engine/executor.cpp
  src/engine/expr.cpp
  src/engine/file.cpp
  src/engine/generators.cpp
  src/engine/header.cpp
  src/engine/heap.cpp
  src/engine/index_btree.cpp
  src/engine/index_build.cpp
  src/engine/index.cpp
  src/engine/index_online.cpp
  src/engine/lexer.cpp
  src/engine/monitoring.cpp
  src/engine/ods.cpp
  src/engine/pager.cpp
  src/engine/parser.cpp
  src/engine/parser_ddl.cpp
  src/engine/parser_dml.cpp
  src/engine/parser_expr.cpp
  src/engine/parser_psql.cpp
  src/engine/parser_select.cpp
  src/engine/parser_session.cpp
  src/engine/planner.cpp
  src/engine/psql_executor.cpp
  src/engine/psql_dev_tools.cpp
  src/engine/resolver.cpp
  src/engine/segment_monitor.cpp
  src/engine/stats.cpp
  src/engine/storage.cpp
  src/engine/system_oids.cpp
  src/engine/txn.cpp
  src/engine/types.cpp
  src/engine/uuid.cpp
  src/engine/wal.cpp
  src/engine/executor_nodes.cpp
  src/engine/query_planner.cpp
  src/engine/statistics.cpp
  src/engine/trigger_engine.cpp
  src/engine/wal_manager.cpp
  src/engine/multi_segment_manager.cpp
  src/engine/alter_table_manager.cpp
  src/engine/security_manager.cpp
  src/engine/serializable_isolation.cpp
  src/engine/index_family.cpp
  src/engine/index_hash.cpp
  src/engine/index_bitmap.cpp
  src/engine/index_gin.cpp
  src/engine/index_lsm.cpp
  src/engine/index_columnstore.cpp
  src/engine/index_rtree.cpp
  src/engine/index_ttl.cpp
  src/engine/fdw.cpp
  src/engine/fdw_csv.cpp
  src/engine/fdw_postgresql.cpp
  src/engine/fdw_json.cpp
  src/engine/database_link.cpp
  src/engine/fdw_security.cpp
  src/engine/fdw_error_handling.cpp
  src/engine/fdw_catalog.cpp
  src/engine/network_server.cpp
  src/engine/session.cpp
  src/engine/protocol_handler.cpp
  src/engine/scratchbird_protocol.cpp
  src/engine/firebird_protocol.cpp
  src/engine/firebird_protocol_handler.cpp
  src/engine/provider_dispatch.cpp
  src/engine/database_provider_simple.cpp
  src/engine/threading.cpp
  src/engine/embedded_provider.cpp
  src/engine/remote_provider.cpp
  src/engine/authentication.cpp
  src/engine/password_auth.cpp
  src/engine/trusted_auth.cpp
  src/engine/two_factor_auth.cpp
  src/engine/tls_server.cpp
  src/engine/connection_security.cpp
  src/engine/role_management.cpp
  src/engine/tcp_optimizer.cpp
  src/engine/connection_pool.cpp)

# Link OpenSSL to the engine library
target_link_libraries(scratchbird_engine OpenSSL::SSL OpenSSL::Crypto)

# Optional: libpq integration for PostgreSQL FDW
option(SCRATCHBIRD_WITH_LIBPQ "Enable libpq for PostgreSQL FDW" OFF)
if(SCRATCHBIRD_WITH_LIBPQ)
  find_library(LIBPQ_LIB pq)
  find_path(LIBPQ_INCLUDE libpq-fe.h)
  if(LIBPQ_LIB AND LIBPQ_INCLUDE)
    target_compile_definitions(scratchbird_engine PRIVATE SCRATCHBIRD_WITH_LIBPQ)
    target_include_directories(scratchbird_engine PRIVATE ${LIBPQ_INCLUDE})
    target_link_libraries(scratchbird_engine ${LIBPQ_LIB})
  else()
    message(WARNING "libpq not found; building without libpq integration")
  endif()
endif()

# Main ScratchBird executable
add_executable(scratchbird src/main.cpp src/scratchbird.cpp)

target_link_libraries(scratchbird scratchbird_engine)

# Database Space Monitor Utility
add_executable(dbspace src/dbspace.cpp)
target_link_libraries(dbspace scratchbird_engine)

# Database Integrity Check Utility
add_executable(dbcheck src/dbcheck.cpp)
target_link_libraries(dbcheck scratchbird_engine)

# Tests
if(SCRATCHBIRD_BUILD_TESTS)
  enable_testing()

  # Find GoogleTest for tests that use it
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(GTEST gtest)
    pkg_check_modules(GTEST_MAIN gtest_main)
  endif()

  # Fallback to manual detection if pkg-config fails
  if(NOT GTEST_FOUND)
    find_library(GTEST_LIB gtest PATHS /usr/lib /usr/local/lib
                                       /usr/lib/x86_64-linux-gnu)
    find_library(GTEST_MAIN_LIB gtest_main PATHS /usr/lib /usr/local/lib
                                                 /usr/lib/x86_64-linux-gnu)
    find_path(GTEST_INCLUDE_DIR gtest/gtest.h PATHS /usr/include
                                                    /usr/local/include)

    if(GTEST_LIB
       AND GTEST_MAIN_LIB
       AND GTEST_INCLUDE_DIR)
      set(GTEST_FOUND TRUE)
      set(GTEST_LIBRARIES ${GTEST_LIB} ${GTEST_MAIN_LIB} pthread)
      set(GTEST_INCLUDE_DIRS ${GTEST_INCLUDE_DIR})
      message(STATUS "Found GoogleTest: ${GTEST_LIB}")
    else()
      message(
        WARNING
          "GoogleTest not found - GoogleTest-based tests will not link properly"
      )
      set(GTEST_FOUND FALSE)
    endif()
  endif()

  # Core engine tests
  add_executable(engine_api_tests tests/engine_api_tests.cpp)
  target_link_libraries(engine_api_tests scratchbird_engine)
  add_test(NAME engine_api_tests COMMAND engine_api_tests)

  add_executable(executor_tests tests/executor_tests.cpp)
  target_link_libraries(executor_tests scratchbird_engine)
  add_test(NAME executor_tests COMMAND executor_tests)

  # Heap storage tests
  add_executable(heap_page_tests tests/heap_page_tests.cpp)
  target_link_libraries(heap_page_tests scratchbird_engine)
  add_test(NAME heap_page_tests COMMAND heap_page_tests)

  add_executable(heap_tuple_tests tests/heap_tuple_tests.cpp)
  target_link_libraries(heap_tuple_tests scratchbird_engine)
  add_test(NAME heap_tuple_tests COMMAND heap_tuple_tests)

  add_executable(heap_rel_basic_tests tests/heap_rel_basic_tests.cpp)
  target_link_libraries(heap_rel_basic_tests scratchbird_engine)
  add_test(NAME heap_rel_basic_tests COMMAND heap_rel_basic_tests)

  add_executable(heap_corruption_tests tests/heap_corruption_tests.cpp)
  target_link_libraries(heap_corruption_tests scratchbird_engine)
  add_test(NAME heap_corruption_tests COMMAND heap_corruption_tests)

  add_executable(heap_fuzz_tests tests/heap_fuzz_tests.cpp)
  target_link_libraries(heap_fuzz_tests scratchbird_engine)
  add_test(NAME heap_fuzz_tests COMMAND heap_fuzz_tests)

  add_executable(heap_many_rows_tests tests/heap_many_rows_tests.cpp)
  target_link_libraries(heap_many_rows_tests scratchbird_engine)
  add_test(NAME heap_many_rows_tests COMMAND heap_many_rows_tests)

  add_executable(heap_overflow_tests tests/heap_overflow_tests.cpp)
  target_link_libraries(heap_overflow_tests scratchbird_engine)
  add_test(NAME heap_overflow_tests COMMAND heap_overflow_tests)

  add_executable(heap_rel_fixed_tests tests/heap_rel_fixed_tests.cpp)
  target_link_libraries(heap_rel_fixed_tests scratchbird_engine)
  add_test(NAME heap_rel_fixed_tests COMMAND heap_rel_fixed_tests)

  add_executable(heap_rel_null_tests tests/heap_rel_null_tests.cpp)
  target_link_libraries(heap_rel_null_tests scratchbird_engine)
  add_test(NAME heap_rel_null_tests COMMAND heap_rel_null_tests)

  add_executable(heap_root_tests tests/heap_root_tests.cpp)
  target_link_libraries(heap_root_tests scratchbird_engine)
  add_test(NAME heap_root_tests COMMAND heap_root_tests)

  add_executable(heap_scan_tests tests/heap_scan_tests.cpp)
  target_link_libraries(heap_scan_tests scratchbird_engine)
  add_test(NAME heap_scan_tests COMMAND heap_scan_tests)

  add_executable(heap_truncate_drop_tests tests/heap_truncate_drop_tests.cpp)
  target_link_libraries(heap_truncate_drop_tests scratchbird_engine)
  add_test(NAME heap_truncate_drop_tests COMMAND heap_truncate_drop_tests)

  add_executable(heap_validator_tests tests/heap_validator_tests.cpp)
  target_link_libraries(heap_validator_tests scratchbird_engine)
  add_test(NAME heap_validator_tests COMMAND heap_validator_tests)

  add_executable(heap_varlen_short_tests tests/heap_varlen_short_tests.cpp)
  target_link_libraries(heap_varlen_short_tests scratchbird_engine)
  add_test(NAME heap_varlen_short_tests COMMAND heap_varlen_short_tests)

  # Transaction tests
  add_executable(txn_visibility_tests tests/txn_visibility_tests.cpp)
  target_link_libraries(txn_visibility_tests scratchbird_engine)
  add_test(NAME txn_visibility_tests COMMAND txn_visibility_tests)

  add_executable(txn_update_visibility_tests
                 tests/txn_update_visibility_tests.cpp)
  target_link_libraries(txn_update_visibility_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(txn_update_visibility_tests ${GTEST_LIBRARIES})
    target_include_directories(txn_update_visibility_tests
                               PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME txn_update_visibility_tests COMMAND txn_update_visibility_tests)

  add_executable(txn_delete_visibility_tests
                 tests/txn_delete_visibility_tests.cpp)
  target_link_libraries(txn_delete_visibility_tests scratchbird_engine)
  add_test(NAME txn_delete_visibility_tests COMMAND txn_delete_visibility_tests)

  add_executable(txn_rr_visibility_tests tests/txn_rr_visibility_tests.cpp)
  target_link_libraries(txn_rr_visibility_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(txn_rr_visibility_tests ${GTEST_LIBRARIES})
    target_include_directories(txn_rr_visibility_tests
                               PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME txn_rr_visibility_tests COMMAND txn_rr_visibility_tests)

  add_executable(txn_deadlock_tests tests/txn_deadlock_tests.cpp)
  target_link_libraries(txn_deadlock_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(txn_deadlock_tests ${GTEST_LIBRARIES})
    target_include_directories(txn_deadlock_tests PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME txn_deadlock_tests COMMAND txn_deadlock_tests)

  add_executable(enhanced_deadlock_tests tests/enhanced_deadlock_tests.cpp)
  target_link_libraries(enhanced_deadlock_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(enhanced_deadlock_tests ${GTEST_LIBRARIES})
    target_include_directories(enhanced_deadlock_tests
                               PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME enhanced_deadlock_tests COMMAND enhanced_deadlock_tests)

  add_executable(txn_prepare_scaffold_tests
                 tests/txn_prepare_scaffold_tests.cpp)
  target_link_libraries(txn_prepare_scaffold_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(txn_prepare_scaffold_tests ${GTEST_LIBRARIES})
    target_include_directories(txn_prepare_scaffold_tests
                               PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME txn_prepare_scaffold_tests COMMAND txn_prepare_scaffold_tests)

  # Optimizer tests
  add_executable(optimizer_stability_tests tests/optimizer_stability_tests.cpp)
  target_link_libraries(optimizer_stability_tests scratchbird_engine)
  add_test(NAME optimizer_stability_tests COMMAND optimizer_stability_tests)

  # Phase 7 tests
  add_executable(phase7_constraints_triggers_tests
                 tests/phase7_constraints_triggers_tests.cpp)
  target_link_libraries(phase7_constraints_triggers_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(phase7_constraints_triggers_tests ${GTEST_LIBRARIES})
    target_include_directories(phase7_constraints_triggers_tests
                               PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME phase7_constraints_triggers_tests
           COMMAND phase7_constraints_triggers_tests)

  # Main test runner
  add_executable(scratchbird_tests tests/scratchbird_tests.cpp
                                   src/scratchbird.cpp)
  target_link_libraries(scratchbird_tests scratchbird_engine)
  add_test(NAME scratchbird_tests COMMAND scratchbird_tests)

  add_executable(wal_tests tests/wal_tests.cpp)
  target_link_libraries(wal_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(wal_tests ${GTEST_LIBRARIES})
    target_include_directories(wal_tests PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME wal_tests COMMAND wal_tests)

  add_executable(having_tests tests/having_tests.cpp)
  target_link_libraries(having_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(having_tests ${GTEST_LIBRARIES})
    target_include_directories(having_tests PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME having_tests COMMAND having_tests)

  add_executable(fk_set_default_tests tests/fk_set_default_tests.cpp)
  target_link_libraries(fk_set_default_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(fk_set_default_tests ${GTEST_LIBRARIES})
    target_include_directories(fk_set_default_tests
                               PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()

  add_executable(create_index_tests tests/create_index_tests.cpp)
  target_link_libraries(create_index_tests scratchbird_engine)

  add_executable(window_functions_tests tests/window_functions_tests.cpp)
  target_link_libraries(window_functions_tests scratchbird_engine)

  add_executable(hash_join_tests tests/hash_join_tests.cpp)
  target_link_libraries(hash_join_tests scratchbird_engine)

  add_executable(expression_engine_tests tests/expression_engine_tests.cpp)
  target_link_libraries(expression_engine_tests scratchbird_engine)

  add_executable(explain_analyze_tests tests/explain_analyze_tests.cpp)
  target_link_libraries(explain_analyze_tests scratchbird_engine)

  add_executable(sort_aggregation_tests tests/sort_aggregation_tests.cpp)
  target_link_libraries(sort_aggregation_tests scratchbird_engine)

  add_executable(real_heap_integration_tests
                 tests/real_heap_integration_tests.cpp)
  target_link_libraries(real_heap_integration_tests scratchbird_engine)

  add_executable(statistics_tests tests/statistics_tests.cpp)
  target_link_libraries(statistics_tests scratchbird_engine)

  add_executable(trigger_engine_tests tests/trigger_engine_tests.cpp)
  target_link_libraries(trigger_engine_tests scratchbird_engine)

  add_executable(wal_manager_tests tests/wal_manager_tests.cpp)
  target_link_libraries(wal_manager_tests scratchbird_engine)

  add_executable(multi_segment_tests tests/multi_segment_tests.cpp)
  target_link_libraries(multi_segment_tests scratchbird_engine)

  add_executable(alter_table_tests tests/alter_table_tests.cpp)
  target_link_libraries(alter_table_tests scratchbird_engine)

  add_executable(security_tests tests/security_tests.cpp)
  target_link_libraries(security_tests scratchbird_engine)

  add_executable(serializable_tests tests/serializable_tests.cpp)
  target_link_libraries(serializable_tests scratchbird_engine)

  add_executable(serializable_simple_tests tests/serializable_simple_tests.cpp)
  target_link_libraries(serializable_simple_tests scratchbird_engine)

  # Set operations and CREATE VIEW tests
  add_executable(test_union_simple tests/test_union_simple.cpp)
  target_link_libraries(test_union_simple scratchbird_engine)
  add_test(NAME test_union_simple COMMAND test_union_simple)

  add_executable(test_union_advanced tests/test_union_advanced.cpp)
  target_link_libraries(test_union_advanced scratchbird_engine)
  add_test(NAME test_union_advanced COMMAND test_union_advanced)

  add_executable(test_create_view tests/test_create_view.cpp)
  target_link_libraries(test_create_view scratchbird_engine)
  add_test(NAME test_create_view COMMAND test_create_view)

  add_test(NAME fk_set_default_tests COMMAND fk_set_default_tests)

  # PSQL Tests
  add_executable(psql_basic_tests tests/psql_basic_tests.cpp)
  target_link_libraries(psql_basic_tests scratchbird_engine)
  add_test(NAME psql_basic_tests COMMAND psql_basic_tests)

  # Stored Procedure Tests
  add_executable(stored_procedure_tests tests/stored_procedure_tests.cpp)
  target_link_libraries(stored_procedure_tests scratchbird_engine)
  add_test(NAME stored_procedure_tests COMMAND stored_procedure_tests)

  # Function Execution Tests
  add_executable(function_execution_tests tests/function_execution_tests.cpp)
  target_link_libraries(function_execution_tests scratchbird_engine)
  add_test(NAME function_execution_tests COMMAND function_execution_tests)

  # Exception Handling Tests
  add_executable(exception_handling_tests tests/exception_handling_tests.cpp)
  target_link_libraries(exception_handling_tests scratchbird_engine)
  add_test(NAME exception_handling_tests COMMAND exception_handling_tests)

  # Cursor Tests
  add_executable(cursor_tests tests/cursor_tests.cpp)
  target_link_libraries(cursor_tests scratchbird_engine)
  add_test(NAME cursor_tests COMMAND cursor_tests)

  # Advanced PSQL Features Tests
  add_executable(advanced_psql_features_tests
                 tests/advanced_psql_features_tests.cpp)
  target_link_libraries(advanced_psql_features_tests scratchbird_engine)
  add_test(NAME advanced_psql_features_tests
           COMMAND advanced_psql_features_tests)

  # Package Support Tests
  add_executable(package_support_tests tests/package_support_tests.cpp)
  target_link_libraries(package_support_tests scratchbird_engine)
  add_test(NAME package_support_tests COMMAND package_support_tests)

  # Performance Optimization Tests
  add_executable(performance_optimization_tests
                 tests/performance_optimization_tests.cpp)
  target_link_libraries(performance_optimization_tests scratchbird_engine)
  add_test(NAME performance_optimization_tests
           COMMAND performance_optimization_tests)

  add_executable(partitioning_tests tests/partitioning_tests.cpp)
  target_link_libraries(partitioning_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(partitioning_tests ${GTEST_LIBRARIES})
    target_include_directories(partitioning_tests PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME partitioning_tests COMMAND partitioning_tests)

  # Analytics and Time-series function tests
  add_executable(analytics_functions_tests tests/analytics_functions_tests.cpp)
  target_link_libraries(analytics_functions_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(analytics_functions_tests ${GTEST_LIBRARIES})
    target_include_directories(analytics_functions_tests PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME analytics_functions_tests COMMAND analytics_functions_tests)

  add_executable(timeseries_functions_tests tests/timeseries_functions_tests.cpp)
  target_link_libraries(timeseries_functions_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(timeseries_functions_tests ${GTEST_LIBRARIES})
    target_include_directories(timeseries_functions_tests PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME timeseries_functions_tests COMMAND timeseries_functions_tests)

  # PSQL Debugging Support Tests
  add_executable(psql_debugging_tests tests/psql_debugging_tests.cpp)
  target_link_libraries(psql_debugging_tests scratchbird_engine)
  add_test(NAME psql_debugging_tests COMMAND psql_debugging_tests)

  # PSQL Development Tools Tests
  add_executable(psql_dev_tools_tests tests/psql_dev_tools_tests.cpp)
  target_link_libraries(psql_dev_tools_tests scratchbird_engine)
  add_test(NAME psql_dev_tools_tests COMMAND psql_dev_tools_tests)

  # Parser Debug Test
  add_executable(parser_debug_test tests/parser_debug_test.cpp)
  target_link_libraries(parser_debug_test scratchbird_engine)

  add_executable(hash_index_tests tests/hash_index_tests.cpp)
  target_link_libraries(hash_index_tests scratchbird_engine)
  add_test(NAME hash_index_tests COMMAND hash_index_tests)

  add_executable(hash_index_memory_debug tests/hash_index_memory_debug.cpp)
  target_link_libraries(hash_index_memory_debug scratchbird_engine)
  add_test(NAME hash_index_memory_debug COMMAND hash_index_memory_debug)

  add_executable(rtree_index_tests tests/rtree_index_tests.cpp)
  target_link_libraries(rtree_index_tests scratchbird_engine)
  add_test(NAME rtree_index_tests COMMAND rtree_index_tests)

  add_executable(lsm_index_tests tests/lsm_index_tests.cpp)
  target_link_libraries(lsm_index_tests scratchbird_engine)
  add_test(NAME lsm_index_tests COMMAND lsm_index_tests)

  add_executable(columnstore_index_tests tests/columnstore_index_tests.cpp)
  target_link_libraries(columnstore_index_tests scratchbird_engine)
  add_test(NAME columnstore_index_tests COMMAND columnstore_index_tests)
  add_executable(index_integration_tests tests/index_integration_tests.cpp)
  target_link_libraries(index_integration_tests scratchbird_engine)
  add_test(NAME index_integration_tests COMMAND index_integration_tests)
  add_executable(query_optimizer_index_tests
                 tests/query_optimizer_index_tests.cpp)
  target_link_libraries(query_optimizer_index_tests scratchbird_engine)
  add_test(NAME query_optimizer_index_tests COMMAND query_optimizer_index_tests)

  add_executable(ttl_index_tests tests/ttl_index_tests.cpp)
  target_link_libraries(ttl_index_tests scratchbird_engine)
  add_test(NAME ttl_index_tests COMMAND ttl_index_tests)

  add_executable(comprehensive_index_validation_tests
                 tests/comprehensive_index_validation_tests.cpp)
  target_link_libraries(comprehensive_index_validation_tests scratchbird_engine)
  add_test(NAME comprehensive_index_validation_tests
           COMMAND comprehensive_index_validation_tests)

  add_executable(fdw_basic_tests tests/fdw_basic_tests.cpp)
  target_link_libraries(fdw_basic_tests scratchbird_engine)
  add_test(NAME fdw_basic_tests COMMAND fdw_basic_tests)

  add_executable(fdw_ddl_tests tests/fdw_ddl_tests.cpp)
  target_link_libraries(fdw_ddl_tests scratchbird_engine)
  add_test(NAME fdw_ddl_tests COMMAND fdw_ddl_tests)

  add_executable(fdw_postgresql_tests tests/fdw_postgresql_tests.cpp)
  target_link_libraries(fdw_postgresql_tests scratchbird_engine)
  add_test(NAME fdw_postgresql_tests COMMAND fdw_postgresql_tests)

  add_executable(database_link_tests tests/database_link_tests.cpp)
  target_link_libraries(database_link_tests scratchbird_engine)
  add_test(NAME database_link_tests COMMAND database_link_tests)

  add_executable(fdw_json_tests tests/fdw_json_tests.cpp)
  target_link_libraries(fdw_json_tests scratchbird_engine)
  add_test(NAME fdw_json_tests COMMAND fdw_json_tests)

  add_executable(fdw_security_tests tests/fdw_security_tests.cpp)
  target_link_libraries(fdw_security_tests scratchbird_engine)
  add_test(NAME fdw_security_tests COMMAND fdw_security_tests)

  add_executable(fdw_error_handling_tests tests/fdw_error_handling_tests.cpp)
  target_link_libraries(fdw_error_handling_tests scratchbird_engine)
  add_test(NAME fdw_error_handling_tests COMMAND fdw_error_handling_tests)

  add_executable(fdw_catalog_tests tests/fdw_catalog_tests.cpp)
  target_link_libraries(fdw_catalog_tests scratchbird_engine)
  add_test(NAME fdw_catalog_tests COMMAND fdw_catalog_tests)

  add_executable(fdw_comprehensive_tests tests/fdw_comprehensive_tests.cpp)
  target_link_libraries(fdw_comprehensive_tests scratchbird_engine)
  add_test(NAME fdw_comprehensive_tests COMMAND fdw_comprehensive_tests)

  # Phase 11: Network Server Tests
  add_executable(network_server_tests tests/network_server_tests.cpp)
  target_link_libraries(network_server_tests scratchbird_engine)
  add_test(NAME network_server_tests COMMAND network_server_tests)

  add_executable(protocol_handler_tests tests/protocol_handler_tests.cpp)
  target_link_libraries(protocol_handler_tests scratchbird_engine)
  add_test(NAME protocol_handler_tests COMMAND protocol_handler_tests)

  add_executable(threading_tests tests/threading_tests.cpp)
  target_link_libraries(threading_tests scratchbird_engine)
  add_test(NAME threading_tests COMMAND threading_tests)

  add_executable(firebird_protocol_tests tests/firebird_protocol_tests.cpp)
  target_link_libraries(firebird_protocol_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(firebird_protocol_tests ${GTEST_LIBRARIES})
    target_include_directories(firebird_protocol_tests
                               PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME firebird_protocol_tests COMMAND firebird_protocol_tests)

  add_executable(provider_dispatch_tests tests/provider_dispatch_tests.cpp)
  target_link_libraries(provider_dispatch_tests scratchbird_engine)
  add_test(NAME provider_dispatch_tests COMMAND provider_dispatch_tests)

  add_executable(database_provider_tests tests/database_provider_tests.cpp)
  target_link_libraries(database_provider_tests scratchbird_engine)
  add_test(NAME database_provider_tests COMMAND database_provider_tests)

  add_executable(embedded_provider_tests tests/embedded_provider_tests.cpp)
  target_link_libraries(embedded_provider_tests scratchbird_engine)
  add_test(NAME embedded_provider_tests COMMAND embedded_provider_tests)

  add_executable(remote_provider_tests tests/remote_provider_tests.cpp)
  target_link_libraries(remote_provider_tests scratchbird_engine)
  add_test(NAME remote_provider_tests COMMAND remote_provider_tests)

  # Phase 11.4: Authentication System Tests
  add_executable(authentication_tests tests/authentication_tests.cpp)
  target_link_libraries(authentication_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(authentication_tests ${GTEST_LIBRARIES})
    target_include_directories(authentication_tests
                               PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME authentication_tests COMMAND authentication_tests)

  add_executable(password_auth_tests tests/password_auth_tests.cpp)
  target_link_libraries(password_auth_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(password_auth_tests ${GTEST_LIBRARIES})
    target_include_directories(password_auth_tests
                               PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME password_auth_tests COMMAND password_auth_tests)

  add_executable(trusted_auth_tests tests/trusted_auth_tests.cpp)
  target_link_libraries(trusted_auth_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(trusted_auth_tests ${GTEST_LIBRARIES})
    target_include_directories(trusted_auth_tests PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME trusted_auth_tests COMMAND trusted_auth_tests)

  add_executable(two_factor_auth_tests tests/two_factor_auth_tests.cpp)
  target_link_libraries(two_factor_auth_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(two_factor_auth_tests ${GTEST_LIBRARIES})
    target_include_directories(two_factor_auth_tests
                               PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME two_factor_auth_tests COMMAND two_factor_auth_tests)

  # Phase 11.5: TLS and Security Tests
  add_executable(tls_server_tests tests/tls_server_tests.cpp)
  target_link_libraries(tls_server_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(tls_server_tests ${GTEST_LIBRARIES})
    target_include_directories(tls_server_tests PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME tls_server_tests COMMAND tls_server_tests)

  # Phase 11.6: Role Management and Security Context Tests
  add_executable(role_management_tests tests/role_management_tests.cpp)
  target_link_libraries(role_management_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(role_management_tests ${GTEST_LIBRARIES})
    target_include_directories(role_management_tests
                               PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME role_management_tests COMMAND role_management_tests)

  # Phase 11.7: TCP Optimizer Tests
  add_executable(tcp_optimizer_tests tests/tcp_optimizer_tests.cpp)
  target_link_libraries(tcp_optimizer_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(tcp_optimizer_tests ${GTEST_LIBRARIES})
    target_include_directories(tcp_optimizer_tests
                               PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME tcp_optimizer_tests COMMAND tcp_optimizer_tests)

  # Phase 11.7: Connection Pool Tests
  add_executable(connection_pool_tests tests/connection_pool_tests.cpp)
  target_link_libraries(connection_pool_tests scratchbird_engine)
  if(GTEST_FOUND)
    target_link_libraries(connection_pool_tests ${GTEST_LIBRARIES})
    target_include_directories(connection_pool_tests
                               PRIVATE ${GTEST_INCLUDE_DIRS})
  endif()
  add_test(NAME connection_pool_tests COMMAND connection_pool_tests)

endif()
