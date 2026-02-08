# Phase 5: Full Implementation Plan - Table Migration & Index Updates

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Purpose**: Complete implementation roadmap for replacing STUB code with production-ready table migration
**Status**: 📋 PLANNING
**Estimated Total Time**: 70-105 hours
**Target**: Production-ready `ALTER TABLE ... SET TABLESPACE` with ONLINE migration support

---

## Overview

Phase 4 completed the **infrastructure and integration** for table migration. Phase 5 will implement the **actual data movement logic** to make the feature production-ready.

### What Works Now (Phase 4 Complete)

✅ **Parser**: Full SQL syntax support for `ALTER TABLE ... SET TABLESPACE [ONLINE]`
✅ **Bytecode**: Opcode generation and execution
✅ **Progress Tracking**: Callbacks, logging, cancellation
✅ **Batch Processing**: Memory-bounded page processing (~8-10 MB per batch)
✅ **Index TID Update Infrastructure**: Framework for all 7 index types
✅ **Error Handling**: Comprehensive error propagation and context

### What Needs Implementation (Phase 5)

❌ **Heap Page Enumeration**: Scan table to find all heap pages
❌ **Page Copying**: Physical page-level data migration
❌ **TID Remapping**: Update all tuple header TID references
❌ **TOAST Handling**: Migrate large values and update references
❌ **Index TID Updates**: Actual scan-and-update for all 7 index types
❌ **Transaction Rollback**: Cleanup on error or cancellation
❌ **ONLINE Migration**: Concurrent read/write support (Phase 5.2)

---

## Phase 5 Task Breakdown

### **Phase 5.1: OFFLINE Migration - Data Movement** (35-50 hours)

Complete the OFFLINE migration path by implementing actual page copying and TID updates.

#### **Task 5.1.1: Heap Page Enumeration** (4-6 hours)

**Goal**: Enumerate all pages belonging to a table for migration

**Current State**: `total_pages = 100; // STUB` (line 2676 in catalog_manager.cpp)

**Implementation**:

1. **Scan table metadata** (1 hour)
   - Read `TableInfo.root_page` from catalog
   - Determine table type (heap, index-organized, etc.)
   - Validate table has heap pages to migrate

2. **Enumerate heap pages** (2-3 hours)
   - **Strategy**: Use Free Space Map (FSM) to find allocated pages
   - **Alternative**: Scan file sequentially, check PageHeader.page_type
   - **API**: `PageManager::getTablePages(table_id, vector<GPID>&)`
   - **Result**: Vector of all GPIDs for this table's heap pages

3. **Handle edge cases** (1-2 hours)
   - Empty tables (0 pages)
   - Fragmented tables (non-contiguous pages)
   - Tables with TOAST (separate TOAST table)
   - Partitioned tables (future)

**Algorithm**:
```cpp
// Method to add to PageManager or CatalogManager
Status enumerateTablePages(const ID &table_id,
                          std::vector<GPID> &pages_out,
                          ErrorContext *ctx)
{
    // 1. Get table info from catalog
    TableInfo table_info;
    Status status = getTable(table_id, table_info, ctx);
    if (status != Status::OK) return status;

    // 2. Get tablespace info
    uint16_t tablespace_id = table_info.tablespace_id;

    // 3. Scan FSM for pages owned by this table
    // Option A: Use FSM to find allocated pages
    for (auto &[gpid, fsm_entry] : fsm_cache_[tablespace_id])
    {
        // Check if page belongs to this table
        void *page_buffer;
        status = buffer_pool_->pinPageGlobal(gpid, &page_buffer, ctx);
        if (status != Status::OK) continue;

        PageHeader *header = static_cast<PageHeader*>(page_buffer);
        if (header->table_id == table_id &&
            header->page_type == PageType::HEAP_PAGE)
        {
            pages_out.push_back(gpid);
        }

        buffer_pool_->unpinPageGlobal(gpid, false, ctx);
    }

    // Option B: Sequential scan of tablespace file
    // (Use if FSM doesn't track table ownership)

    LOG_INFO(CATALOG, "Enumerated %zu heap pages for table", pages_out.size());
    return Status::OK;
}
```

