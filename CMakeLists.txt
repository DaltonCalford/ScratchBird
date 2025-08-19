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
option(SCRATCHBIRD_WITH_ICU "Enable ICU for Unicode casefolding/collation" OFF)
option(SCRATCHBIRD_WITH_UTF8PROC "Enable utf8proc for Unicode casefolding" OFF)

# Warnings
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  if(SCRATCHBIRD_WARNINGS_AS_ERRORS)
    add_compile_options(-Werror)
  endif()
endif()

# Library target example
add_library(
  scratchbird STATIC
  src/scratchbird.cpp
  src/engine/engine.cpp
  src/engine/file.cpp
  src/server/server.cpp
  src/server/yvalve.cpp
  src/server/listener.cpp
  src/protocol/protocol.cpp
  src/auth/auth.cpp
  src/providers/providers.cpp
  src/engine/capi.cpp
  src/engine/pager.cpp
  src/engine/alloc.cpp
  src/engine/txn.cpp
  src/engine/header.cpp
  src/engine/generators.cpp
  src/engine/parser.cpp
  src/engine/parser_select.cpp
  src/engine/parser_expr.cpp
  src/engine/parser_dml.cpp
  src/engine/executor.cpp
  src/engine/planner.cpp
  src/engine/stats.cpp
  src/engine/expr.cpp
  src/engine/storage.cpp
  src/engine/ods.cpp
  src/engine/types.cpp
  src/engine/lexer.cpp
  src/engine/resolver.cpp
  src/engine/parser_session.cpp
  src/engine/parser_psql.cpp
  src/engine/parser_ddl.cpp
  src/engine/config.cpp)
target_sources(scratchbird PRIVATE src/engine/uuid.cpp)
target_sources(scratchbird PRIVATE src/engine/catalog_bootstrap.cpp)
target_sources(scratchbird PRIVATE src/engine/system_oids.cpp)
target_sources(scratchbird PRIVATE src/engine/catalog_manager.cpp)
target_sources(scratchbird PRIVATE src/engine/index.cpp)
target_sources(scratchbird PRIVATE src/engine/index_btree.cpp)
target_sources(scratchbird PRIVATE src/engine/btree_page.cpp)
target_sources(scratchbird PRIVATE src/engine/btree_v1.cpp)
target_sources(scratchbird PRIVATE src/engine/wal.cpp)
target_sources(scratchbird PRIVATE src/engine/index_build.cpp)
target_sources(scratchbird PRIVATE src/engine/index_online.cpp)
target_sources(scratchbird PRIVATE src/engine/catalog_mem.cpp)
target_sources(scratchbird PRIVATE src/engine/monitoring.cpp)

target_include_directories(
  scratchbird PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                     $<INSTALL_INTERFACE:include>)

