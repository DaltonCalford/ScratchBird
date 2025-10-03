# ID Alias Remediation Plan

**Date:** 2025-09-16
**Author:** Agent C (Test Writer)
**Source:** Codebase analysis of `UuidV7Bytes` usage.

**Objective:** This document outlines the tasks required to standardize the codebase by replacing all usages of the concrete type `UuidV7Bytes` with its established alias, `ID`, for SQL object identifiers. This will improve code consistency and readability.

---

## Phase 1: Core Header Refactoring

This phase focuses on updating the header files in the core engine to use the `ID` alias.

### 1.1: Refactor `btree.h`
- **File:** `include/scratchbird/core/btree.h`
- **Task:** Replace all instances of `UuidV7Bytes` with `ID`.
- **Details:**
    - [ ] Change `btr_index_uuid` from `UuidV7Bytes` to `ID`.
    - [ ] Change `btr_table_uuid` from `UuidV7Bytes` to `ID`.
    - [ ] Change `idx_uuid` from `UuidV7Bytes` to `ID`.
    - [ ] Change `idx_table_uuid` from `UuidV7Bytes` to `ID`.

### 1.2: Refactor `btree_page.h`
- **File:** `include/scratchbird/core/btree_page.h`
- **Task:** Replace `UuidV7Bytes` in the `initialize` method signature with `ID`.
- **Details:**
    - [ ] Change `const UuidV7Bytes &index_uuid` to `const ID &index_uuid`.
    - [ ] Change `const UuidV7Bytes &table_uuid` to `const ID &table_uuid`.

### 1.3: Refactor `database.h`
- **File:** `include/scratchbird/core/database.h`
- **Task:** Replace all instances of `UuidV7Bytes` with `ID`.
- **Details:**
    - [ ] Change the return type of the `uuid()` method to `const ID &`.
    - [ ] Change the type of the `db_uuid_` member to `ID`.
    - [ ] Change the `db_uuid` parameter in `create_catalog_page` to `const ID &`.
    - [ ] Change the `db_uuid` parameter in `create_fsm_page` to `const ID &`.

### 1.4: Refactor `heap_page.h`
- **File:** `include/scratchbird/core/heap_page.h`
- **Task:** Replace all instances of `UuidV7Bytes` with `ID`.
- **Details:**
    - [ ] Change the `table_id` parameter in the `HeapPage` constructor to `const ID &`.
    - [ ] Change the type of the `table_id_` member to `ID`.

---

## Phase 2: Core Source File Refactoring

This phase applies the same standardization to the corresponding implementation files.

### 2.1: Refactor `btree_page.cpp`
- **File:** `src/core/btree_page.cpp`
- **Task:** Update the `initialize` method signature to use `ID`.
- **Details:**
    - [ ] Change `const UuidV7Bytes &index_uuid` to `const ID &index_uuid`.
    - [ ] Change `const UuidV7Bytes &table_uuid` to `const ID &table_uuid`.

### 2.2: Refactor `database.cpp`
- **File:** `src/core/database.cpp`
- **Task:** Replace all instances of `UuidV7Bytes` with `ID`.
- **Details:**
    - [ ] Change `db_uuid` parameter in `create_catalog_page` to `const ID &`.
    - [ ] Change `schema_uuid` local variable to `ID`.
    - [ ] Change `db_uuid` parameter in `create_fsm_page` to `const ID &`.
    - [ ] Change `db_uuid` local variable in `create` to `ID`.
    - [ ] Change `db_uuid` local variable in `open` to `ID`.

### 2.3: Refactor `heap_page.cpp`
- **File:** `src/core/heap_page.cpp`
- **Task:** Update the `HeapPage` constructor signature to use `ID`.
- **Details:**
    - [ ] Change `const UuidV7Bytes &table_id` to `const ID &table_id`.

### 2.4: Refactor `uuidv7.cpp`
- **File:** `src/core/uuidv7.cpp`
- **Task:** Update the return type of the generation function.
- **Details:**
    - [ ] Change the return type of `generate_uuid_v7` to `ID`.
    - [ ] Change the type of the `out` local variable to `ID`.

---

## Phase 3: Test File Refactoring

This phase ensures the test suite is consistent with the new standard.

### 3.1: Refactor `btree_page_test.cpp`
- **File:** `tests/unit/btree_page_test.cpp`
- **Task:** Replace all instances of `UuidV7Bytes` with `ID`.
- **Details:**
    - [ ] Change `index_uuid` and `table_uuid` local variables to `ID`.

### 3.2: Refactor `test_heap_page_toast_api.cpp`
- **File:** `tests/unit/test_heap_page_toast_api.cpp`
- **Task:** Replace `UuidV7Bytes` with `ID`.
- **Details:**
    - [ ] Change `table_id` local variable to `ID`.

---

## Phase 4: Application Code Refactoring

### 4.1: Refactor `main.cpp`
- **File:** `src/main.cpp`
- **Task:** Update the `print_uuid` function signature.
- **Details:**
    - [ ] Change `const UuidV7Bytes &uuid` to `const ID &uuid`.

---

## Phase 5: Verification

### 5.1: Compile the Project
- **Task:** Build the entire ScratchBird project from a clean build directory.
- **Details:**
    - [ ] Run `cmake ..` and `make` from the `build` directory.
    - [ ] The build must complete without any errors.

### 5.2: Run All Tests
- **Task:** Execute all unit and integration tests.
- **Details:**
    - [ ] Run `ctest --output-on-failure` from the `build` directory.
    - [ ] All existing tests must pass.