**Files to Modify**:
- `include/scratchbird/core/catalog_manager.h`: Add `enumerateTablePages()` declaration
- `src/core/catalog_manager.cpp`: Implement `enumerateTablePages()`
- `src/core/catalog_manager.cpp`: Replace `total_pages = 100` with actual enumeration

**Testing**:
```cpp
// Test empty table
CREATE TABLE empty_table (id INT);
ALTER TABLE empty_table SET TABLESPACE ts;
// Expected: 0 pages enumerated, migration succeeds

// Test small table
CREATE TABLE small_table (id INT);
INSERT INTO small_table VALUES (1), (2), (3);
ALTER TABLE small_table SET TABLESPACE ts;
// Expected: 1 page enumerated, migration succeeds

// Test large table
CREATE TABLE large_table (id INT);
INSERT INTO large_table SELECT generate_series(1, 100000);
ALTER TABLE large_table SET TABLESPACE ts;
// Expected: ~12500 pages enumerated (8 rows/page × 8KB pages)
```

---

#### **Task 5.1.2: Page Copying with TID Remapping** (8-12 hours)

**Goal**: Copy heap pages from source to target tablespace, updating all TID references

**Current State**: Simulation loop with sleep (lines 2726-2822 in catalog_manager.cpp)

**Implementation**:

1. **Page reading** (2 hours)
   - Pin source page via `buffer_pool_->pinPageGlobal(source_gpid, &buffer)`
   - Validate page integrity (checksum, magic number)
   - Wrap in `HeapPage` class for structured access

2. **Page allocation in target tablespace** (2 hours)
   - Allocate new page: `page_manager_->allocatePageInTablespace(target_ts_id, &new_gpid)`
   - Pin target page for writing
   - Initialize page header with new GPID

3. **Tuple-level copying with TID remapping** (3-4 hours)
   - Iterate all tuples in source page
   - For each tuple:
     - Read tuple header and data
     - Update `TupleHeader.ctid_gpid` to new GPID
     - Update `TupleHeader.back_version_gpid` if in tid_mapping
     - Copy tuple to target page at same slot
   - Maintain slot-level alignment (critical for TID stability)

4. **TID mapping construction** (1-2 hours)
   - Build mapping: `tid_mapping[old_gpid] = new_gpid`
   - Handle slot-level mapping if needed (future optimization)
   - Store mapping for index update phase

5. **Page finalization** (1-2 hours)
   - Update target page header (item count, free space)
   - Calculate and set checksum
   - Unpin pages (mark target as dirty, source as clean)

**Algorithm**:
```cpp
// Replace simulation loop in moveTableToTablespace()
// Lines 2726-2822 in catalog_manager.cpp

// ===== STEP 5: Batch-based page migration (REAL IMPLEMENTATION) =====
std::unordered_map<uint64_t, uint64_t> tid_mapping; // old GPID → new GPID

for (uint32_t batch_start = 0; batch_start < total_pages; batch_start += batch_size)
{
    uint32_t batch_end = std::min(batch_start + batch_size, total_pages);
    uint32_t this_batch_size = batch_end - batch_start;

    LOG_INFO(CATALOG, "Batch %u: Processing pages %u-%u (%u pages)",
            current_batch, batch_start + 1, batch_end, this_batch_size);

    // Process each page in batch
    for (uint32_t i = batch_start; i < batch_end; i++)
    {
        GPID source_gpid = heap_pages[i];

        // 1. Pin source page
        void *source_buffer;
        Status status = db_->buffer_pool()->pinPageGlobal(source_gpid, &source_buffer, ctx);
        if (status != Status::OK)
        {
            LOG_ERROR(CATALOG, "Failed to pin source page %lu", source_gpid);
            // Rollback: free all allocated pages in tid_mapping
            rollbackPageMigration(tid_mapping, ctx);
            return status;
        }

        // 2. Allocate target page
        GPID target_gpid;
        status = db_->page_manager()->allocatePageInTablespace(target_tablespace_id,
                                                               &target_gpid, ctx);
        if (status != Status::OK)
        {
            db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
            rollbackPageMigration(tid_mapping, ctx);
            return status;
        }

        // 3. Pin target page
        void *target_buffer;
        status = db_->buffer_pool()->pinPageGlobal(target_gpid, &target_buffer, ctx);
        if (status != Status::OK)
        {
            db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
            rollbackPageMigration(tid_mapping, ctx);
            return status;
        }

        // 4. Copy page with TID remapping
        status = copyPageWithTIDRemapping(source_buffer, target_buffer,
                                         source_gpid, target_gpid,
                                         tid_mapping, ctx);
        if (status != Status::OK)
        {
            db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
            db_->buffer_pool()->unpinPageGlobal(target_gpid, false, ctx);
            rollbackPageMigration(tid_mapping, ctx);
            return status;
        }

        // 5. Record TID mapping
        tid_mapping[source_gpid] = target_gpid;

        // 6. Unpin pages
        db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx); // Clean
        db_->buffer_pool()->unpinPageGlobal(target_gpid, true, ctx);  // Dirty

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

            if (progress_callback && !progress_callback(pages_copied, total_pages))
            {
                rollbackPageMigration(tid_mapping, ctx);
                return Status::CANCELLED;
            }
        }
    }

    LOG_INFO(CATALOG, "Batch %u complete: %u pages copied", current_batch, this_batch_size);
}
```