# tools: isql meta parser (OBJECT library)
add_library(isql_meta OBJECT tools/isql/meta.cpp)
target_include_directories(isql_meta PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(SCRATCHBIRD_WITH_ICU)
  find_package(
    ICU
    COMPONENTS uc i18n
    REQUIRED)
  target_compile_definitions(scratchbird PUBLIC SCRATCHBIRD_WITH_ICU=1)
  target_link_libraries(scratchbird PUBLIC ICU::uc ICU::i18n)
endif()
if(SCRATCHBIRD_WITH_UTF8PROC)
  find_path(UTF8PROC_INCLUDE_DIR utf8proc.h)
  find_library(UTF8PROC_LIBRARY NAMES utf8proc)
  if(UTF8PROC_INCLUDE_DIR AND UTF8PROC_LIBRARY)
    target_compile_definitions(scratchbird PUBLIC SCRATCHBIRD_WITH_UTF8PROC=1)
    target_include_directories(scratchbird PUBLIC ${UTF8PROC_INCLUDE_DIR})
    target_link_libraries(scratchbird PUBLIC ${UTF8PROC_LIBRARY})
  else()
    message(FATAL_ERROR "utf8proc requested but not found")
  endif()
endif()

# Example CLI
add_executable(scratchbird_cli src/main.cpp)

target_link_libraries(scratchbird_cli PRIVATE scratchbird)

# Tests
if(SCRATCHBIRD_BUILD_TESTS)
  include(CTest)
  if(BUILD_TESTING)
    add_executable(index_validation_tests tests/index_validation_tests.cpp)
    target_link_libraries(index_validation_tests PRIVATE scratchbird)
    add_test(NAME index_validation_tests COMMAND index_validation_tests)
    add_executable(index_btree_build_tests tests/index_btree_build_tests.cpp)
    target_link_libraries(index_btree_build_tests PRIVATE scratchbird)
    add_test(NAME index_btree_build_tests COMMAND index_btree_build_tests)
    add_executable(index_build_manager_tests
                   tests/index_build_manager_tests.cpp)
    target_link_libraries(index_build_manager_tests PRIVATE scratchbird)
    add_test(NAME index_build_manager_tests COMMAND index_build_manager_tests)
    add_executable(index_online_build_tests tests/index_online_build_tests.cpp)
    target_link_libraries(index_online_build_tests PRIVATE scratchbird pthread)
    add_test(NAME index_online_build_tests COMMAND index_online_build_tests)
    add_executable(index_btree_stage1_tests tests/index_btree_stage1_tests.cpp)
    target_link_libraries(index_btree_stage1_tests PRIVATE scratchbird)
    add_test(NAME index_btree_stage1_tests COMMAND index_btree_stage1_tests)
    add_executable(index_btree_stage2_tests tests/index_btree_stage2_tests.cpp)
    target_link_libraries(index_btree_stage2_tests PRIVATE scratchbird)
    add_test(NAME index_btree_stage2_tests COMMAND index_btree_stage2_tests)
    add_executable(index_btree_stage3_tests tests/index_btree_stage3_tests.cpp)
    target_link_libraries(index_btree_stage3_tests PRIVATE scratchbird)
    add_test(NAME index_btree_stage3_tests COMMAND index_btree_stage3_tests)
    add_executable(btree_page_roundtrip_tests
                   tests/btree_page_roundtrip_tests.cpp)
    target_link_libraries(btree_page_roundtrip_tests PRIVATE scratchbird)
    add_test(NAME btree_page_roundtrip_tests COMMAND btree_page_roundtrip_tests)
    add_executable(btree_v1_randomized_tests
                   tests/btree_v1_randomized_tests.cpp)
    target_link_libraries(btree_v1_randomized_tests PRIVATE scratchbird)
    add_test(NAME btree_v1_randomized_tests COMMAND btree_v1_randomized_tests)
    set_tests_properties(btree_v1_randomized_tests PROPERTIES TIMEOUT 20)
    add_executable(btree_v1_concurrency_tests
                   tests/btree_v1_concurrency_tests.cpp)
    target_link_libraries(btree_v1_concurrency_tests PRIVATE scratchbird
                                                             pthread)
    add_test(NAME btree_v1_concurrency_tests COMMAND btree_v1_concurrency_tests)
    # Keep a modest timeout; workload has been reduced to avoid flakiness
    set_tests_properties(btree_v1_concurrency_tests PROPERTIES TIMEOUT 15)
    add_executable(wal_recovery_tests tests/wal_recovery_tests.cpp)
    target_link_libraries(wal_recovery_tests PRIVATE scratchbird)
    add_test(NAME wal_recovery_tests COMMAND wal_recovery_tests)
    add_executable(wal_crash_recovery_tests tests/wal_crash_recovery_tests.cpp)
    target_link_libraries(wal_crash_recovery_tests PRIVATE scratchbird)
    add_test(NAME wal_crash_recovery_tests COMMAND wal_crash_recovery_tests)
    # bootstrap sidecar test disabled pending SQL executor integration
    add_executable(scratchbird_tests tests/scratchbird_tests.cpp)
    target_link_libraries(scratchbird_tests PRIVATE scratchbird)
    add_test(NAME scratchbird_tests COMMAND scratchbird_tests)
    add_executable(engine_api_tests tests/engine_api_tests.cpp)
    target_link_libraries(engine_api_tests PRIVATE scratchbird)
    add_test(NAME engine_api_tests COMMAND engine_api_tests)
    add_executable(protocol_tests tests/protocol_tests.cpp)
    target_link_libraries(protocol_tests PRIVATE scratchbird)
    add_test(NAME protocol_tests COMMAND protocol_tests)
    add_executable(protocol_framing_tests tests/protocol_framing_tests.cpp)
    target_link_libraries(protocol_framing_tests PRIVATE scratchbird)
    add_test(NAME protocol_framing_tests COMMAND protocol_framing_tests)
    add_executable(auth_tests tests/auth_tests.cpp)
    target_link_libraries(auth_tests PRIVATE scratchbird)
    add_test(NAME auth_tests COMMAND auth_tests)
    add_executable(provider_tests tests/provider_tests.cpp)
    add_executable(server_session_tests tests/server_session_tests.cpp)
    target_link_libraries(server_session_tests PRIVATE scratchbird)
    add_test(NAME server_session_tests COMMAND server_session_tests)
    target_link_libraries(provider_tests PRIVATE scratchbird)
    add_test(NAME provider_tests COMMAND provider_tests)
    add_executable(yvalve_tests tests/yvalve_tests.cpp)
    target_link_libraries(yvalve_tests PRIVATE scratchbird)
    add_test(NAME yvalve_tests COMMAND yvalve_tests)
    add_executable(listener_tests tests/listener_tests.cpp)
    target_link_libraries(listener_tests PRIVATE scratchbird)
    add_test(NAME listener_tests COMMAND listener_tests)
    add_executable(capi_tests tests/capi_tests.cpp)
    target_link_libraries(capi_tests PRIVATE scratchbird)
    add_test(NAME capi_tests COMMAND capi_tests)
    add_executable(parser_tests tests/parser_tests.cpp)
    target_link_libraries(parser_tests PRIVATE scratchbird)
    add_test(NAME parser_tests COMMAND parser_tests)
    add_executable(executor_tests tests/executor_tests.cpp)
    target_link_libraries(executor_tests PRIVATE scratchbird)
    add_test(NAME executor_tests COMMAND executor_tests)
    add_executable(types_tests tests/types_tests.cpp)
    target_link_libraries(types_tests PRIVATE scratchbird)
    add_test(NAME types_tests COMMAND types_tests)
    add_executable(parser_acceptance_lex tests/parser_acceptance_lex.cpp)
    target_link_libraries(parser_acceptance_lex PRIVATE scratchbird)
    add_test(NAME parser_acceptance_lex COMMAND parser_acceptance_lex)
    add_executable(parser_select_tests tests/parser_select_tests.cpp)
    target_link_libraries(parser_select_tests PRIVATE scratchbird)
    add_test(NAME parser_select_tests COMMAND parser_select_tests)
    # Re-enable full SELECT tests set_tests_properties(parser_select_tests
    # PROPERTIES DISABLED TRUE) Focused window-only tests for iterative
    # stabilization
    add_executable(parser_window_tests tests/parser_window_tests.cpp)
    target_link_libraries(parser_window_tests PRIVATE scratchbird)
    add_test(NAME parser_window_tests COMMAND parser_window_tests)
    add_executable(parser_expr_tests tests/parser_expr_tests.cpp)
    target_link_libraries(parser_expr_tests PRIVATE scratchbird)
    add_test(NAME parser_expr_tests COMMAND parser_expr_tests)
    add_executable(parser_dml_tests tests/parser_dml_tests.cpp)
    target_link_libraries(parser_dml_tests PRIVATE scratchbird)
    add_test(NAME parser_dml_tests COMMAND parser_dml_tests)
    add_executable(resolver_ident_tests tests/resolver_ident_tests.cpp)
    target_link_libraries(resolver_ident_tests PRIVATE scratchbird)
    add_test(NAME resolver_ident_tests COMMAND resolver_ident_tests)
    add_executable(parser_session_tests tests/parser_session_tests.cpp)
    target_link_libraries(parser_session_tests PRIVATE scratchbird)
    add_test(NAME parser_session_tests COMMAND parser_session_tests)
    add_executable(parser_session_options_tests
                   tests/parser_session_options_tests.cpp)
    target_link_libraries(parser_session_options_tests PRIVATE scratchbird)
    add_test(NAME parser_session_options_tests
             COMMAND parser_session_options_tests)
    add_executable(parser_ddl_tests tests/parser_ddl_tests.cpp)
    target_link_libraries(parser_ddl_tests PRIVATE scratchbird)
    add_test(NAME parser_ddl_tests COMMAND parser_ddl_tests)
    add_executable(parser_ddl2_tests tests/parser_ddl2_tests.cpp)
    target_link_libraries(parser_ddl2_tests PRIVATE scratchbird)
    add_test(NAME parser_ddl2_tests COMMAND parser_ddl2_tests)
    add_executable(parser_ddl_external_tests
                   tests/parser_ddl_external_tests.cpp)
    target_link_libraries(parser_ddl_external_tests PRIVATE scratchbird)
    add_test(NAME parser_ddl_external_tests COMMAND parser_ddl_external_tests)
    add_executable(parser_grant_revoke_tests
                   tests/parser_grant_revoke_tests.cpp)
    target_link_libraries(parser_grant_revoke_tests PRIVATE scratchbird)
    add_test(NAME parser_grant_revoke_tests COMMAND parser_grant_revoke_tests)
    add_executable(parser_grant_revoke_ext_tests
                   tests/parser_grant_revoke_ext_tests.cpp)
    target_link_libraries(parser_grant_revoke_ext_tests PRIVATE scratchbird)
    add_test(NAME parser_grant_revoke_ext_tests
             COMMAND parser_grant_revoke_ext_tests)
    add_executable(parser_comment_rename_ext_tests
                   tests/parser_comment_rename_ext_tests.cpp)
    target_link_libraries(parser_comment_rename_ext_tests PRIVATE scratchbird)
    add_test(NAME parser_comment_rename_ext_tests
             COMMAND parser_comment_rename_ext_tests)
    add_executable(parser_tablespace_tests tests/parser_tablespace_tests.cpp)
    target_link_libraries(parser_tablespace_tests PRIVATE scratchbird)
    add_test(NAME parser_tablespace_tests COMMAND parser_tablespace_tests)
    add_executable(parser_foreign_data_tests
                   tests/parser_foreign_data_tests.cpp)
    target_link_libraries(parser_foreign_data_tests PRIVATE scratchbird)
    add_test(NAME parser_foreign_data_tests COMMAND parser_foreign_data_tests)
    add_executable(parser_replication_tests tests/parser_replication_tests.cpp)
    target_link_libraries(parser_replication_tests PRIVATE scratchbird)
    add_test(NAME parser_replication_tests COMMAND parser_replication_tests)
    add_executable(parser_logging_audit_tests
                   tests/parser_logging_audit_tests.cpp)
    target_link_libraries(parser_logging_audit_tests PRIVATE scratchbird)
    add_test(NAME parser_logging_audit_tests COMMAND parser_logging_audit_tests)
    add_executable(parser_cluster_tests tests/parser_cluster_tests.cpp)
    target_link_libraries(parser_cluster_tests PRIVATE scratchbird)
    add_test(NAME parser_cluster_tests COMMAND parser_cluster_tests)
    add_executable(parser_modern_features_tests
                   tests/parser_modern_features_tests.cpp)
    target_link_libraries(parser_modern_features_tests PRIVATE scratchbird)
    add_test(NAME parser_modern_features_tests
             COMMAND parser_modern_features_tests)
    add_executable(parser_ddl_rename_basic_tests
                   tests/parser_ddl_rename_basic_tests.cpp)
    target_link_libraries(parser_ddl_rename_basic_tests PRIVATE scratchbird)
    add_test(NAME parser_ddl_rename_basic_tests
             COMMAND parser_ddl_rename_basic_tests)
    add_executable(parser_move_schema_tests tests/parser_move_schema_tests.cpp)
    target_link_libraries(parser_move_schema_tests PRIVATE scratchbird)
    add_test(NAME parser_move_schema_tests COMMAND parser_move_schema_tests)
    add_executable(parser_grant_revoke_newobjects_tests
                   tests/parser_grant_revoke_newobjects_tests.cpp)
    target_link_libraries(parser_grant_revoke_newobjects_tests
                          PRIVATE scratchbird)
    add_test(NAME parser_grant_revoke_newobjects_tests
             COMMAND parser_grant_revoke_newobjects_tests)
    add_executable(psql_execute_block_tests tests/psql_execute_block_tests.cpp)
    target_link_libraries(psql_execute_block_tests PRIVATE scratchbird)
    add_test(NAME psql_execute_block_tests COMMAND psql_execute_block_tests)
    add_executable(psql_stmt_enrich_tests tests/psql_stmt_enrich_tests.cpp)
    target_link_libraries(psql_stmt_enrich_tests PRIVATE scratchbird)
    add_test(NAME psql_stmt_enrich_tests COMMAND psql_stmt_enrich_tests)
    add_executable(psql_routines_triggers_packages_tests
                   tests/psql_routines_triggers_packages_tests.cpp)
    target_link_libraries(psql_routines_triggers_packages_tests
                          PRIVATE scratchbird)
    add_test(NAME psql_routines_triggers_packages_tests
             COMMAND psql_routines_triggers_packages_tests)
    add_executable(isql_meta_tests tests/isql_meta_tests.cpp)
    target_link_libraries(isql_meta_tests PRIVATE isql_meta scratchbird)
    add_test(NAME isql_meta_tests COMMAND isql_meta_tests)
    add_executable(parser_admin_surfaces_tests
                   tests/parser_admin_surfaces_tests.cpp)
    target_link_libraries(parser_admin_surfaces_tests PRIVATE scratchbird)
    add_test(NAME parser_admin_surfaces_tests
             COMMAND parser_admin_surfaces_tests)
    add_executable(parser_join_tree_tests tests/parser_join_tree_tests.cpp)
    target_link_libraries(parser_join_tree_tests PRIVATE scratchbird)
    add_test(NAME parser_join_tree_tests COMMAND parser_join_tree_tests)
    add_executable(lexer_literal_tests tests/lexer_literal_tests.cpp)
    target_link_libraries(lexer_literal_tests PRIVATE scratchbird)
    add_test(NAME lexer_literal_tests COMMAND lexer_literal_tests)
    add_executable(types_spatial_smoke tests/types_spatial_smoke.cpp)
    target_link_libraries(types_spatial_smoke PRIVATE scratchbird)
    add_test(NAME types_spatial_smoke COMMAND types_spatial_smoke)
    add_executable(parser_ddl_index_ext_tests
                   tests/parser_ddl_index_ext_tests.cpp)
    target_link_libraries(parser_ddl_index_ext_tests PRIVATE scratchbird)
    add_test(NAME parser_ddl_index_ext_tests COMMAND parser_ddl_index_ext_tests)
    add_executable(parser_dblink_tests tests/parser_dblink_tests.cpp)
    target_link_libraries(parser_dblink_tests PRIVATE scratchbird)
    add_test(NAME parser_dblink_tests COMMAND parser_dblink_tests)

    add_executable(phase7_constraints_triggers_tests
                   tests/phase7_constraints_triggers_tests.cpp)
    target_include_directories(phase7_constraints_triggers_tests
                               PRIVATE include)
    target_link_libraries(phase7_constraints_triggers_tests
                          PRIVATE scratchbird gtest gtest_main)
    add_test(NAME phase7_constraints_triggers_tests
             COMMAND phase7_constraints_triggers_tests)
    # Fuzz and bench (lightweight, deterministic)
    add_executable(fuzz_smoke tools/fuzz/fuzz_smoke.cpp)
    target_link_libraries(fuzz_smoke PRIVATE scratchbird)
    add_test(NAME fuzz_smoke COMMAND fuzz_smoke)
    add_executable(parser_bench_smoke tools/fuzz/parser_bench.cpp)
    target_link_libraries(parser_bench_smoke PRIVATE scratchbird)
    add_test(NAME parser_bench_smoke COMMAND parser_bench_smoke)
    # Optimizer stability/CI gate
    add_executable(optimizer_stability_tests
                   tests/optimizer_stability_tests.cpp)
    target_link_libraries(optimizer_stability_tests PRIVATE scratchbird)
    add_test(NAME optimizer_stability_tests COMMAND optimizer_stability_tests)
  endif()
endif()

# Tools
add_executable(dbhdr tools/dbhdr/dbhdr.cpp)
target_include_directories(dbhdr PRIVATE include)
target_link_libraries(dbhdr PRIVATE scratchbird)
add_executable(page_dump tools/page_dump/page_dump.cpp)
target_include_directories(page_dump PRIVATE include)
target_link_libraries(page_dump PRIVATE scratchbird)
add_executable(fast_check tools/fast_check/fast_check.cpp)
target_include_directories(fast_check PRIVATE include)
target_link_libraries(fast_check PRIVATE scratchbird)
add_executable(bench_btree tools/bench/bench_btree.cpp)
target_include_directories(bench_btree PRIVATE include)
target_link_libraries(bench_btree PRIVATE scratchbird)
add_executable(tx_inspect tools/tx_inspect/tx_inspect.cpp)
target_include_directories(tx_inspect PRIVATE include)
target_link_libraries(tx_inspect PRIVATE scratchbird)
add_executable(txn_update_visibility_tests
               tests/txn_update_visibility_tests.cpp)
target_include_directories(txn_update_visibility_tests PRIVATE include)
target_link_libraries(txn_update_visibility_tests PRIVATE scratchbird gtest
                                                          gtest_main)
add_test(NAME txn_update_visibility_tests COMMAND txn_update_visibility_tests)
add_executable(txn_rr_visibility_tests tests/txn_rr_visibility_tests.cpp)
target_include_directories(txn_rr_visibility_tests PRIVATE include)
target_link_libraries(txn_rr_visibility_tests PRIVATE scratchbird gtest
                                                      gtest_main)
add_test(NAME txn_rr_visibility_tests COMMAND txn_rr_visibility_tests)
add_executable(txn_deadlock_tests tests/txn_deadlock_tests.cpp)
target_include_directories(txn_deadlock_tests PRIVATE include)
target_link_libraries(txn_deadlock_tests PRIVATE scratchbird gtest gtest_main)
add_test(NAME txn_deadlock_tests COMMAND txn_deadlock_tests)
add_executable(txn_prepare_scaffold_tests tests/txn_prepare_scaffold_tests.cpp)
target_include_directories(txn_prepare_scaffold_tests PRIVATE include)
target_link_libraries(txn_prepare_scaffold_tests PRIVATE scratchbird gtest
                                                         gtest_main)
add_test(NAME txn_prepare_scaffold_tests COMMAND txn_prepare_scaffold_tests)
add_executable(heap_page_tests tests/heap_page_tests.cpp)
target_include_directories(heap_page_tests PRIVATE include)
target_link_libraries(heap_page_tests PRIVATE scratchbird)
add_test(NAME heap_page_tests COMMAND heap_page_tests)
add_executable(heap_tuple_tests tests/heap_tuple_tests.cpp)
target_include_directories(heap_tuple_tests PRIVATE include)
target_link_libraries(heap_tuple_tests PRIVATE scratchbird)
add_test(NAME heap_tuple_tests COMMAND heap_tuple_tests)
add_executable(heap_root_tests tests/heap_root_tests.cpp)
target_include_directories(heap_root_tests PRIVATE include)
target_link_libraries(heap_root_tests PRIVATE scratchbird)
add_test(NAME heap_root_tests COMMAND heap_root_tests)
add_executable(heap_rel_basic_tests tests/heap_rel_basic_tests.cpp)
target_include_directories(heap_rel_basic_tests PRIVATE include)
target_link_libraries(heap_rel_basic_tests PRIVATE scratchbird)
add_test(NAME heap_rel_basic_tests COMMAND heap_rel_basic_tests)
add_executable(heap_overflow_tests tests/heap_overflow_tests.cpp)
target_include_directories(heap_overflow_tests PRIVATE include)
target_link_libraries(heap_overflow_tests PRIVATE scratchbird)
add_test(NAME heap_overflow_tests COMMAND heap_overflow_tests)
add_executable(heap_scan_tests tests/heap_scan_tests.cpp)
target_include_directories(heap_scan_tests PRIVATE include)
target_link_libraries(heap_scan_tests PRIVATE scratchbird)
add_test(NAME heap_scan_tests COMMAND heap_scan_tests)
add_executable(heap_rel_null_tests tests/heap_rel_null_tests.cpp)
target_include_directories(heap_rel_null_tests PRIVATE include)
target_link_libraries(heap_rel_null_tests PRIVATE scratchbird)
add_test(NAME heap_rel_null_tests COMMAND heap_rel_null_tests)
add_executable(heap_rel_fixed_tests tests/heap_rel_fixed_tests.cpp)
target_include_directories(heap_rel_fixed_tests PRIVATE include)
target_link_libraries(heap_rel_fixed_tests PRIVATE scratchbird)
add_test(NAME heap_rel_fixed_tests COMMAND heap_rel_fixed_tests)
add_executable(heap_varlen_short_tests tests/heap_varlen_short_tests.cpp)
target_include_directories(heap_varlen_short_tests PRIVATE include)
target_link_libraries(heap_varlen_short_tests PRIVATE scratchbird)
add_test(NAME heap_varlen_short_tests COMMAND heap_varlen_short_tests)
add_executable(heap_corruption_tests tests/heap_corruption_tests.cpp)
target_include_directories(heap_corruption_tests PRIVATE include)
target_link_libraries(heap_corruption_tests PRIVATE scratchbird)
add_test(NAME heap_corruption_tests COMMAND heap_corruption_tests)
add_executable(heap_fuzz_tests tests/heap_fuzz_tests.cpp)
target_include_directories(heap_fuzz_tests PRIVATE include)
target_link_libraries(heap_fuzz_tests PRIVATE scratchbird)
add_test(NAME heap_fuzz_tests COMMAND heap_fuzz_tests)
add_executable(heap_many_rows_tests tests/heap_many_rows_tests.cpp)
target_include_directories(heap_many_rows_tests PRIVATE include)
target_link_libraries(heap_many_rows_tests PRIVATE scratchbird)
add_test(NAME heap_many_rows_tests COMMAND heap_many_rows_tests)
add_executable(heap_truncate_drop_tests tests/heap_truncate_drop_tests.cpp)
target_include_directories(heap_truncate_drop_tests PRIVATE include)
target_link_libraries(heap_truncate_drop_tests PRIVATE scratchbird)
add_test(NAME heap_truncate_drop_tests COMMAND heap_truncate_drop_tests)
add_executable(space_create_tests tests/space_create_tests.cpp)
target_include_directories(space_create_tests PRIVATE include)
target_link_libraries(space_create_tests PRIVATE scratchbird)
add_test(NAME space_create_tests COMMAND space_create_tests)
add_executable(txn_visibility_tests tests/txn_visibility_tests.cpp)
target_include_directories(txn_visibility_tests PRIVATE include)
target_link_libraries(txn_visibility_tests PRIVATE scratchbird)
add_test(NAME txn_visibility_tests COMMAND txn_visibility_tests)
add_executable(txn_delete_visibility_tests
               tests/txn_delete_visibility_tests.cpp)
target_include_directories(txn_delete_visibility_tests PRIVATE include)
target_link_libraries(txn_delete_visibility_tests PRIVATE scratchbird)
add_test(NAME txn_delete_visibility_tests COMMAND txn_delete_visibility_tests)
add_executable(heap_check tools/heap_check/heap_check.cpp)
target_include_directories(heap_check PRIVATE include)
target_link_libraries(heap_check PRIVATE scratchbird)
add_executable(space_info tools/space_info/space_info.cpp)
target_include_directories(space_info PRIVATE include)
target_link_libraries(space_info PRIVATE scratchbird)

add_executable(isql tools/isql/isql.cpp tools/isql/meta.cpp)
target_include_directories(isql PRIVATE include tools)
target_link_libraries(isql PRIVATE scratchbird)
add_executable(catalog_inspect tools/catalog_inspect/catalog_inspect.cpp)
target_include_directories(catalog_inspect PRIVATE include)
target_link_libraries(catalog_inspect PRIVATE scratchbird)

# Tool tests
if(SCRATCHBIRD_BUILD_TESTS)
  if(BUILD_TESTING)
    add_executable(tools_dbhdr_tests tests/tools_dbhdr_tests.cpp)
    target_link_libraries(tools_dbhdr_tests PRIVATE scratchbird)
    add_test(NAME tools_dbhdr_tests COMMAND tools_dbhdr_tests)
  endif()
endif()
