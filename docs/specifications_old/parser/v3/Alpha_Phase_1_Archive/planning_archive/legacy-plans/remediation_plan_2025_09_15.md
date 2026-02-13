# UUID Migration Remediation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date:** 2025-09-15

**Objective:** This document outlines the detailed tasks required to complete the migration from `uint32_t` to `UuidV7Bytes` for all table and schema identifiers. This plan is based on the findings from the deep technical analysis report dated 2025-09-15.

---

## Phase 1: Core Data Structure and Logic Refactoring

This phase addresses the most critical flaws in the core engine's data structures and logic.

### 1.1: Correct Catalog Manager's Internal `TableInfo` Struct
- **File:** `include/scratchbird/core/catalog_manager.h`
- **Task:** Modify the `TableInfo` struct to use `UuidV7Bytes` for identifiers.
- **Details:**
    - [x] Change the type of the `table_id` field from `uint32_t` to `UuidV7Bytes`.
    - [x] Change the type of the `schema_id` field from `uint32_t` to `UuidV7Bytes`.

### 1.2: Refactor Catalog Manager Implementation
- **File:** `src/core/catalog_manager.cpp`
- **Task:** Update the `CatalogManager`'s implementation to natively support UUIDs, removing all integer-based logic for identifiers.
- **Details:**
    - [x] Locate the primary internal data structure for storing table metadata (e.g., a map like `table_map_by_int_id_`). Change its key from `uint32_t` to `UuidV7Bytes`.
    - [x] Refactor the `CreateTable` method to work with the updated `TableInfo` struct and the UUID-keyed map.
    - [x] Refactor the `GetTable` (and any similar lookup methods) to perform direct lookups using the `UuidV7Bytes` key. Remove any inefficient loops that iterate and convert values.
    - [x] Refactor the `DropTable` method to use `UuidV7Bytes` for lookups and deletion.
    - [x] Audit all other methods within the file for any logic that assumes integer-based identifiers and update them accordingly.

---

## Phase 2: Unit Test Overhaul

This phase focuses on correcting the unit tests to ensure they accurately validate the UUID implementation.

### 2.1: Update Catalog Manager Unit Tests
- **File:** `tests/unit/test_catalog_manager.cpp`
- **Task:** Eliminate the use of casted integer literals for identifiers and use a real UUID generator.
- **Details:**
    - [x] In each relevant test case or fixture, create an instance of `UuidV7Generator`.
    - [x] Replace all occurrences of `static_cast<UuidV7Bytes>(1)`, `static_cast<UuidV7Bytes>(2)`, etc., with calls to the `uuid_gen.Generate()` method to create authentic UUIDs.
    - [x] Update all assertions and lookup calls to use the generated UUID variables.
    - [x] Ensure that tests for failure scenarios (e.g., looking up a non-existent table) also use properly generated UUIDs.

---

## Phase 3: Documentation Audit and Correction

This phase ensures that all project documentation is consistent with the UUID-based design.

### 3.1: Correct Code Example in `HEAP_TOAST_INTEGRATION.md`
- **File:** `/docs/specifications/parser/v3/HEAP_TOAST_INTEGRATION.md`
- **Task:** Update the outdated code example.
- **Details:**
    - [x] Locate the pseudo-code example under the "Heap Page API Example" section.
    - [x] Change the line `uint32_t table_id = 100;` to `UuidV7Bytes table_id = uuid_gen.Generate();`. You may need to add a comment indicating that `uuid_gen` is a `UuidV7Generator` instance.

### 3.2: Perform a Global Audit of Documentation
- **Files:** All files within `docs/` and `project/` directories.
- **Task:** Find and correct any other remaining instances of integer-based identifiers in documentation and code examples.
- **Details:**
    - [x] Perform a case-sensitive search for "uint32_t" across all markdown files.
    - [x] For each result, analyze the context. If it refers to a table, schema, or other database object identifier, replace it with `UuidV7Bytes`.
    - [x] Search for common variable names like `table_id`, `schema_id`, etc., in code blocks to find instances that might have been missed.
    - [x] Pay special attention to specifications, design documents, and implementation plans.

---

## Phase 4: Process Improvement

This phase implements a process change to prevent similar issues in the future.

### 4.1: Enforce Mandatory Peer Review
- **Applies to:** All future code and documentation changes related to this remediation.
- **Task:** Institute a formal peer review process.
- **Details:**
    - [x] All pull requests or commits for the tasks listed above must be reviewed and approved by at least one other developer on the team before being merged.
    - [x] The reviewer is responsible for verifying that the changes completely address the specific task and do not introduce new inconsistencies.