**Helper Method: `copyPageWithTIDRemapping()`**:
```cpp
// Add to CatalogManager as private method
Status copyPageWithTIDRemapping(const void *source_buffer,
                                void *target_buffer,
                                GPID source_gpid,
                                GPID target_gpid,
                                const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
                                ErrorContext *ctx)
{
    // 1. Cast to page headers
    const PageHeader *source_header = static_cast<const PageHeader*>(source_buffer);
    PageHeader *target_header = static_cast<PageHeader*>(target_buffer);

    // 2. Validate source page
    if (source_header->magic != PAGE_MAGIC)
    {
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid page magic");
        return Status::PAGE_CORRUPT;
    }

    // 3. Copy entire page as base
    std::memcpy(target_buffer, source_buffer, PAGE_SIZE);

    // 4. Update page header
    target_header->page_id = target_gpid; // Update GPID in header

    // 5. Wrap in HeapPage for tuple access
    HeapPage source_page(const_cast<uint8_t*>(static_cast<const uint8_t*>(source_buffer)),
                         PAGE_SIZE);
    HeapPage target_page(static_cast<uint8_t*>(target_buffer), PAGE_SIZE);

    // 6. Update all tuple TIDs
    uint16_t item_count = source_page.getItemCount();
    for (uint16_t slot = 0; slot < item_count; slot++)
    {
        const uint8_t *tuple_data;
        uint32_t tuple_size;

        Status status = source_page.getTuple(slot, &tuple_data, &tuple_size, ctx);
        if (status == Status::NOT_FOUND) continue; // Deleted tuple
        if (status != Status::OK) return status;

        // Get tuple header
        TupleHeader *tuple_header = const_cast<TupleHeader*>(
            reinterpret_cast<const TupleHeader*>(tuple_data)
        );

        // Update ctid to new GPID (keep same slot)
        tuple_header->setTID(target_gpid, slot);

        // Update back_version_gpid if it's in the mapping
        if (tuple_header->hasBackVersion())
        {
            GPID old_back_gpid = tuple_header->back_version_gpid;
            auto it = tid_mapping.find(old_back_gpid);
            if (it != tid_mapping.end())
            {
                GPID new_back_gpid = it->second;
                tuple_header->setBackVersionTID(new_back_gpid,
                                               tuple_header->back_version_slot);
            }
        }
    }

    // 7. Recalculate checksum
    target_header->checksum = calculateChecksum(target_buffer, PAGE_SIZE);

    return Status::OK;
}
```

**Files to Modify**:
- `include/scratchbird/core/catalog_manager.h`: Add `copyPageWithTIDRemapping()` declaration
- `src/core/catalog_manager.cpp`: Implement page copying logic
- `src/core/catalog_manager.cpp`: Replace simulation loop with real copying

