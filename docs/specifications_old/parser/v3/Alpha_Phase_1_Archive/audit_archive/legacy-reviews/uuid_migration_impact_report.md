# UUID Migration Impact Report

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date:** 2025-09-09

## 1. Introduction

This report assesses the impact and difficulty of migrating the database's identification system from `uint32_t` integers to UUIDs for `schema_id` and `table_id`. The original specification intended for UUIDs to be used from the beginning, and this change is necessary to align the implementation with the design.

## 2. Difficulty Assessment

This is a **high-difficulty** task. The change is pervasive and affects multiple layers of the system, from the on-disk format to the public API of the core modules. The effort required to implement and test this change is significant.

## 3. Scope of Changes

The migration to UUIDs will affect the following areas:

### 3.1. Core Engine

*   **`catalog_manager`**: This is the most affected module.
    *   The in-memory data structures (`SchemaInfo`, `TableInfo`, `ColumnInfo`) must be updated to use UUIDs.
    *   The on-disk record structures (`SchemaRecord`, `TableRecord`, `ColumnRecord`) must be updated, which constitutes a change to the physical storage format.
    *   All function signatures that accept or return IDs must be changed.
    *   The ID generation logic must be replaced with a UUID generator.
*   **`storage_engine`**: The storage engine uses `table_id` to identify tables. All relevant methods will need to be updated.
*   **`toast`**: The TOAST manager uses `table_id` to name and manage TOAST tables. The naming convention will need to be updated to handle UUIDs.
*   **`heap_page`**: The heap page implementation uses `table_id` for TOAST operations and will need to be updated.

### 3.2. On-Disk Format

The size of the ID fields in the catalog's on-disk format will increase from 4 bytes (`uint32_t`) to 16 bytes (UUID). This is a breaking change to the file format. For the purpose of this project, we will assume that we can create a new database from scratch and do not need to support migrating existing data.

### 3.3. Testing

A significant portion of the unit tests will need to be updated. The current tests are written with the assumption of sequential integer IDs. The changes will include:

*   Updating test data to use UUIDs.
*   Modifying test logic to handle UUIDs instead of integers.
*   Updating assertions to compare UUIDs.

The affected test files include:
*   `test_catalog_manager.cpp`
*   `test_storage_engine.cpp`
*   `test_toast.cpp`
*   `test_heap_toast_integration.cpp`
*   `test_security_issues.cpp`

### 3.4. Documentation

All design documents, specifications, and code comments that refer to `schema_id` and `table_id` as `uint32_t` will need to be updated to reflect the change to UUIDs.

## 4. Proposed Implementation Plan

1.  **Introduce a `using ID = UuidV7Bytes;` typedef.** This will be used for all schema and table IDs to make the code more readable and maintainable.
2.  **Update Data Structures:** Modify all relevant data structures in the `catalog_manager` and other modules to use the new `ID` type.
3.  **Update Function Signatures:** Change all function signatures that use `schema_id` or `table_id` to use the `ID` type.
4.  **Update ID Generation:** Replace the current sequential ID generation logic with calls to the existing UUIDv7 generator.
5.  **Update Tests:** Systematically update all affected tests to use UUIDs.
6.  **Update Documentation:** Review and update all documentation to reflect the change.

## 5. Conclusion

While the migration to UUIDs is a complex and time-consuming task, it is essential for aligning the implementation with the original design and for building a robust and scalable database system. The change will improve the uniqueness and security of the database's identification system.
