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
  src/engine/heap_validator.cpp
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
  src/engine/statistics.cpp)

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
  add_test(NAME txn_update_visibility_tests COMMAND txn_update_visibility_tests)

  add_executable(txn_delete_visibility_tests
                 tests/txn_delete_visibility_tests.cpp)
  target_link_libraries(txn_delete_visibility_tests scratchbird_engine)
  add_test(NAME txn_delete_visibility_tests COMMAND txn_delete_visibility_tests)

  add_executable(txn_rr_visibility_tests tests/txn_rr_visibility_tests.cpp)
  target_link_libraries(txn_rr_visibility_tests scratchbird_engine)
  add_test(NAME txn_rr_visibility_tests COMMAND txn_rr_visibility_tests)

  add_executable(txn_deadlock_tests tests/txn_deadlock_tests.cpp)
  target_link_libraries(txn_deadlock_tests scratchbird_engine)
  add_test(NAME txn_deadlock_tests COMMAND txn_deadlock_tests)

  add_executable(txn_prepare_scaffold_tests
                 tests/txn_prepare_scaffold_tests.cpp)
  target_link_libraries(txn_prepare_scaffold_tests scratchbird_engine)
  add_test(NAME txn_prepare_scaffold_tests COMMAND txn_prepare_scaffold_tests)

  # Optimizer tests
  add_executable(optimizer_stability_tests tests/optimizer_stability_tests.cpp)
  target_link_libraries(optimizer_stability_tests scratchbird_engine)
  add_test(NAME optimizer_stability_tests COMMAND optimizer_stability_tests)

  # Phase 7 tests
  add_executable(phase7_constraints_triggers_tests
                 tests/phase7_constraints_triggers_tests.cpp)
  target_link_libraries(phase7_constraints_triggers_tests scratchbird_engine)
  add_test(NAME phase7_constraints_triggers_tests
           COMMAND phase7_constraints_triggers_tests)

  # Main test runner
  add_executable(scratchbird_tests tests/scratchbird_tests.cpp)
  target_link_libraries(scratchbird_tests scratchbird_engine)
  add_test(NAME scratchbird_tests COMMAND scratchbird_tests)

  add_executable(wal_tests tests/wal_tests.cpp)
  target_link_libraries(wal_tests scratchbird_engine)
  add_test(NAME wal_tests COMMAND wal_tests)

  add_executable(having_tests tests/having_tests.cpp)
  target_link_libraries(having_tests scratchbird_engine)
  add_test(NAME having_tests COMMAND having_tests)

  add_executable(fk_set_default_tests tests/fk_set_default_tests.cpp)
  target_link_libraries(fk_set_default_tests scratchbird_engine)

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
  add_test(NAME fk_set_default_tests COMMAND fk_set_default_tests)
endif()