**Testing**:
```cpp
// Test single-page table
CREATE TABLE single_page (id INT);
INSERT INTO single_page VALUES (1), (2), (3);
ALTER TABLE single_page SET TABLESPACE ts;
// Verify: 1 page copied, TIDs updated correctly

// Test multi-page table
CREATE TABLE multi_page (id INT, data VARCHAR(1000));
INSERT INTO multi_page SELECT i, repeat('x', 1000) FROM generate_series(1, 100) i;
ALTER TABLE multi_page SET TABLESPACE ts;
// Verify: All pages copied, all TIDs reference new tablespace

// Test table with version chains
BEGIN;
UPDATE single_page SET id = 10 WHERE id = 1;
COMMIT;
ALTER TABLE single_page SET TABLESPACE ts;
// Verify: back_version_gpid updated correctly
```

---

#### **Task 5.1.3: TOAST Handling** (6-10 hours)

**Goal**: Handle large values (TOAST) during migration

**Current State**: Not implemented

**Background**: TOAST (The Oversized-Attribute Storage Technique) stores large values (>~2KB) in separate TOAST tables. When migrating a table, we must also migrate its TOAST table and update references.

**Implementation**:

1. **Detect TOAST table** (1 hour)
   - Check `TableInfo.has_toast` flag
   - Get TOAST table ID from catalog
   - Enumerate TOAST table pages

2. **Migrate TOAST table first** (2-3 hours)
   - Recursively call `moveTableToTablespace()` for TOAST table
   - Build TOAST TID mapping
   - Ensure TOAST migration completes before main table

3. **Update TOAST references in main table** (2-3 hours)
   - Scan tuples for TOAST pointers (external values)
   - Update TOAST OID/TID references using TOAST tid_mapping
   - Handle both compressed and uncompressed TOAST

4. **Edge cases** (1-2 hours)
   - TOAST chunks spanning multiple pages
   - Partial TOAST values (inline + external)
   - TOAST table already in target tablespace

**Algorithm**:
```cpp
// Add before main page migration loop
// Check if table has TOAST
if (table_info.has_toast)
{
    LOG_INFO(CATALOG, "Table has TOAST, migrating TOAST table first");

    // Get TOAST table ID
    ID toast_table_id = getToastTableId(table_id, ctx);

    // Recursively migrate TOAST table
    Status toast_status = moveTableToTablespace(toast_table_id,
                                                target_tablespace_id,
                                                false, // offline
                                                nullptr, // no progress callback
                                                ctx);
    if (toast_status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, toast_status, "Failed to migrate TOAST table");
        return toast_status;
    }

    LOG_INFO(CATALOG, "TOAST table migration complete");
}

// During tuple copying (in copyPageWithTIDRemapping):
// Check for TOAST references
if (tuple_has_toast_reference(tuple_data, tuple_size))
{
    // Update TOAST OIDs/TIDs
    status = updateToastReferences(tuple_data, tuple_size,
                                  toast_tid_mapping, ctx);
    if (status != Status::OK) return status;
}
```

**Files to Modify**:
- `src/core/catalog_manager.cpp`: Add TOAST detection and recursive migration
- `src/core/catalog_manager.cpp`: Add TOAST reference updating in `copyPageWithTIDRemapping()`

**Testing**:
```cpp
// Test table with TOAST values
CREATE TABLE toast_table (id INT, large_text TEXT);
INSERT INTO toast_table VALUES (1, repeat('x', 10000));
ALTER TABLE toast_table SET TABLESPACE ts;
// Verify: TOAST table migrated, references updated

// Test table with mixed inline/TOAST
INSERT INTO toast_table VALUES (2, 'small'), (3, repeat('y', 10000));
ALTER TABLE toast_table SET TABLESPACE ts;
// Verify: Only TOAST values migrated
```

---

#### **Task 5.1.4: Transaction Rollback** (4-6 hours)

**Goal**: Implement rollback logic to cleanup on error or cancellation

**Current State**: Comment only: `// In full implementation: rollback page migration here`

**Implementation**:

1. **Track allocated pages** (1 hour)
   - TID mapping already contains: old_gpid → new_gpid
   - Extract all `new_gpid` values for deallocation

2. **Implement rollback helper** (2-3 hours)
   - Free all allocated pages in target tablespace
   - Mark pages as free in FSM
   - Log rollback progress

