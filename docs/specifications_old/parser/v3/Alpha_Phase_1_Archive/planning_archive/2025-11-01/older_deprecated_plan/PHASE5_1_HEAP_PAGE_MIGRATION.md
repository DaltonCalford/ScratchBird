# Phase 5.1: Heap Page Migration - Design Document

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Document Status**: DESIGN COMPLETE
**Version**: 1.0
**Date**: 2025-10-21
**Author**: Development Team
**Related Documents**:
- [TABLESPACE_IMPLEMENTATION_PLAN.md](./TABLESPACE_IMPLEMENTATION_PLAN.md)
- [PHASE5_FULL_IMPLEMENTATION_PLAN.md](./PHASE5_FULL_IMPLEMENTATION_PLAN.md)

---

## Executive Summary

This document provides the detailed design for Phase 5.1: Heap Page Migration, which implements the actual data movement logic for offline table migration. Phase 4 completed the infrastructure (parser, bytecode, progress tracking, batch processing framework). Phase 5.1 replaces the STUB code with real page enumeration, copying, TID remapping, TOAST handling, and rollback.

**Total Estimated Time**: 35-50 hours
**Dependencies**: Phase 4 complete (all 6 tasks)
**Target**: Production-ready OFFLINE table migration

---

## Table of Contents

