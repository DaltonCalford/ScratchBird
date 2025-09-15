# UUID Migration Implementation Plan

**Date:** 2025-09-09

This document outlines a step-by-step plan for migrating the database's identification system from `uint32_t` integers to UUIDs. This plan is designed to be executed over multiple sessions.

## Phase 1: Core Data Structure and API Changes

The goal of this phase is to update all the core data structures and function signatures to use UUIDs instead of `uint32_t` for IDs.

*   **Step 1.1: Introduce `ID` typedef**
    *   **File:** `include/scratchbird/core/catalog_manager.h`
    *   **Action:** Introduce a `using ID = UuidV7Bytes;` typedef. Replace all instances of `uint32_t` for `schema_id` and `table_id` with this new `ID` type in the `SchemaInfo`, `TableInfo`, and `ColumnInfo` structs.

*   **Step 1.2: Update On-Disk Format**
    *   **File:** `src/core/catalog_manager.cpp`
    *   **Action:** Update the on-disk record structures (`SchemaRecord`, `TableRecord`, `ColumnRecord`) to use the new `ID` type. This will involve changing the field types from `uint32_t` to a 16-byte array for storing the UUID.

*   **Step 1.3: Update Catalog Manager API**
    *   **File:** `include/scratchbird/core/catalog_manager.h`
    *   **Action:** Update all function signatures that accept or return `schema_id` or `table_id` to use the new `ID` type.

*   **Step 1.4: Implement Catalog Manager API Changes**
    *   **File:** `src/core/catalog_manager.cpp`
    *   **Action:** Implement the changes to the function signatures from the previous step. This will involve updating the method bodies to handle UUIDs.

*   **Step 1.5: Update ID Generation**
    *   **File:** `src/core/catalog_manager.cpp`
    *   **Action:** Replace the sequential ID generation logic (`next_schema_id_++`, `next_table_id_++`) with calls to the UUIDv7 generator.

*   **Step 1.6: Update Storage Engine API**
    *   **File:** `include/scratchbird/core/storage_engine.h`
    *   **Action:** Update all function signatures that use `table_id` to use the new `ID` type.

*   **Step 1.7: Implement Storage Engine API Changes**
    *   **File:** `src/core/storage_engine.cpp`
    *   **Action:** Implement the changes to the function signatures from the previous step.

*   **Step 1.8: Update TOAST API**
    *   **File:** `include/scratchbird/core/toast.h`
    *   **Action:** Update all function signatures that use `table_id` to use the new `ID` type.

*   **Step 1.9: Implement TOAST API Changes**
    *   **File:** `src/core/toast.cpp`
    *   **Action:** Implement the changes to the function signatures from the previous step. The TOAST table naming convention will need to be updated to handle UUIDs.

*   **Step 1.10: Update Heap Page API**
    *   **File:** `include/scratchbird/core/heap_page.h`
    *   **Action:** Update all function signatures that use `table_id` to use the new `ID` type.

*   **Step 1.11: Implement Heap Page API Changes**
    *   **File:** `src/core/heap_page.cpp`
    *   **Action:** Implement the changes to the function signatures from the previous step.

## Phase 2: Test-Driven Migration

The goal of this phase is to update all tests to use UUIDs and to ensure that the changes from Phase 1 are correct.

*   **Step 2.1: Update Catalog Manager Tests**
    *   **File:** `tests/unit/test_catalog_manager.cpp`
    *   **Action:** For each test, update it to use UUIDs. This will likely involve generating UUIDs for testing and updating assertions.

*   **Step 2.2: Update Storage Engine Tests**
    *   **File:** `tests/unit/test_storage_engine.cpp`
    *   **Action:** Update all tests to use UUIDs.

*   **Step 2.3: Update TOAST Tests**
    *   **File:** `tests/unit/test_toast.cpp`
    *   **Action:** Update all tests to use UUIDs.

*   **Step 2.4: Update Heap TOAST Integration Tests**
    *   **File:** `tests/unit/test_heap_toast_integration.cpp`
    *   **Action:** Update all tests to use UUIDs.

*   **Step 2.5: Update Security Issues Tests**
    *   **File:** `tests/unit/test_security_issues.cpp`
    *   **Action:** Update all tests to use UUIDs.

## Phase 3: Documentation

The goal of this phase is to update all documentation to reflect the change to UUIDs.

*   **Step 3.1: Update Design Documents and Specifications**
    *   **Directory:** `docs`
    *   **Action:** Review and update all relevant documentation to reflect the change to UUIDs.

*   **Step 3.2: Update Code Comments**
    *   **Action:** Review and update all code comments that refer to `schema_id` and `table_id` as `uint32_t`.