3. **Integrate rollback points** (1-2 hours)
   - On error: Call rollback before returning
   - On cancellation: Call rollback and return Status::CANCELLED
   - On success: Skip rollback (commit changes)

**Algorithm**:
```cpp
// Add as private method to CatalogManager
Status rollbackPageMigration(const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
                            ErrorContext *ctx)
{
    LOG_WARNING(CATALOG, "Rolling back page migration (%zu pages allocated)",
               tid_mapping.size());

    uint32_t pages_freed = 0;

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
            LOG_WARNING(CATALOG, "Failed to free page %lu during rollback", new_gpid);
            // Continue freeing other pages even if one fails
        }
        else
        {
            pages_freed++;
        }
    }

    LOG_INFO(CATALOG, "Rollback complete: %u pages freed", pages_freed);
    return Status::OK;
}
```

**Files to Modify**:
- `include/scratchbird/core/catalog_manager.h`: Add `rollbackPageMigration()` declaration
- `src/core/catalog_manager.cpp`: Implement rollback logic
- `src/core/catalog_manager.cpp`: Add rollback calls at error/cancellation points

**Testing**:
```cpp
// Test rollback on error
// Simulate error during migration (e.g., disk full)
ALTER TABLE large_table SET TABLESPACE full_tablespace;
// Expected: Migration fails, all allocated pages freed

// Test rollback on cancellation
// Simulate user cancellation via progress callback
ALTER TABLE large_table SET TABLESPACE ts;
// Cancel during migration
// Expected: Migration cancelled, all allocated pages freed
```

---

### **Phase 5.2: Index TID Updates - B-Tree** (6-10 hours)

**Goal**: Implement actual B-Tree index TID updates (most common index type)

**Priority**: HIGH (B-Tree is default, ~90% of indexes)

**Current State**: STUB with comments (lines 2520-2529 in catalog_manager.cpp)

**Implementation**:

1. **B-Tree API understanding** (1 hour)
   - Study `include/scratchbird/core/btree.h`
   - Understand leaf node structure
   - Identify TID storage location in leaf entries

2. **Leaf node traversal** (2-3 hours)
   - Start from B-Tree root
   - Traverse to leftmost leaf
   - Scan all leaf nodes via next_leaf pointers

3. **TID update logic** (2-3 hours)
   - For each leaf entry:
     - Extract TID (GPID)
     - Check if TID in tid_mapping
     - If yes, replace with new GPID
     - Mark leaf page as dirty

4. **Integration** (1-2 hours)
   - Replace STUB in `updateIndexTIDs()`
   - Add error handling
   - Add progress logging

**Algorithm**:
```cpp
// Replace STUB in updateIndexTIDs() for IndexType::BTREE
case IndexType::BTREE:
{
    LOG_INFO(CATALOG, "Index '%s': B-Tree index - updating TIDs",
            index_info.index_name.c_str());

    // 1. Open B-Tree index
    // Assuming BTree class exists with root_page
    // (May need to add BTree::open() API)

    // 2. Find leftmost leaf
    GPID leaf_gpid = findLeftmostLeaf(index_info.root_page, ctx);

    uint32_t entries_scanned = 0;
    uint32_t entries_updated = 0;

    // 3. Scan all leaf nodes
    while (leaf_gpid != INVALID_GPID)
    {
        // Pin leaf page
        void *leaf_buffer;
        Status status = db_->buffer_pool()->pinPageGlobal(leaf_gpid, &leaf_buffer, ctx);
        if (status != Status::OK)
        {
            LOG_ERROR(CATALOG, "Failed to pin B-Tree leaf page %lu", leaf_gpid);
            return status;
        }

        // Get leaf node structure
        // (Structure depends on BTree implementation)
        BTreeLeafNode *leaf = static_cast<BTreeLeafNode*>(leaf_buffer);

        bool leaf_modified = false;

        // 4. Update TIDs in this leaf
        for (uint32_t i = 0; i < leaf->entry_count; i++)
        {
            BTreeLeafEntry *entry = &leaf->entries[i];
            GPID old_tid = entry->tid; // Assuming TID stored here

            entries_scanned++;

            // Check if TID needs updating
            auto it = tid_mapping.find(old_tid);
            if (it != tid_mapping.end())
            {
                GPID new_tid = it->second;
                entry->tid = new_tid;
                entries_updated++;
                leaf_modified = true;
            }
        }

        // 5. Get next leaf
        GPID next_leaf_gpid = leaf->next_leaf;

        // 6. Unpin leaf (mark dirty if modified)
        db_->buffer_pool()->unpinPageGlobal(leaf_gpid, leaf_modified, ctx);

        leaf_gpid = next_leaf_gpid;
    }

    LOG_INFO(CATALOG, "Index '%s': B-Tree updated - %u entries scanned, %u TIDs updated",
            index_info.index_name.c_str(), entries_scanned, entries_updated);

    break;
}
```