1. [Background and Context](#background-and-context)
2. [Architecture Overview](#architecture-overview)
3. [Task 5.1.1: Heap Page Enumeration](#task-511-heap-page-enumeration)
4. [Task 5.1.2: Page Copying with TID Remapping](#task-512-page-copying-with-tid-remapping)
5. [Task 5.1.3: TOAST Handling](#task-513-toast-handling)
6. [Task 5.1.4: Transaction Rollback](#task-514-transaction-rollback)
7. [Integration Points](#integration-points)
8. [Error Handling Strategy](#error-handling-strategy)
9. [Testing Strategy](#testing-strategy)
10. [Performance Considerations](#performance-considerations)

---

## Background and Context

### What Works Now (Phase 4)

Phase 4 implemented the complete infrastructure for table migration:

- **Parser**: `ALTER TABLE ... SET TABLESPACE [ONLINE]` syntax
- **Bytecode**: `OP_ALTER_TABLE_SET_TABLESPACE` opcode and generation
- **Executor**: `executeAlterTableSetTablespace()` method
- **Catalog Manager**: `moveTableToTablespace()` framework with:
  - Table/tablespace validation
  - Batch processing (MAX_BATCH_SIZE_PAGES = 1000)
  - Progress tracking (callbacks every 100 pages)
  - Memory tracking (~8-10 MB per batch)
  - Index TID update infrastructure (STUB)
  - Transaction strategy (single transaction)

### What's Missing (Phase 5.1)

The current implementation has STUB code in these areas:

```cpp
// Line ~2676 in catalog_manager.cpp
uint32_t total_pages = 100; // STUB: Replace with actual page enumeration

// Lines 2726-2822: STUB simulation loop
for (uint32_t batch_start = 0; batch_start < total_pages; batch_start += batch_size)
{
    // Simulate page copying with sleep
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
```

Phase 5.1 replaces these STUBs with:
1. Real page enumeration using FSM or sequential scan
2. Actual page copying with memory-to-memory transfers
3. Tuple-level TID remapping for version chains
4. TOAST table migration (for large values)
5. Rollback logic for error recovery

---

## Architecture Overview

### Data Flow

```
moveTableToTablespace(table_id, target_ts_id, online, progress_callback)
    │
    ├─> Step 1: Validation (table exists, tablespace different)
    │
    ├─> Step 2: TOAST Handling (if table has TOAST)
    │       └─> Recursive call: moveTableToTablespace(toast_table_id, ...)
    │           └─> Build toast_tid_mapping
    │
    ├─> Step 3: Heap Page Enumeration
    │       └─> enumerateTablePages(table_id) → vector<GPID>
    │
    ├─> Step 4: Batch-Based Page Migration
    │       └─> For each batch (1000 pages max):
    │           ├─> For each page in batch:
    │           │   ├─> Pin source page (buffer_pool)
    │           │   ├─> Allocate target page (page_manager)
    │           │   ├─> Pin target page (buffer_pool)
    │           │   ├─> copyPageWithTIDRemapping(src, tgt, tid_mapping)
    │           │   ├─> Unpin source (clean)
    │           │   └─> Unpin target (dirty)
    │           ├─> Progress callback (every 100 pages)
    │           └─> Cancellation check
    │
    ├─> Step 5: Index TID Updates
    │       └─> updateIndexTIDs(table_id, tid_mapping)
    │
    ├─> Step 6: Catalog Update
    │       └─> table_info.tablespace_id = target_ts_id
    │
    ├─> Step 7: Free Old Pages
    │       └─> For each old_gpid in tid_mapping: freePageInTablespace()
    │
    └─> Step 8: Commit (or rollback on error)
```

### Key Data Structures

```cpp
// TID mapping: old GPID → new GPID
std::unordered_map<uint64_t, uint64_t> tid_mapping;

// Page list: All heap pages for this table
std::vector<GPID> heap_pages;

// TOAST mapping (if table has TOAST)
std::unordered_map<uint64_t, uint64_t> toast_tid_mapping;

// Progress tracking
uint32_t pages_copied = 0;
uint32_t total_pages = heap_pages.size();
```

### Memory Budget

Per-batch memory usage (batch_size = 1000 pages):
- Heap page data: 1000 pages × 8 KB = 8 MB
- TID mapping: 1000 entries × 32 bytes = 32 KB
- **Total: ~8-10 MB per batch**

This ensures constant memory usage regardless of table size.

---

## Task 5.1.1: Heap Page Enumeration

**Goal**: Find all heap pages belonging to a table for migration

**Estimated Time**: 4-6 hours

### 5.1.1.1: Problem Statement

The catalog manager needs to enumerate all heap pages for a given table. The current STUB uses `total_pages = 100`. We need to implement a real page enumeration algorithm.

### 5.1.1.2: Design Options

**Option A: Scan Free Space Map (FSM)**
- Iterate all pages in FSM for the source tablespace
- For each page, check if `PageHeader.table_id == target_table_id`
- PRO: Fast (no disk I/O for empty pages)
- CON: Requires pinning every allocated page to read header

**Option B: Sequential File Scan**
- Read tablespace file sequentially from page 0 to max_page
- For each page, parse PageHeader
- Filter pages where `page_type == HEAP_PAGE && table_id == target_table_id`
- PRO: Simple, no FSM dependency
- CON: Slow for large tablespaces (reads all pages)

**Option C: Catalog-Based (Future)**
- Store `root_page` and page list in catalog
- PRO: O(1) lookup
- CON: Requires catalog redesign (out of scope for Phase 5)

**DECISION: Use Option A (FSM-based)**
- FSM already tracks allocated pages
- Avoids reading empty pages
- Minimal disk I/O (only allocated pages)

### 5.1.1.3: Implementation

#### API Design

```cpp
// Add to CatalogManager (private helper)
auto enumerateTablePages(const ID &table_id,
                        std::vector<GPID> &pages_out,
                        ErrorContext *ctx = nullptr) -> Status;
```

#### Algorithm

```cpp
Status CatalogManager::enumerateTablePages(const ID &table_id,
                                           std::vector<GPID> &pages_out,
                                           ErrorContext *ctx)
{
    // 1. Get table info from catalog
    TableInfo table_info;
    Status status = getTable(table_id, table_info, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Table not found in catalog");
        return status;
    }

    uint16_t source_ts_id = table_info.tablespace_id;

    LOG_INFO(CATALOG, "Enumerating heap pages for table '%s' (ID %u) in tablespace %u",
             table_info.table_name.c_str(), table_id, source_ts_id);

    // 2. Access FSM for source tablespace
    // Note: FSM is owned by PageManager, may need to add API
    // PageManager::getTablePages(tablespace_id, table_id, vector<GPID>&)

    // For now, assume PageManager exposes FSM or we add helper method
    // Alternative: Scan via buffer pool

    // 3. Scan all allocated pages in tablespace
    // (Assuming FSM provides iterator or list of allocated pages)

    // Option: Scan via PageManager's internal FSM
    // Get page manager reference
    PageManager *page_mgr = db_->page_manager();

    // Get all allocated pages in this tablespace
    std::vector<GPID> candidate_pages;
    status = page_mgr->getAllocatedPages(source_ts_id, candidate_pages, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to get allocated pages from FSM");
        return status;
    }

    LOG_INFO(CATALOG, "Found %zu allocated pages in tablespace %u",
             candidate_pages.size(), source_ts_id);

    // 4. Filter for heap pages belonging to this table
    uint32_t pages_scanned = 0;
    for (GPID gpid : candidate_pages)
    {
        // Pin page to read header
        void *page_buffer;
        status = db_->buffer_pool()->pinPageGlobal(gpid, &page_buffer, ctx);
        if (status != Status::OK)
        {
            LOG_WARNING(CATALOG, "Failed to pin page %lu during enumeration", gpid);
            continue; // Skip page
        }

        // Read page header
        const PageHeader *header = static_cast<const PageHeader*>(page_buffer);

        // Check if this is a heap page for our table
        bool is_target_page = (header->page_type == PageType::HEAP_PAGE &&
                              header->table_id == table_id);

        // Unpin page (not modified)
        db_->buffer_pool()->unpinPageGlobal(gpid, false, ctx);

        if (is_target_page)
        {
            pages_out.push_back(gpid);
        }

        pages_scanned++;
    }

    LOG_INFO(CATALOG, "Enumerated %zu heap pages (scanned %u allocated pages)",
             pages_out.size(), pages_scanned);

    return Status::OK;
}
```

### 5.1.1.4: Edge Cases

1. **Empty Table**: `pages_out.size() == 0`
   - Migration should succeed immediately (no pages to copy)
   - Update catalog only

2. **Fragmented Table**: Pages non-contiguous
   - Algorithm handles naturally (scans all allocated pages)

3. **Table with TOAST**: Separate TOAST table
   - Main algorithm only returns heap pages for main table
   - TOAST pages enumerated separately (Task 5.1.3)

4. **Concurrent Page Allocation**: Pages added during enumeration
   - OFFLINE migration holds EXCLUSIVE lock, prevents this
   - ONLINE migration (Phase 5.4) handles with delta log

### 5.1.1.5: PageManager API Extension

The algorithm assumes `PageManager::getAllocatedPages()` exists. If not, we need to add it:

```cpp
// Add to PageManager class
auto getAllocatedPages(uint16_t tablespace_id,
                      std::vector<GPID> &pages_out,
                      ErrorContext *ctx = nullptr) -> Status;
```

**Implementation** (in page_manager.cpp):
```cpp
Status PageManager::getAllocatedPages(uint16_t tablespace_id,
                                     std::vector<GPID> &pages_out,
                                     ErrorContext *ctx)
{
    // Access FSM cache for this tablespace
    auto it = fsm_cache_.find(tablespace_id);
    if (it == fsm_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tablespace not found in FSM");
        return Status::NOT_FOUND;
    }

    const auto &fsm_map = it->second;

    // Extract all GPIDs with free_space > 0 (allocated pages)
    for (const auto &[gpid, fsm_entry] : fsm_map)
    {
        // Note: FSM tracks pages with free space, not all allocated pages
        // May need to scan all pages in tablespace file instead
        // OR: Use max_page counter and check each page
        pages_out.push_back(gpid);
    }

    return Status::OK;
}
```

**Alternative**: Scan tablespace file page-by-page (Option B from design options).

### 5.1.1.6: Files to Modify

- `include/scratchbird/core/catalog_manager.h`: Add `enumerateTablePages()` declaration
- `src/core/catalog_manager.cpp`: Implement `enumerateTablePages()`
- `include/scratchbird/core/page_manager.h`: Add `getAllocatedPages()` declaration (if needed)
- `src/core/page_manager.cpp`: Implement `getAllocatedPages()` (if needed)
- `src/core/catalog_manager.cpp`: Replace `total_pages = 100` with real enumeration call

### 5.1.1.7: Testing

```cpp
// Test Case 1: Empty table
CREATE TABLE empty_table (id INT);
ALTER TABLE empty_table SET TABLESPACE ts;
// Expected: 0 pages enumerated, migration succeeds

// Test Case 2: Single-page table
CREATE TABLE small_table (id INT);
INSERT INTO small_table VALUES (1), (2), (3);
ALTER TABLE small_table SET TABLESPACE ts;
// Expected: 1 page enumerated, 3 rows migrated

// Test Case 3: Multi-page table
CREATE TABLE large_table (id INT, data VARCHAR(1000));
INSERT INTO large_table SELECT i, repeat('x', 1000) FROM generate_series(1, 100) i;
ALTER TABLE large_table SET TABLESPACE ts;
// Expected: ~13 pages enumerated (8 rows/page × 8KB pages)

// Test Case 4: Table with gaps (deleted pages)
CREATE TABLE fragmented (id INT);
INSERT INTO fragmented SELECT i FROM generate_series(1, 10000) i;
DELETE FROM fragmented WHERE id % 2 = 0; -- Delete half
VACUUM; -- May free some pages
ALTER TABLE fragmented SET TABLESPACE ts;
// Expected: Only allocated pages enumerated
```

---

## Task 5.1.2: Page Copying with TID Remapping

**Goal**: Copy heap pages from source to target tablespace, updating all TID references

**Estimated Time**: 8-12 hours

### 5.1.2.1: Problem Statement

The current implementation simulates page copying with `std::this_thread::sleep_for()`. We need to implement real page copying that:
1. Reads source page from buffer pool
2. Allocates new page in target tablespace
3. Copies page data byte-by-byte
4. Updates all TID references in tuple headers
5. Updates page header with new GPID
6. Recalculates checksum

### 5.1.2.2: TID References in Heap Pages

Each heap page contains tuples with the following TID references:

**TupleHeader Structure** (from tid.h):
```cpp
struct TupleHeader
{
    uint32_t xmin;           // Transaction ID (created)
    uint32_t xmax;           // Transaction ID (deleted/updated)
    uint64_t ctid_gpid;      // Current TID: GPID of this tuple
    uint16_t ctid_slot;      // Current TID: Slot within page
    uint64_t back_version_gpid; // Previous version GPID (MVCC)
    uint16_t back_version_slot; // Previous version slot
    // ... other fields
};
```

**TIDs to Update**:
1. `ctid_gpid`: Must be updated to new GPID (same slot)
2. `back_version_gpid`: Must be updated if that page was also migrated (check tid_mapping)

### 5.1.2.3: Implementation

#### API Design

```cpp
// Add to CatalogManager (private helper)
auto copyPageWithTIDRemapping(const void *source_buffer,
                             void *target_buffer,
                             GPID source_gpid,
                             GPID target_gpid,
                             const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
                             ErrorContext *ctx = nullptr) -> Status;
```

#### Algorithm

```cpp
Status CatalogManager::copyPageWithTIDRemapping(const void *source_buffer,
                                                void *target_buffer,
                                                GPID source_gpid,
                                                GPID target_gpid,
                                                const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
                                                ErrorContext *ctx)
{
    // 1. Validate source page
    const PageHeader *source_header = static_cast<const PageHeader*>(source_buffer);

    if (source_header->magic != PAGE_MAGIC)
    {
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid page magic number");
        return Status::PAGE_CORRUPT;
    }

    if (source_header->page_type != PageType::HEAP_PAGE)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Page is not a heap page");
        return Status::INVALID_ARGUMENT;
    }

    // 2. Copy entire page as base
    std::memcpy(target_buffer, source_buffer, PAGE_SIZE);

    // 3. Update page header with new GPID
    PageHeader *target_header = static_cast<PageHeader*>(target_buffer);
    target_header->page_id = target_gpid;

    // 4. Wrap in HeapPage for structured tuple access
    HeapPage source_page(const_cast<uint8_t*>(static_cast<const uint8_t*>(source_buffer)),
                         PAGE_SIZE);
    HeapPage target_page(static_cast<uint8_t*>(target_buffer), PAGE_SIZE);

    // 5. Update TIDs in all tuples
    uint16_t item_count = source_page.getItemCount();
    uint32_t tuples_updated = 0;

    for (uint16_t slot = 0; slot < item_count; slot++)
    {
        // Get tuple data
        const uint8_t *tuple_data;
        uint32_t tuple_size;

        Status status = source_page.getTuple(slot, &tuple_data, &tuple_size, ctx);

        if (status == Status::NOT_FOUND)
        {
            // Deleted tuple (slot is empty)
            continue;
        }

        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to read tuple from source page");
            return status;
        }

        // Parse tuple header
        // Note: tuple_data points to TupleHeader followed by user data
        TupleHeader *tuple_header = const_cast<TupleHeader*>(
            reinterpret_cast<const TupleHeader*>(tuple_data)
        );

        // Update ctid to new GPID (keep same slot)
        tuple_header->ctid_gpid = target_gpid;
        tuple_header->ctid_slot = slot;

        // Update back_version_gpid if it references a migrated page
        if (tuple_header->back_version_gpid != 0) // 0 = no back version
        {
            GPID old_back_gpid = tuple_header->back_version_gpid;

            // Check if this GPID was migrated
            auto it = tid_mapping.find(old_back_gpid);
            if (it != tid_mapping.end())
            {
                // Update to new GPID
                GPID new_back_gpid = it->second;
                tuple_header->back_version_gpid = new_back_gpid;
                // Note: back_version_slot remains unchanged
            }
            // else: back version is in a different table or not yet migrated (keep old GPID)
        }

        tuples_updated++;
    }

    // 6. Recalculate page checksum
    target_header->checksum = 0; // Clear old checksum
    target_header->checksum = calculateChecksum(target_buffer, PAGE_SIZE);

    LOG_DEBUG(CATALOG, "Copied page %lu → %lu: %u tuples updated",
              source_gpid, target_gpid, tuples_updated);

    return Status::OK;
}
```

### 5.1.2.4: Integration into Batch Processing Loop

Replace the STUB simulation loop (lines 2726-2822) with real page copying:

```cpp
// ===== STEP 5: Batch-based page migration (REPLACE STUB) =====
std::unordered_map<uint64_t, uint64_t> tid_mapping; // old GPID → new GPID
std::vector<GPID> heap_pages;

// Enumerate all heap pages for this table
Status status = enumerateTablePages(table_id, heap_pages, ctx);
if (status != Status::OK)
{
    SET_ERROR_CONTEXT(ctx, status, "Failed to enumerate heap pages");
    return status;
}

uint32_t total_pages = static_cast<uint32_t>(heap_pages.size());
LOG_INFO(CATALOG, "Table '%s': %u heap pages to migrate", table_info.table_name.c_str(), total_pages);

// Handle empty table
if (total_pages == 0)
{
    LOG_INFO(CATALOG, "Table '%s' is empty, skipping page migration", table_info.table_name.c_str());
    // Skip to catalog update
    goto update_catalog;
}

// Calculate batch size (already implemented in Phase 4.1.4)
uint32_t batch_size = TableMigration::MAX_BATCH_SIZE_PAGES;
if (total_pages < TableMigration::MIN_BATCH_SIZE_PAGES)
{
    batch_size = total_pages; // Small table: process all at once
}
else if (total_pages < TableMigration::MAX_BATCH_SIZE_PAGES)
{
    batch_size = std::max(TableMigration::MIN_BATCH_SIZE_PAGES, total_pages / 10);
}

uint32_t pages_copied = 0;
auto last_log_time = std::chrono::steady_clock::now();
constexpr auto LOG_INTERVAL = std::chrono::seconds(5);

// Batch processing loop
for (uint32_t batch_start = 0; batch_start < total_pages; batch_start += batch_size)
{
    uint32_t batch_end = std::min(batch_start + batch_size, total_pages);
    uint32_t this_batch_size = batch_end - batch_start;
    uint32_t current_batch = batch_start / batch_size + 1;

    LOG_INFO(CATALOG, "Batch %u: Processing pages %u-%u (%u pages)",
             current_batch, batch_start + 1, batch_end, this_batch_size);

    // Process each page in batch
    for (uint32_t i = batch_start; i < batch_end; i++)
    {
        GPID source_gpid = heap_pages[i];

        // 1. Pin source page
        void *source_buffer;
        status = db_->buffer_pool()->pinPageGlobal(source_gpid, &source_buffer, ctx);
        if (status != Status::OK)
        {
            LOG_ERROR(CATALOG, "Failed to pin source page %lu", source_gpid);
            rollbackPageMigration(tid_mapping, ctx);
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin source page");
            return status;
        }

        // 2. Allocate target page in target tablespace
        GPID target_gpid;
        status = db_->page_manager()->allocatePageInTablespace(target_tablespace_id,
                                                               &target_gpid, ctx);
        if (status != Status::OK)
        {
            LOG_ERROR(CATALOG, "Failed to allocate target page in tablespace %u",
                     target_tablespace_id);
            db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
            rollbackPageMigration(tid_mapping, ctx);
            SET_ERROR_CONTEXT(ctx, status, "Failed to allocate target page");
            return status;
        }

        // 3. Pin target page
        void *target_buffer;
        status = db_->buffer_pool()->pinPageGlobal(target_gpid, &target_buffer, ctx);
        if (status != Status::OK)
        {
            LOG_ERROR(CATALOG, "Failed to pin target page %lu", target_gpid);
            db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
            db_->page_manager()->freePageInTablespace(
                getTablespaceID(target_gpid), getPageNumber(target_gpid), ctx);
            rollbackPageMigration(tid_mapping, ctx);
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin target page");
            return status;
        }

        // 4. Copy page with TID remapping
        status = copyPageWithTIDRemapping(source_buffer, target_buffer,
                                         source_gpid, target_gpid,
                                         tid_mapping, ctx);
        if (status != Status::OK)
        {
            LOG_ERROR(CATALOG, "Failed to copy page %lu → %lu", source_gpid, target_gpid);
            db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
            db_->buffer_pool()->unpinPageGlobal(target_gpid, false, ctx);
            rollbackPageMigration(tid_mapping, ctx);
            SET_ERROR_CONTEXT(ctx, status, "Failed to copy page with TID remapping");
            return status;
        }

        // 5. Record TID mapping (for index updates and back version updates)
        tid_mapping[source_gpid] = target_gpid;

        // 6. Unpin pages
        db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx); // Clean (not modified)
        db_->buffer_pool()->unpinPageGlobal(target_gpid, true, ctx);  // Dirty (modified)

        pages_copied++;

        // 7. Progress tracking
        if (pages_copied % TableMigration::PROGRESS_CALLBACK_INTERVAL_PAGES == 0)
        {
            auto now = std::chrono::steady_clock::now();
            if (now - last_log_time >= LOG_INTERVAL)
            {
                LOG_INFO(CATALOG, "Migrating table '%s': %u / %u pages copied (%.1f%%)",
                        table_info.table_name.c_str(), pages_copied, total_pages,
                        (pages_copied * 100.0) / total_pages);
                last_log_time = now;
            }

            // Invoke progress callback
            if (progress_callback && !progress_callback(pages_copied, total_pages))
            {
                LOG_WARNING(CATALOG, "Table migration cancelled by user");
                rollbackPageMigration(tid_mapping, ctx);
                SET_ERROR_CONTEXT(ctx, Status::CANCELLED, "Migration cancelled by user");
                return Status::CANCELLED;
            }
        }
    }

    LOG_INFO(CATALOG, "Batch %u complete: %u pages copied", current_batch, this_batch_size);
}

LOG_INFO(CATALOG, "All %u pages copied successfully", total_pages);

update_catalog:
// Continue to Step 6: Update indexes (already implemented)
// ... rest of moveTableToTablespace() ...
```

### 5.1.2.5: Edge Cases

1. **Version Chains Across Pages**:
   - Tuple T1 on page P1 has `back_version_gpid = P2`
   - Both P1 and P2 are migrated
   - Solution: Build tid_mapping incrementally, update as we go

2. **Version Chains to Unmigrated Pages**:
   - Tuple T1 on table A has `back_version_gpid = P_toast` (in TOAST table)
   - TOAST table not yet migrated
   - Solution: Migrate TOAST table first (Task 5.1.3)

3. **Checksum Calculation**:
   - Use existing `calculateChecksum()` function (assumed to exist)
   - If not, implement CRC32 or similar

4. **Deleted Tuples**:
   - HeapPage::getTuple() returns `Status::NOT_FOUND` for deleted slots
   - Skip these tuples (no TID to update)

### 5.1.2.6: Files to Modify

- `include/scratchbird/core/catalog_manager.h`: Add `copyPageWithTIDRemapping()` declaration
- `src/core/catalog_manager.cpp`: Implement `copyPageWithTIDRemapping()`
- `src/core/catalog_manager.cpp`: Replace STUB loop with real page copying logic (lines 2726-2822)

### 5.1.2.7: Testing

```cpp
// Test Case 1: Single-page table (no version chains)
CREATE TABLE simple (id INT);
INSERT INTO simple VALUES (1), (2), (3);
ALTER TABLE simple SET TABLESPACE ts;
// Verify: All TIDs updated to new GPID

// Test Case 2: Multi-page table
CREATE TABLE multi (id INT, data VARCHAR(1000));
INSERT INTO multi SELECT i, repeat('x', 1000) FROM generate_series(1, 100) i;
ALTER TABLE multi SET TABLESPACE ts;
// Verify: All pages copied, all TIDs updated

// Test Case 3: Table with version chains
BEGIN;
UPDATE simple SET id = 10 WHERE id = 1;
COMMIT;
ALTER TABLE simple SET TABLESPACE ts;
// Verify: ctid and back_version_gpid both updated correctly

// Test Case 4: Table with deleted tuples
DELETE FROM simple WHERE id = 2;
ALTER TABLE simple SET TABLESPACE ts;
// Verify: Deleted slots skipped, no errors
```

---

## Task 5.1.3: TOAST Handling

**Goal**: Handle TOAST (The Oversized-Attribute Storage Technique) during migration

**Estimated Time**: 6-10 hours

### 5.1.3.1: Background

TOAST stores large values (>~2KB) in separate TOAST tables. Each main table with large columns has an associated TOAST table (`pg_toast.<table_oid>`).

**TOAST Structure**:
- Main table tuple: Contains TOAST pointer (OID + TID)
- TOAST table: Stores chunks of the large value
- Chunks: 2KB slices of the original value

**Migration Challenge**:
- When migrating a table, we must also migrate its TOAST table
- TOAST TID references must be updated

### 5.1.3.2: Implementation Strategy

**Approach**: Migrate TOAST table first, then main table

**Reason**:
- Main table tuples reference TOAST TIDs
- If we migrate main table first, TOAST references become invalid
- Migrating TOAST first ensures references remain valid

### 5.1.3.3: Implementation

#### API Design

```cpp
// Add to TableInfo structure (or query from catalog)
bool has_toast;         // True if table has TOAST
ID toast_table_id;     // ID of TOAST table (if has_toast == true)
```

#### Algorithm

```cpp
// Add before main page migration loop in moveTableToTablespace()

// ===== STEP 4.5: TOAST Handling (NEW STEP) =====
std::unordered_map<uint64_t, uint64_t> toast_tid_mapping;

if (table_info.has_toast)
{
    LOG_INFO(CATALOG, "Table '%s' has TOAST, migrating TOAST table first",
             table_info.table_name.c_str());

    // Get TOAST table ID
    ID toast_table_id = table_info.toast_table_id;

    // Recursively migrate TOAST table
    Status toast_status = moveTableToTablespace(toast_table_id,
                                                target_tablespace_id,
                                                false, // offline (recursive)
                                                nullptr, // no progress callback for TOAST
                                                ctx);
    if (toast_status != Status::OK)
    {
        LOG_ERROR(CATALOG, "Failed to migrate TOAST table (ID %u)", toast_table_id);
        SET_ERROR_CONTEXT(ctx, toast_status, "TOAST table migration failed");
        return toast_status;
    }

    LOG_INFO(CATALOG, "TOAST table migration complete for table '%s'",
             table_info.table_name.c_str());

    // Note: tid_mapping for TOAST table is built during recursive call
    // We don't need toast_tid_mapping here because TOAST tuples are already updated
    // Main table tuples reference TOAST OIDs, not TIDs directly
}

// Continue with main table page migration...
```

### 5.1.3.4: TOAST Reference Updates

**TOAST Pointer Structure** (typical):
```cpp
struct TOASTPointer
{
    uint32_t toast_oid;    // OID of TOAST table
    uint64_t toast_tid;    // TID of first TOAST chunk (GPID)
    uint32_t original_size; // Uncompressed size
    uint32_t stored_size;  // Compressed size (if compressed)
};
```

**Update Strategy**:
- TOAST OID remains unchanged (table ID doesn't change)
- TOAST TID must be updated if TOAST table was migrated
- **BUT**: Recursive `moveTableToTablespace()` call already updates all TOAST tuples
- Main table tuples reference TOAST OIDs, which remain stable

**Conclusion**: No additional TOAST reference updates needed in main table tuples! The recursive migration handles everything.

### 5.1.3.5: Edge Cases

1. **TOAST Table Already in Target Tablespace**:
   - Recursive call to `moveTableToTablespace()` will detect this
   - Returns early with "already in target tablespace" error
   - Solution: Check if TOAST table is in target before recursing

```cpp
// Before recursing
TableInfo toast_info;
status = getTable(toast_table_id, toast_info, ctx);
if (status == Status::OK && toast_info.tablespace_id == target_tablespace_id)
{
    LOG_INFO(CATALOG, "TOAST table already in target tablespace, skipping");
}
else
{
    // Migrate TOAST table
    status = moveTableToTablespace(toast_table_id, target_tablespace_id, false, nullptr, ctx);
}
```

2. **Partial TOAST Values** (inline + external):
   - Some columns stored inline, others TOASTed
   - Recursive migration handles all external values
   - Inline values remain unchanged

3. **TOAST Chunks Spanning Multiple Pages**:
   - Each chunk stored in a single TOAST tuple
   - Multiple tuples may span multiple pages
   - Recursive migration copies all pages

4. **TOAST Compression**:
   - Compressed TOAST values stored as-is
   - No decompression/recompression needed
   - Byte-level copy is sufficient

### 5.1.3.6: Files to Modify

- `src/core/catalog_manager.cpp`: Add TOAST detection and recursive migration before main loop

### 5.1.3.7: Testing

```cpp
// Test Case 1: Table with TOAST values
CREATE TABLE toast_test (id INT, large_text TEXT);
INSERT INTO toast_test VALUES (1, repeat('x', 10000)); -- Forces TOAST
ALTER TABLE toast_test SET TABLESPACE ts;
// Verify: TOAST table migrated, main table migrated, large_text accessible

// Test Case 2: Table with mixed inline/TOAST
INSERT INTO toast_test VALUES (2, 'small'), (3, repeat('y', 10000));
ALTER TABLE toast_test SET TABLESPACE ts;
// Verify: Only TOAST values migrated, inline values unchanged

// Test Case 3: Multiple TOAST columns
CREATE TABLE multi_toast (id INT, text1 TEXT, text2 TEXT, text3 TEXT);
INSERT INTO multi_toast VALUES (1, repeat('a', 10000), repeat('b', 10000), repeat('c', 10000));
ALTER TABLE multi_toast SET TABLESPACE ts;
// Verify: All TOAST chunks migrated, all columns accessible

// Test Case 4: TOAST table already in target tablespace
-- Manually move TOAST table first
-- ALTER TABLE pg_toast.toast_test SET TABLESPACE ts; (internal)
ALTER TABLE toast_test SET TABLESPACE ts;
// Verify: No error, TOAST migration skipped
```

---

## Task 5.1.4: Transaction Rollback

**Goal**: Implement rollback logic to cleanup on error or cancellation

**Estimated Time**: 4-6 hours

### 5.1.4.1: Problem Statement

If migration fails (disk full, corruption, cancellation), we must:
1. Free all allocated pages in target tablespace
2. Leave table in original tablespace (unchanged)
3. Ensure no orphaned pages

Current code has comments like:
```cpp
// In full implementation: rollback page migration here
```

### 5.1.4.2: Rollback Strategy

**What to Rollback**:
- All pages allocated in target tablespace (recorded in `tid_mapping`)
- Catalog changes (in-memory only, not persisted until commit)

**What NOT to Rollback**:
- Source tablespace pages (unchanged)
- Index structures (not yet updated on error)

**Transaction Semantics**:
- Phase 4.1.4 decided: Single transaction for entire migration
- Rollback = undo all changes in transaction
- Database-level rollback handles catalog changes
- We must explicitly free allocated pages

### 5.1.4.3: Implementation

#### API Design

```cpp
// Add to CatalogManager (private helper)
auto rollbackPageMigration(const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
                          ErrorContext *ctx = nullptr) -> Status;
```

#### Algorithm

```cpp
Status CatalogManager::rollbackPageMigration(const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
                                            ErrorContext *ctx)
{
    LOG_WARNING(CATALOG, "Rolling back page migration (%zu pages allocated)",
                tid_mapping.size());

    uint32_t pages_freed = 0;
    uint32_t pages_failed = 0;

    // Free all allocated pages in target tablespace
    for (const auto &[old_gpid, new_gpid] : tid_mapping)
    {
        // Extract tablespace_id and page_number from new_gpid
        uint16_t ts_id = getTablespaceID(new_gpid);
        uint64_t page_num = getPageNumber(new_gpid);

        // Free the page
        Status status = db_->page_manager()->freePageInTablespace(ts_id, page_num, ctx);
        if (status != Status::OK)
        {
            LOG_WARNING(CATALOG, "Failed to free page %lu during rollback (error: %d)",
                       new_gpid, static_cast<int>(status));
            pages_failed++;
            // Continue freeing other pages even if one fails
        }
        else
        {
            pages_freed++;
        }
    }

    LOG_INFO(CATALOG, "Rollback complete: %u pages freed, %u failures",
             pages_freed, pages_failed);

    if (pages_failed > 0)
    {
        LOG_WARNING(CATALOG, "Rollback incomplete: %u pages could not be freed (orphaned)",
                   pages_failed);
        // Not a critical error (garbage collection can clean up later)
        // Return OK to allow error propagation from original failure
    }

    return Status::OK;
}
```

### 5.1.4.4: Rollback Call Sites

Add rollback calls at all error points:

```cpp
// Example error handling in page copying loop
if (status != Status::OK)
{
    LOG_ERROR(CATALOG, "Failed to copy page %lu → %lu", source_gpid, target_gpid);

    // Unpin any pinned pages
    db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
    db_->buffer_pool()->unpinPageGlobal(target_gpid, false, ctx);

    // Rollback all allocated pages
    rollbackPageMigration(tid_mapping, ctx);

    // Set error context and return
    SET_ERROR_CONTEXT(ctx, status, "Failed to copy page with TID remapping");
    return status;
}
```

**All Rollback Call Sites**:
1. Failed to pin source page
2. Failed to allocate target page
3. Failed to pin target page
4. Failed to copy page (corruption, invalid data)
5. User cancellation (progress callback returns false)
6. Failed to update indexes
7. Failed to update catalog (rare, but possible)

### 5.1.4.5: Edge Cases

1. **Partial Rollback Failure**:
   - Some pages freed successfully, others fail
   - Solution: Continue freeing remaining pages, log failures
   - Orphaned pages will be cleaned by VACUUM or manual intervention

2. **Rollback After Index Updates**:
   - Indexes updated, then catalog update fails
   - Solution: Rollback pages, but indexes may have stale TIDs
   - Requires REINDEX after error (document in error message)

3. **TOAST Rollback**:
   - Main table migration fails after TOAST migrated
   - Solution: Also rollback TOAST table pages
   - Requires tracking TOAST tid_mapping separately

```cpp
// Enhanced rollback for TOAST
Status rollbackPageMigration(const std::unordered_map<uint64_t, uint64_t> &main_tid_mapping,
                            const std::unordered_map<uint64_t, uint64_t> &toast_tid_mapping,
                            ErrorContext *ctx)
{
    // Free main table pages
    rollbackPageMigration(main_tid_mapping, ctx);

    // Free TOAST table pages
    if (!toast_tid_mapping.empty())
    {
        LOG_INFO(CATALOG, "Rolling back TOAST table pages (%zu pages)",
                 toast_tid_mapping.size());
        rollbackPageMigration(toast_tid_mapping, ctx);
    }

    return Status::OK;
}
```

4. **Transaction Abort**:
   - Database transaction manager aborts transaction
   - Catalog changes automatically undone
   - Page deallocation must still be explicit

### 5.1.4.6: Files to Modify

- `include/scratchbird/core/catalog_manager.h`: Add `rollbackPageMigration()` declaration
- `src/core/catalog_manager.cpp`: Implement `rollbackPageMigration()`
- `src/core/catalog_manager.cpp`: Add rollback calls at all error points

### 5.1.4.7: Testing

```cpp
// Test Case 1: Rollback on disk full
-- Create tablespace with small MAXSIZE
CREATE TABLESPACE small_ts LOCATION '/tmp/small_ts' MAXSIZE 10MB;

-- Create large table
CREATE TABLE large (id INT, data VARCHAR(1000));
INSERT INTO large SELECT i, repeat('x', 1000) FROM generate_series(1, 10000) i;

-- Attempt migration (will fail with OUT_OF_SPACE)
ALTER TABLE large SET TABLESPACE small_ts;
-- Expected: Error, rollback, table still in default tablespace

-- Verify table still accessible
SELECT COUNT(*) FROM large;
-- Expected: 10000 rows

-- Verify no orphaned pages in small_ts
-- (Check FSM or tablespace file size)

// Test Case 2: Rollback on cancellation
-- Start migration of large table
ALTER TABLE large SET TABLESPACE ts;
-- Cancel during migration (simulate via progress callback)
-- Expected: Rollback, table in original tablespace

// Test Case 3: Rollback on corruption
-- Simulate page corruption during copy (e.g., invalid checksum)
-- Expected: Error, rollback, table unchanged

// Test Case 4: Partial rollback failure
-- Simulate freePageInTablespace() failure for some pages
-- Expected: Rollback continues, failures logged, orphaned pages reported
```

---

## Integration Points

### 5.1.5.1: BufferPool Integration

**Interactions**:
- `pinPageGlobal(gpid, &buffer)`: Read source page
- `pinPageGlobal(gpid, &buffer)`: Write target page
- `unpinPageGlobal(gpid, dirty)`: Release pages

**Concurrency**:
- OFFLINE migration holds EXCLUSIVE lock on table
- No concurrent access to heap pages during migration
- BufferPool handles page locking internally

### 5.1.5.2: PageManager Integration

**Interactions**:
- `getAllocatedPages(ts_id, pages)`: Enumerate pages (new API)
- `allocatePageInTablespace(ts_id, &gpid)`: Allocate target page
- `freePageInTablespace(ts_id, page_num)`: Free old pages (rollback)

**FSM Updates**:
- FSM automatically updated on page allocation/free
- No manual FSM updates needed

### 5.1.5.3: HeapPage Integration

**Interactions**:
- `HeapPage::getItemCount()`: Count tuples
- `HeapPage::getTuple(slot, &data, &size)`: Read tuple
- Tuple header access: `TupleHeader *hdr = (TupleHeader*)data`

**No Modifications**:
- HeapPage API is read-only here
- Target page modified via direct memcpy and pointer manipulation

### 5.1.5.4: CatalogManager Integration

**Existing Methods Used**:
- `getTable(table_id, table_info)`: Get table metadata
- `listIndexesForTable(table_id, indexes)`: Get indexes
- `updateIndexTIDs(table_id, tid_mapping)`: Update indexes (Phase 4.1.5)

**New Methods Added**:
- `enumerateTablePages(table_id, pages)`
- `copyPageWithTIDRemapping(src, tgt, ...)`
- `rollbackPageMigration(tid_mapping)`

---

## Error Handling Strategy

### 5.1.6.1: Error Categories

**Recoverable Errors** (with rollback):
1. Disk full (OUT_OF_SPACE)
2. Page allocation failure
3. Buffer pool exhaustion (too many pinned pages)
4. User cancellation

**Unrecoverable Errors** (abort transaction, rollback):
1. Page corruption (invalid magic, checksum mismatch)
2. Catalog inconsistency (table not found)
3. System errors (I/O errors, memory allocation failure)

### 5.1.6.2: Error Propagation

**Pattern**:
```cpp
Status status = someOperation(..., ctx);
if (status != Status::OK)
{
    // Cleanup
    unpinPages();
    rollbackPageMigration(tid_mapping, ctx);

    // Propagate error
    SET_ERROR_CONTEXT(ctx, status, "Descriptive message");
    return status;
}
```

**Error Context**:
- Use `ErrorContext` to provide detailed error messages
- Include table name, page GPIDs, batch number
- Help user diagnose issue

### 5.1.6.3: Logging

**Log Levels**:
- `LOG_INFO`: Normal progress (every 5 seconds, batch completion)
- `LOG_WARNING`: Recoverable errors, rollback initiated
- `LOG_ERROR`: Unrecoverable errors, migration failed
- `LOG_DEBUG`: Detailed page-level operations

**Key Log Points**:
1. Migration start (table name, source/target tablespace)
2. Page enumeration complete (total pages)
3. Batch progress (every 5 seconds)
4. TOAST migration (if applicable)
5. Index update start/complete
6. Rollback initiated
7. Migration complete (total time, pages copied)

---

## Testing Strategy

### 5.1.7.1: Unit Tests

**Test `enumerateTablePages()`**:
- Empty table (0 pages)
- Single-page table (1 page)
- Multi-page table (100 pages)
- Table with deleted pages (fragmentation)

**Test `copyPageWithTIDRemapping()`**:
- Page with no version chains
- Page with version chains (back_version_gpid)
- Page with deleted tuples
- Page with TOAST references (verify no corruption)

**Test `rollbackPageMigration()`**:
- Rollback 10 pages
- Rollback with partial failures
- Verify all pages freed

### 5.1.7.2: Integration Tests

**End-to-End Migration**:
```cpp
// Test 1: Simple table
CREATE TABLE t1 (id INT);
INSERT INTO t1 SELECT generate_series(1, 10000);
ALTER TABLE t1 SET TABLESPACE ts;
SELECT COUNT(*) FROM t1; -- Expect 10000

// Test 2: Table with indexes
CREATE TABLE t2 (id INT, name VARCHAR(100));
CREATE INDEX idx_id ON t2(id);
INSERT INTO t2 SELECT i, 'name' || i FROM generate_series(1, 10000) i;
ALTER TABLE t2 SET TABLESPACE ts;
SELECT * FROM t2 WHERE id = 5000; -- Expect correct row via index

// Test 3: Table with TOAST
CREATE TABLE t3 (id INT, large_text TEXT);
INSERT INTO t3 VALUES (1, repeat('x', 10000));
ALTER TABLE t3 SET TABLESPACE ts;
SELECT length(large_text) FROM t3 WHERE id = 1; -- Expect 10000

// Test 4: Version chains
BEGIN;
UPDATE t1 SET id = 20000 WHERE id = 1;
COMMIT;
ALTER TABLE t1 SET TABLESPACE ts;
-- Verify both current and old version accessible (MVCC)

// Test 5: Cancellation
-- Long-running migration, cancel midway
ALTER TABLE large_table SET TABLESPACE ts; -- Cancel via Ctrl+C
SELECT COUNT(*) FROM large_table; -- Expect original count in original tablespace
```

### 5.1.7.3: Stress Tests

**Large Table Migration**:
- Table with 100M rows (~12.5M pages)
- Verify batch processing keeps memory bounded
- Measure throughput (pages/sec)

**Concurrent Migrations** (future):
- Migrate multiple tables in parallel
- Verify no GPID collisions

**Disk Full**:
- Tablespace with MAXSIZE 100MB
- Insert until full, trigger migration
- Verify graceful error and rollback

### 5.1.7.4: Failure Tests

**Rollback on Error**:
- Simulate disk full during batch 5 of 10
- Verify batches 1-4 rolled back (pages freed)

**Corruption Detection**:
- Corrupt source page (invalid magic)
- Attempt migration
- Verify error detected, rollback succeeds

**TOAST Migration Failure**:
- Simulate TOAST migration failure
- Verify main table migration aborts
- Verify table unchanged

---

## Performance Considerations

### 5.1.8.1: Throughput Targets

**Target**: 100 pages/sec (800 KB/sec)

**Assumptions**:
- SSD storage (~500 MB/s sequential I/O)
- Single-threaded migration
- 8KB page size

**Bottlenecks**:
1. Disk I/O (reading source pages, writing target pages)
2. Buffer pool contention (pinning/unpinning)
3. TID remapping overhead (tuple iteration)

### 5.1.8.2: Optimizations

**1. Batch Processing** (implemented in Phase 4.1.4):
- Process 1000 pages per batch
- Reduces transaction overhead
- Bounds memory usage

**2. Sequential I/O**:
- Enumerate pages in GPID order (tablespace layout)
- Improves disk read performance
- Reduces seek time

**3. Parallel Copy** (future):
- Multiple threads copying different batches
- Requires careful synchronization
- Deferred to post-BETA

**4. Readahead**:
- Prefetch next batch while processing current batch
- Reduces I/O wait time
- Requires buffer pool extensions

### 5.1.8.3: Memory Usage

**Per-Batch Memory**:
- Heap pages: 1000 × 8 KB = 8 MB
- TID mapping: 1000 × 32 bytes = 32 KB
- **Total: ~8-10 MB per batch**

**Peak Memory**:
- Single batch processed at a time
- Constant memory regardless of table size

### 5.1.8.4: Benchmark Tests

```cpp
// Benchmark 1: Small table (1 MB)
CREATE TABLE bench_small AS SELECT i FROM generate_series(1, 125) i;
ALTER TABLE bench_small SET TABLESPACE ts; -- Measure time

// Benchmark 2: Medium table (100 MB)
CREATE TABLE bench_medium AS SELECT i FROM generate_series(1, 12500) i;
ALTER TABLE bench_medium SET TABLESPACE ts; -- Measure time

// Benchmark 3: Large table (1 GB)
CREATE TABLE bench_large AS SELECT i FROM generate_series(1, 125000) i;
ALTER TABLE bench_large SET TABLESPACE ts; -- Measure time

// Expected times (at 100 pages/sec):
// - Small: ~1 second
// - Medium: ~2 minutes
// - Large: ~20 minutes
```

---

## Appendix: Code Examples

### Example 1: Complete `moveTableToTablespace()` Flow

```cpp
Status CatalogManager::moveTableToTablespace(const ID &table_id,
                                             uint16_t target_tablespace_id,
                                             bool online,
                                             const TableMigrationProgressCallback &progress_callback,
                                             ErrorContext *ctx)
{
    // Step 0: Reject ONLINE mode (not implemented)
    if (online)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "ONLINE migration not yet implemented");
        return Status::NOT_IMPLEMENTED;
    }

    // Step 1: Validate table exists
    TableInfo table_info;
    Status status = getTable(table_id, table_info, ctx);
    if (status != Status::OK) return status;

    // Step 2: Validate target tablespace different
    if (table_info.tablespace_id == target_tablespace_id)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Table already in target tablespace");
        return Status::INVALID_ARGUMENT;
    }

    LOG_INFO(CATALOG, "Starting OFFLINE migration: table '%s' (ID %u) from tablespace %u → %u",
             table_info.table_name.c_str(), table_id,
             table_info.tablespace_id, target_tablespace_id);

    // Step 3: TOAST Handling
    if (table_info.has_toast)
    {
        LOG_INFO(CATALOG, "Migrating TOAST table first");
        status = moveTableToTablespace(table_info.toast_table_id, target_tablespace_id,
                                       false, nullptr, ctx);
        if (status != Status::OK) return status;
    }

    // Step 4: Enumerate heap pages
    std::vector<GPID> heap_pages;
    status = enumerateTablePages(table_id, heap_pages, ctx);
    if (status != Status::OK) return status;

    uint32_t total_pages = static_cast<uint32_t>(heap_pages.size());
    if (total_pages == 0)
    {
        LOG_INFO(CATALOG, "Table is empty, updating catalog only");
        goto update_catalog;
    }

    // Step 5: Batch-based page migration
    std::unordered_map<uint64_t, uint64_t> tid_mapping;
    {
        uint32_t batch_size = calculateBatchSize(total_pages);
        uint32_t pages_copied = 0;

        for (uint32_t batch_start = 0; batch_start < total_pages; batch_start += batch_size)
        {
            uint32_t batch_end = std::min(batch_start + batch_size, total_pages);

            for (uint32_t i = batch_start; i < batch_end; i++)
            {
                GPID source_gpid = heap_pages[i];
                GPID target_gpid;

                // Pin source, allocate target, pin target
                void *src_buf, *tgt_buf;
                status = db_->buffer_pool()->pinPageGlobal(source_gpid, &src_buf, ctx);
                if (status != Status::OK) { rollbackPageMigration(tid_mapping, ctx); return status; }

                status = db_->page_manager()->allocatePageInTablespace(target_tablespace_id, &target_gpid, ctx);
                if (status != Status::OK) { /* cleanup and rollback */ return status; }

                status = db_->buffer_pool()->pinPageGlobal(target_gpid, &tgt_buf, ctx);
                if (status != Status::OK) { /* cleanup and rollback */ return status; }

                // Copy page with TID remapping
                status = copyPageWithTIDRemapping(src_buf, tgt_buf, source_gpid, target_gpid, tid_mapping, ctx);
                if (status != Status::OK) { /* cleanup and rollback */ return status; }

                tid_mapping[source_gpid] = target_gpid;

                // Unpin
                db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
                db_->buffer_pool()->unpinPageGlobal(target_gpid, true, ctx);

                pages_copied++;

                // Progress callback
                if (pages_copied % 100 == 0 && progress_callback)
                {
                    if (!progress_callback(pages_copied, total_pages))
                    {
                        rollbackPageMigration(tid_mapping, ctx);
                        return Status::CANCELLED;
                    }
                }
            }
        }
    }

    // Step 6: Update indexes
    status = updateIndexTIDs(table_id, tid_mapping, ctx);
    if (status != Status::OK) { rollbackPageMigration(tid_mapping, ctx); return status; }

update_catalog:
    // Step 7: Update catalog
    table_info.tablespace_id = target_tablespace_id;
    status = updateTable(table_id, table_info, ctx);
    if (status != Status::OK) return status;

    // Step 8: Free old pages
    for (const auto &[old_gpid, new_gpid] : tid_mapping)
    {
        uint16_t ts_id = getTablespaceID(old_gpid);
        uint64_t page_num = getPageNumber(old_gpid);
        db_->page_manager()->freePageInTablespace(ts_id, page_num, ctx);
    }

    LOG_INFO(CATALOG, "Migration complete: %u pages migrated", total_pages);
    return Status::OK;
}
```

---

## Summary

Phase 5.1 implements the core data movement logic for table migration:

**Deliverables**:
1. **Task 5.1.1**: `enumerateTablePages()` - Find all heap pages via FSM
2. **Task 5.1.2**: `copyPageWithTIDRemapping()` - Copy pages with TID updates
3. **Task 5.1.3**: TOAST migration - Recursive migration of TOAST tables
4. **Task 5.1.4**: `rollbackPageMigration()` - Cleanup on error

**Total Time**: 35-50 hours (distributed across 4 tasks)

**Success Criteria**:
- ✅ Can migrate empty tables (0 pages)
- ✅ Can migrate small tables (< 1000 pages)
- ✅ Can migrate large tables (> 1M pages) with bounded memory
- ✅ All TID references updated correctly
- ✅ TOAST tables migrated successfully
- ✅ Rollback works on error or cancellation
- ✅ No orphaned pages after migration or rollback

**Next Steps** (Phase 5.2-5.4):
- Phase 5.2: Implement B-Tree index TID updates (6-10 hours)
- Phase 5.3: Implement other index types (18-25 hours)
- Phase 5.4: ONLINE migration (40-60 hours, post-BETA)

---

**Document Version**: 1.0
**Last Updated**: October 21, 2025
**Next Review**: Start of Phase 5.1 implementation
