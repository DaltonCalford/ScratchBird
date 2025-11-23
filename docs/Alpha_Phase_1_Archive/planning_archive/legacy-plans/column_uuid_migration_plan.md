# Column UUID Migration Implementation Plan

**Date:** 2025-09-16

**Objective:** This document provides a detailed, step-by-step plan to refactor the codebase to use UUIDs for column identifiers, based on the findings from the `comprehensive_uuid_audit_2025_09_16.md` report.

---

## Phase 1: Core Data Structure Refactoring

This phase focuses on updating the primary in-memory and on-disk data structures that define columns and their relationships to other objects.

### 1.1: Modify In-Memory Column & Index Structs
- **File:** `include/scratchbird/core/catalog_manager.h`
- **Task:** Change all integer-based column identifiers to the `ID` type (`UuidV7Bytes`).
- **Details:**
    - [ ] In `struct ColumnInfo`, change the type of the `column_id` field from `uint16_t` to `ID`.
    - [ ] In `struct IndexInfo`, change the type of the `column_ids` field from `std::vector<uint16_t>` to `std::vector<ID>`.

### 1.2: Modify On-Disk Column & Index Structs
- **File:** `src/core/catalog_manager.cpp`
- **Task:** Update the on-disk record structures to match the in-memory changes. This is a file-format-breaking change.
- **Details:**
    - [ ] In `struct ColumnRecord`, change the type of the `column_id` field from `uint16_t` to `ID`.
    - [ ] In `struct IndexRecord`, change the type of the `column_ids` array from `uint16_t[16]` to `ID[16]`.

---

## Phase 2: Logic and Implementation Refactoring

This phase addresses the code in the `CatalogManager` that handles the creation and retrieval of the modified data structures.

### 2.1: Update Column Creation Logic
- **File:** `src/core/catalog_manager.cpp`
- **Task:** Modify the `create_table` and `write_column_records` methods to assign a new UUID to each column upon creation.
- **Details:**
    - [ ] In `create_table`, when iterating through the input `std::vector<ColumnInfo>`, use a `UuidV7Generator` to generate and assign a new UUID to each `ColumnInfo` object's `column_id` field before it is stored.
    - [ ] Ensure the `write_column_records` function correctly writes the new `ID` type to the on-disk `ColumnRecord` structure.

### 2.2: Update Index Creation Logic
- **File:** `src/core/catalog_manager.cpp`
- **Task:** Modify the `create_index` method to correctly resolve column names to their new UUIDs.
- **Details:**
    - [ ] In `create_index`, the logic for resolving column names to IDs will still work, but the `column_ids` vector it populates is now of type `std::vector<ID>`. Ensure the resolved `column_id` (`ID`) is correctly stored.
    - [ ] Update the `write_index_record` function to handle writing an array of `ID`s to the on-disk `IndexRecord` structure.

### 2.3: Update Data Retrieval and Conversion Logic
- **File:** `src/core/catalog_manager.cpp`
- **Task:** Update the helper functions that read records from disk to correctly handle the new `ID` types.
- **Details:**
    - [ ] In `read_column_records`, update the `converter` lambda to correctly copy the `ID` from `ColumnRecord` to `ColumnInfo`.
    - [ ] In `read_index_records`, update the `converter` lambda to correctly copy the array of `ID`s from `IndexRecord` to the `std::vector<ID>` in `IndexInfo`.

---

## Phase 3: Unit Test Overhaul

This phase ensures that the unit tests are updated to reflect the data structure and logic changes and to properly validate the new implementation.

### 3.1: Update Catalog Manager Unit Tests
- **File:** `tests/unit/test_catalog_manager.cpp`
- **Task:** Modify existing tests to work with and validate column UUIDs.
- **Details:**
    - [ ] In `TEST_F(CatalogManagerTest, CreateAndGetTable)`, when defining the `columns` vector, the `column_id` field should be left in its default-initialized state, as the `create_table` function is now responsible for generating it.
    - [ ] In `TEST_F(CatalogManagerTest, GetColumns)`, after retrieving the columns, add an assertion to verify that each `retrieved_columns[i].column_id` is not null or empty (i.e., that a UUID was properly generated and assigned).
    - [ ] In `TEST_F(CatalogManagerTest, GetColumnByName)`, add an assertion to verify that the retrieved `col_info.column_id` is a valid, non-null UUID.
    - [ ] Add a new test case, `TEST_F(CatalogManagerTest, CreateAndGetIndex)`, to specifically test the creation and retrieval of an index with UUID-based column IDs.

---

## Phase 4: Verification

This phase ensures the project is stable and the changes are correct.

### 4.1: Compile the Project
- **Task:** Build the entire ScratchBird project.
- **Details:**
    - [ ] Run `cmake ..` and `make` from the `build` directory.
    - [ ] Ensure there are no compilation errors.

### 4.2: Run All Tests
- **Task:** Execute all unit and integration tests.
- **Details:**
    - [ ] Run `ctest --output-on-failure` from the `build` directory.
    - [ ] Ensure all tests pass, especially the updated `CatalogManagerTest` cases.