**Files to Modify**:
- `src/core/catalog_manager.cpp`: Replace B-Tree STUB with real implementation

**Testing**:
```cpp
// Test B-Tree index update
CREATE TABLE indexed_table (id INT, name VARCHAR(100));
CREATE INDEX idx_id ON indexed_table(id);
INSERT INTO indexed_table SELECT i, 'name' || i FROM generate_series(1, 10000) i;

ALTER TABLE indexed_table SET TABLESPACE ts;

// Verify index still works
SELECT * FROM indexed_table WHERE id = 5000;
// Expected: Correct row returned (index used)

// Verify index consistency
REINDEX INDEX idx_id;
// Expected: No errors (index is consistent)
```

---

### **Phase 5.3: Index TID Updates - Other Types** (18-25 hours)

**Goal**: Implement TID updates for remaining 6 index types

#### **Task 5.3.1: Hash Index** (3-4 hours)

Similar to B-Tree but with bucket-based structure.

**Algorithm**:
```cpp
case IndexType::HASH:
{
    // 1. Open hash index
    // 2. Scan all buckets (0 to bucket_count-1)
    for (uint32_t bucket = 0; bucket < hash_idx->bucket_count; bucket++)
    {
        // 3. Pin bucket page
        // 4. Update TIDs in bucket entries
        // 5. Unpin bucket page (dirty if modified)
    }
    break;
}
```

#### **Task 5.3.2: Vector/HNSW Index** (6-8 hours)

Complex graph structure requiring multi-layer traversal.

**Algorithm**:
```cpp
case IndexType::VECTOR:
{
    // 1. Open HNSW index
    // 2. For each layer (top to bottom)
    for (int layer = max_layer; layer >= 0; layer--)
    {
        // 3. Scan all nodes in layer
        // 4. Update node TID
        // 5. Update neighbor TIDs in graph edges
    }
    // 6. Update entry point TID
    break;
}
```

#### **Task 5.3.3: Full-Text Index** (4-6 hours)

Inverted index with posting lists.

**Algorithm**:
```cpp
case IndexType::FULLTEXT:
{
    // 1. Scan all terms in dictionary
    // 2. For each term, get posting list
    // 3. Update TIDs in posting list
    break;
}
```

#### **Task 5.3.4: GIN Index** (5-7 hours)

Generalized inverted index with posting trees.

**Algorithm**:
```cpp
case IndexType::GIN:
{
    // 1. Traverse GIN B-Tree (keys)
    // 2. For each key, get posting tree
    // 3. Update TIDs in posting tree
    break;
}
```

#### **Task 5.3.5: GIST Index** (4-6 hours)

Generalized search tree.

**Algorithm**:
```cpp
case IndexType::GIST:
{
    // 1. Depth-first traversal of GIST tree
    // 2. Update TIDs in leaf nodes
    // 3. Optionally recompute bounding boxes
    break;
}
```

#### **Task 5.3.6: BRIN Index** (3-4 hours)

Block range index (simplest).

**Algorithm**:
```cpp
case IndexType::BRIN:
{
    // 1. Scan BRIN summary pages
    // 2. Update page range references (start_gpid, end_gpid)
    // 3. Recompute min/max if page boundaries changed
    break;
}
```

---

### **Phase 5.4: ONLINE Migration** (40-60 hours)

**Goal**: Support concurrent reads/writes during migration

**Current State**: Rejected in Phase 4 (line 2667-2671 in catalog_manager.cpp)

**Strategy**: Firebird-style incremental migration with catch-up phase

#### **Task 5.4.1: Concurrent Read Support** (8-12 hours)

**Implementation**:
- Shadow table approach: Create temporary target table
- Route reads to appropriate tablespace based on migration progress
- Visibility rules: Check both source and target tablespaces

#### **Task 5.4.2: Concurrent Write Support** (12-18 hours)

**Implementation**:
- Dual-write: Write to both source and target tablespaces
- Transaction coordination: Ensure atomicity across tablespaces
- Conflict resolution: Handle simultaneous updates

#### **Task 5.4.3: Catch-Up Phase** (10-15 hours)

**Implementation**:
- Track changes during migration (write-ahead log)
- Apply incremental changes to target tablespace
- Converge source and target

#### **Task 5.4.4: Final Swap** (5-8 hours)

**Implementation**:
- Brief exclusive lock (milliseconds)
- Atomic catalog update: `tablespace_id` swap
- Redirect all traffic to target tablespace

#### **Task 5.4.5: Cleanup** (5-7 hours)

**Implementation**:
- Free pages in source tablespace
- Remove shadow structures
- Update statistics

---

## Detailed Task List for TABLESPACE_IMPLEMENTATION_PLAN.md

### Phase 5.1: OFFLINE Migration - Data Movement

```markdown
### **Phase 5.1: OFFLINE Migration - Data Movement** (35-50 hours)

- [ ] **5.1.1**: Heap Page Enumeration (4-6 hours)
  - [ ] 5.1.1.1: Scan table metadata (1 hour)
  - [ ] 5.1.1.2: Enumerate heap pages via FSM (2-3 hours)
  - [ ] 5.1.1.3: Handle edge cases (empty, fragmented, TOAST tables) (1-2 hours)
  - **Deliverable**: `enumerateTablePages()` method
  - **Test**: Empty table, single-page table, large table

- [ ] **5.1.2**: Page Copying with TID Remapping (8-12 hours)
  - [ ] 5.1.2.1: Page reading infrastructure (2 hours)
  - [ ] 5.1.2.2: Page allocation in target tablespace (2 hours)
  - [ ] 5.1.2.3: Tuple-level TID remapping (3-4 hours)
  - [ ] 5.1.2.4: TID mapping construction (1-2 hours)
  - [ ] 5.1.2.5: Page finalization (checksums, unpinning) (1-2 hours)
  - **Deliverable**: `copyPageWithTIDRemapping()` method
  - **Test**: Single-page, multi-page, version chains

- [ ] **5.1.3**: TOAST Handling (6-10 hours)
  - [ ] 5.1.3.1: Detect TOAST table (1 hour)
  - [ ] 5.1.3.2: Migrate TOAST table first (2-3 hours)
  - [ ] 5.1.3.3: Update TOAST references in main table (2-3 hours)
  - [ ] 5.1.3.4: Handle edge cases (chunks, partial TOAST) (1-2 hours)
  - **Deliverable**: TOAST migration logic
  - **Test**: Table with large TEXT/BLOB values

- [ ] **5.1.4**: Transaction Rollback (4-6 hours)
  - [ ] 5.1.4.1: Track allocated pages (1 hour)
  - [ ] 5.1.4.2: Implement rollback helper (2-3 hours)
  - [ ] 5.1.4.3: Integrate rollback points (1-2 hours)
  - **Deliverable**: `rollbackPageMigration()` method
  - **Test**: Error scenarios, cancellation scenarios
```

### Phase 5.2: Index TID Updates - B-Tree

```markdown
### **Phase 5.2: Index TID Updates - B-Tree** (6-10 hours)

- [ ] **5.2.1**: B-Tree TID Update Implementation (6-10 hours)
  - [ ] 5.2.1.1: B-Tree API understanding (1 hour)
  - [ ] 5.2.1.2: Leaf node traversal (2-3 hours)
  - [ ] 5.2.1.3: TID update logic (2-3 hours)
  - [ ] 5.2.1.4: Integration and testing (1-2 hours)
  - **Deliverable**: Real B-Tree TID updates (replace STUB)
  - **Test**: B-Tree index on migrated table
```

### Phase 5.3: Index TID Updates - Other Types

```markdown
### **Phase 5.3: Index TID Updates - Other Types** (18-25 hours)

- [ ] **5.3.1**: Hash Index TID Update (3-4 hours)
- [ ] **5.3.2**: Vector/HNSW Index TID Update (6-8 hours)
- [ ] **5.3.3**: Full-Text Index TID Update (4-6 hours)
- [ ] **5.3.4**: GIN Index TID Update (5-7 hours)
- [ ] **5.3.5**: GIST Index TID Update (4-6 hours)
- [ ] **5.3.6**: BRIN Index TID Update (3-4 hours)
- **Deliverable**: All 7 index types supported
- **Test**: Mixed index types on migrated table
```

### Phase 5.4: ONLINE Migration

```markdown
### **Phase 5.4: ONLINE Migration** (40-60 hours)

- [ ] **5.4.1**: Concurrent Read Support (8-12 hours)
- [ ] **5.4.2**: Concurrent Write Support (12-18 hours)
- [ ] **5.4.3**: Catch-Up Phase (10-15 hours)
- [ ] **5.4.4**: Final Swap (5-8 hours)
- [ ] **5.4.5**: Cleanup (5-7 hours)
- **Deliverable**: ONLINE migration fully functional
- **Test**: Concurrent workload during migration
```

---

## Success Criteria

### Phase 5.1 (OFFLINE Migration)
- ✅ Can migrate tables without indexes
- ✅ Can migrate tables with B-Tree indexes
- ✅ Can migrate tables with TOAST values
- ✅ Rollback works on error/cancellation
- ✅ All existing tests pass
- ✅ TID mapping correctly built and used

### Phase 5.2 (B-Tree Indexes)
- ✅ B-Tree indexes remain functional after migration
- ✅ Index scans return correct results
- ✅ REINDEX succeeds (no corruption)

### Phase 5.3 (All Index Types)
- ✅ All 7 index types supported
- ✅ Mixed index types on same table work

### Phase 5.4 (ONLINE Migration)
- ✅ Concurrent reads work during migration
- ✅ Concurrent writes work during migration
- ✅ No data loss or corruption
- ✅ Migration completes successfully under load

---

## Risk Mitigation

### High Risk Items

1. **TOAST Recursive Migration**: Complexity in handling circular references
   - **Mitigation**: Migrate TOAST table first, validate references

2. **Multi-Page Transactions**: Ensuring atomicity across page copies
   - **Mitigation**: Single transaction for entire migration (already decided)

3. **Index Corruption**: TID updates missing some entries
   - **Mitigation**: Comprehensive testing, REINDEX verification

4. **Memory Usage**: Large tables consuming excessive memory
   - **Mitigation**: Batch processing already implemented (Phase 4.1.4)

### Medium Risk Items

1. **Performance**: Migration taking too long for large tables
   - **Mitigation**: Batch size tuning, parallel processing (future)

2. **Disk Space**: Target tablespace running out of space mid-migration
   - **Mitigation**: Pre-check available space, rollback on error

---

## Performance Targets

### OFFLINE Migration

| Table Size | Pages | Estimated Time | Throughput |
|-----------|-------|---------------|-----------|
| 1 MB | 125 | ~1 second | 125 pages/sec |
| 100 MB | 12,500 | ~2 minutes | 100 pages/sec |
| 1 GB | 125,000 | ~20 minutes | 100 pages/sec |
| 10 GB | 1,250,000 | ~3.5 hours | 100 pages/sec |

**Assumptions**:
- 8KB pages
- SSD storage (~500 MB/s sequential I/O)
- Single-threaded migration
- Batch size: 1000 pages (~8 MB)

### ONLINE Migration

- **Downtime**: < 100ms (final swap phase)
- **Performance Impact**: < 20% during migration
- **Catch-up Time**: < 5% of total migration time

---

**Document Version**: 1.0
**Last Updated**: October 21, 2025
**Next Review**: Start of Phase 5 implementation
