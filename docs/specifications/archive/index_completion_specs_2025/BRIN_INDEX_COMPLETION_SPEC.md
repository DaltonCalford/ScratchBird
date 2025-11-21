# BRIN Index - Completion Specification

**Project**: ScratchBird Database Engine
**Component**: BRIN (Block Range Index) - Complete Remaining Features
**Current Status**: 50% Complete (Infrastructure only, critical features missing)
**Remaining Effort**: 60-100 hours
**Priority**: HIGH (Production features incomplete - vacuum, multi-page, revmap)

---

## ⚠️ CRITICAL: MGA COMPLIANCE REQUIRED

**BEFORE ANY WORK**: Read `/MGA_RULES.md` - Firebird MGA rules are ABSOLUTE.

**Key MGA Rules**:
- Use `TransactionId` (uint64_t), NOT `Snapshot` or `SnapshotData`
- All BRIN operations must respect xmin/xmax visibility
- TIP-based visibility via `TransactionManager::isVersionVisible()`
- NO PostgreSQL MVCC contamination
- Version traversal follows N2O (Newest-to-Oldest) chains

**If context is lost during compaction, re-read `/MGA_RULES.md` immediately.**

---

## Table of Contents

1. [Current Status](#1-current-status)
2. [Missing Feature 1: Vacuum/Compaction](#2-missing-feature-1-vacuumcompaction)
3. [Missing Feature 2: Multi-Page Support](#3-missing-feature-2-multi-page-support)
4. [Missing Feature 3: Revmap (Reverse Map)](#4-missing-feature-3-revmap-reverse-map)
5. [Missing Feature 4: Statistics](#5-missing-feature-4-statistics)
6. [Testing Requirements](#6-testing-requirements)
7. [Implementation Breakdown](#7-implementation-breakdown)

---

## 1. Current Status

### What Works (50% Complete)

**File**: `src/core/brin_index.cpp` (532 lines)

**Implemented Features**:
- ✅ BRIN page structure (SBBrinPage, SBBrinRange)
- ✅ Insert with summary updates (min/max tracking)
- ✅ Scan with range filtering (rangeOverlaps check)
- ✅ Min/max summary tracking
- ✅ MGA compliance structure (xmin/xmax fields)
- ✅ Basic visibility checking (isRangeVisible helper)
- ✅ Root page allocation and initialization
- ✅ Range creation and updates

### What's Missing (50% = 60-100 hours)

**Missing Feature 1**: Vacuum/compaction (Line 410-413)
- **Current**: Stub that identifies dead ranges but doesn't remove them
- **Required**: Remove dead ranges, compact page, reclaim free space
- **Impact**: Dead ranges accumulate, wasting space and slowing scans
- **Effort**: 30-40 hours

**Missing Feature 2**: Multi-page support (Phase 1 limitation noted in audit)
- **Current**: Single root page only (limited to ~50-100 ranges)
- **Required**: Handle tables larger than one BRIN page (sibling pointers, page allocation)
- **Impact**: Cannot index large tables (limited to ~6,400-12,800 blocks with range_size=128)
- **Effort**: 20-30 hours

**Missing Feature 3**: Revmap (reverse map) (Phase 2 feature noted in audit)
- **Current**: Linear scan to find range containing specific block
- **Required**: Fast O(1) lookup of range containing specific block
- **Impact**: INSERT/UPDATE performance degrades as index grows
- **Effort**: 20-30 hours

**Missing Feature 4**: Statistics (Line 495)
- **Current**: Returns placeholder values (avg_range_selectivity = 0.0)
- **Required**: Actual selectivity calculation based on range overlap
- **Impact**: Query planner cannot estimate BRIN effectiveness
- **Effort**: 5-10 hours

---

## 2. Missing Feature 1: Vacuum/Compaction

### 2.1 Problem Statement

**Current Code** (Lines 356-427):
```cpp
Status BrinIndex::vacuum(VacuumStats *stats_out, ErrorContext *ctx)
{
    // ... identify dead ranges ...

    for (uint16_t i = 0; i < page->brin_count; ++i)
    {
        SBBrinRange *range = reinterpret_cast<SBBrinRange*>(range_ptr);
        ranges_visited++;

        // Check if range is dead (xmax set and committed before oldest active)
        if (range->brn_xmax != 0 && range->brn_xmax < oldest_xid)
        {
            dead_ranges.push_back(i);
            ranges_removed++;
        }

        size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;
        range_ptr += range_size;
    }

    // Remove dead ranges (compact page)
    // TODO: Implement actual range removal and compaction  ← STUB!

    buffer_pool->unpinPage(index_info_.idx_root_page, ranges_removed > 0, ctx);
    // ... statistics ...
}
```

**Impact**:
- Dead ranges identified but never physically removed
- Page remains bloated with deleted ranges
- Free space not reclaimed
- Scan performance degrades (must skip dead ranges)

### 2.2 Solution: Physical Range Removal with Page Compaction

**Architecture**:
```
Vacuum Process:

1. Pin BRIN page
2. Scan ranges, identify dead (xmax < oldest_active_xid)
3. Build list of live ranges
4. Create temporary buffer with live ranges only
5. Clear page entry area
6. Copy compacted entries back to page
7. Update page metadata (count, free_space)
8. Clear HAS_GARBAGE flag
9. Mark page dirty
10. Update statistics
```

**Why This Is Important**:
- BRIN is designed for space efficiency (90%+ savings vs B-Tree)
- Dead ranges waste space, defeating the purpose
- Unlike B-Tree, BRIN pages are simpler (no tree balancing)
- Compaction is straightforward: copy live ranges, zero rest

### 2.3 Implementation Details

**Step 1: Implement vacuumPage() Helper** (15-20 hours)

```cpp
// Add to brin_index.h private section:
Status vacuumPage(uint64_t page_num,
                 uint64_t oldest_xid,
                 uint64_t* ranges_removed_out,
                 uint64_t* bytes_reclaimed_out,
                 ErrorContext* ctx);
```

**Step 2: Complete vacuum() with Physical Removal** (15-20 hours)

```cpp
// src/core/brin_index.cpp

Status BrinIndex::vacuum(VacuumStats *stats_out, ErrorContext *ctx)
{
    if (!db_)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No database");
        return Status::INVALID_ARGUMENT;
    }

    BufferPool *buffer_pool = db_->buffer_pool();
    TransactionManager *txn_mgr = db_->transaction_manager();

    if (!buffer_pool || !txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing components");
        return Status::INVALID_ARGUMENT;
    }

    uint64_t total_ranges_visited = 0;
    uint64_t total_ranges_removed = 0;
    uint64_t total_bytes_reclaimed = 0;

    // Get oldest active transaction
    uint64_t oldest_xid = txn_mgr->getOldestActiveXid();

    LOG_INFO(GENERAL, "BRIN vacuum starting: OAT=%lu", oldest_xid);

    // Vacuum root page (in Phase 1, only root exists)
    uint64_t ranges_removed = 0;
    uint64_t bytes_reclaimed = 0;
    Status status = vacuumPage(index_info_.idx_root_page, oldest_xid,
                                &ranges_removed, &bytes_reclaimed, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    total_ranges_removed += ranges_removed;
    total_bytes_reclaimed += bytes_reclaimed;

    // TODO: In Phase 2 (multi-page), traverse all BRIN pages via sibling pointers

    if (stats_out)
    {
        stats_out->ranges_visited = total_ranges_visited;
        stats_out->ranges_removed = total_ranges_removed;
        stats_out->ranges_updated = 0;
        stats_out->bytes_reclaimed = total_bytes_reclaimed;
    }

    LOG_INFO(GENERAL, "BRIN vacuum complete: removed %lu ranges, reclaimed %lu bytes",
             total_ranges_removed, total_bytes_reclaimed);

    return Status::OK;
}

Status BrinIndex::vacuumPage(uint64_t page_num,
                             uint64_t oldest_xid,
                             uint64_t* ranges_removed_out,
                             uint64_t* bytes_reclaimed_out,
                             ErrorContext* ctx)
{
    *ranges_removed_out = 0;
    *bytes_reclaimed_out = 0;

    BufferPool *buffer_pool = db_->buffer_pool();

    // Pin page
    void *page_buffer = nullptr;
    Status status = buffer_pool->pinPage(page_num, &page_buffer, ctx);
    if (status != Status::OK || !page_buffer)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin BRIN page");
        return Status::IO_ERROR;
    }

    uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
    SBBrinPage *page = reinterpret_cast<SBBrinPage*>(page_data);

    bool has_garbage = (page->brin_flags & static_cast<uint16_t>(BrinFlags::HAS_GARBAGE)) != 0;

    if (!has_garbage && page->brin_ranges_deleted == 0)
    {
        // No garbage, skip
        buffer_pool->unpinPage(page_num, false, ctx);
        return Status::OK;
    }

    // 1. Scan ranges and collect live ones
    std::vector<uint8_t> live_ranges_buffer;
    live_ranges_buffer.reserve(8192 - sizeof(SBBrinPage));

    uint8_t *range_ptr = page_data + sizeof(SBBrinPage);
    uint16_t original_count = page->brin_count;
    uint16_t live_count = 0;

    for (uint16_t i = 0; i < page->brin_count; ++i)
    {
        SBBrinRange *range = reinterpret_cast<SBBrinRange*>(range_ptr);
        size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;

        // Check if range is dead
        bool is_dead = (range->brn_xmax != 0) && (range->brn_xmax < oldest_xid);

        if (is_dead)
        {
            // Dead range - don't copy to live buffer
            (*ranges_removed_out)++;
            (*bytes_reclaimed_out) += range_size;

            LOG_DEBUG(GENERAL, "BRIN vacuum: Removing dead range [%u-%u], xmax=%lu",
                     range->brn_start_block, range->brn_end_block, range->brn_xmax);
        }
        else
        {
            // Live range - copy to buffer
            live_ranges_buffer.insert(live_ranges_buffer.end(),
                                     range_ptr,
                                     range_ptr + range_size);
            live_count++;
        }

        range_ptr += range_size;
    }

    // 2. Compact page if we removed any ranges
    if (live_count < original_count)
    {
        // Clear entry area
        size_t entry_area_start = sizeof(SBBrinPage);
        size_t entry_area_size = 8192 - sizeof(SBBrinPage);
        std::memset(page_data + entry_area_start, 0, entry_area_size);

        // Copy live ranges back
        if (!live_ranges_buffer.empty())
        {
            std::memcpy(page_data + entry_area_start,
                       live_ranges_buffer.data(),
                       live_ranges_buffer.size());
        }

        // Update page metadata
        page->brin_count = live_count;
        page->brin_free_space = entry_area_size - live_ranges_buffer.size();
        page->brin_ranges_deleted -= (*ranges_removed_out);

        // Clear HAS_GARBAGE flag if page is now clean
        if (page->brin_ranges_deleted == 0)
        {
            page->brin_flags &= ~static_cast<uint16_t>(BrinFlags::HAS_GARBAGE);
        }

        // Mark page dirty
        buffer_pool->unpinPage(page_num, true, ctx);

        LOG_DEBUG(GENERAL, "BRIN vacuum: Page %lu compacted: %u live ranges (was %u)",
                 page_num, live_count, original_count);
    }
    else
    {
        // No changes
        buffer_pool->unpinPage(page_num, false, ctx);
    }

    return Status::OK;
}
```

### 2.4 Testing Requirements

**Unit Tests** (`tests/unit/test_brin_vacuum.cpp`):
1. [ ] Insert 100 ranges, mark 50 deleted, vacuum, verify 50 removed
2. [ ] Vacuum with no dead ranges (should be no-op)
3. [ ] Vacuum removes only ranges with xmax < oldest_active_xid
4. [ ] Vacuum preserves ranges visible to active transactions
5. [ ] Verify page free_space increases after vacuum
6. [ ] Verify HAS_GARBAGE flag cleared after vacuum
7. [ ] Verify statistics (ranges_removed, bytes_reclaimed) correct
8. [ ] Scan after vacuum returns only live ranges

**Acceptance Criteria**:
- All 8 tests pass
- Dead ranges physically removed from pages
- Free space correctly reclaimed
- Active transactions not affected by vacuum
- Statistics accurate

---

## 3. Missing Feature 2: Multi-Page Support

### 3.1 Problem Statement

**Current Limitation**:
- BRIN index uses only single root page
- Page capacity: ~50-100 ranges (depending on min/max value sizes)
- With range_size=128 blocks, covers ~6,400-12,800 blocks
- Cannot index tables larger than ~50-100 MB (8KB pages)

**Impact**:
- BRIN unusable for large time-series tables (terabytes of data)
- Defeats primary use case (large append-only tables)
- Page overflow on large tables

### 3.2 Solution: Multi-Page BRIN with Sibling Pointers

**Architecture**:
```
Multi-Page BRIN Index:

Root Page (Ranges 0-99):
┌────────────────────────────────────┐
│ Range 0: Blocks 0-127              │
│ Range 1: Blocks 128-255            │
│ ...                                │
│ Range 99: Blocks 12,672-12,799     │
│ brin_right_sibling: Page 42        │ ← Points to next page
└────────────────────────────────────┘

Page 42 (Ranges 100-199):
┌────────────────────────────────────┐
│ Range 100: Blocks 12,800-12,927    │
│ Range 101: Blocks 12,928-13,055    │
│ ...                                │
│ brin_left_sibling: Root Page       │ ← Back pointer
│ brin_right_sibling: Page 57        │ ← Next page
└────────────────────────────────────┘

Page 57 (Ranges 200-299):
┌────────────────────────────────────┐
│ Range 200: Blocks 25,600-25,727    │
│ ...                                │
│ brin_left_sibling: Page 42         │
│ brin_right_sibling: 0 (end)        │
└────────────────────────────────────┘
```

**Key Design Decisions**:
- Linked list of pages (left/right sibling pointers)
- No tree structure needed (ranges are sequential by block number)
- Page allocation when root page full
- Scan traverses sibling chain

### 3.3 Implementation Details

**Step 1: Implement splitPage() for Page Overflow** (10-15 hours)

```cpp
// Add to brin_index.h private section:
Status splitPage(uint64_t page_num, ErrorContext* ctx);
```

**Step 2: Modify insert() to Handle Page Overflow** (10-15 hours)

```cpp
// src/core/brin_index.cpp

Status BrinIndex::insert(const std::vector<uint8_t> &value,
                        uint32_t block_number,
                        ErrorContext *ctx)
{
    if (!db_)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No database");
        return Status::INVALID_ARGUMENT;
    }

    BufferPool *buffer_pool = db_->buffer_pool();
    TransactionManager *txn_mgr = db_->transaction_manager();

    if (!buffer_pool || !txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing components");
        return Status::INVALID_ARGUMENT;
    }

    // Calculate which range this block belongs to
    uint32_t range_index = block_number / index_info_.idx_range_size;
    uint32_t range_start = range_index * index_info_.idx_range_size;
    uint32_t range_end = range_start + index_info_.idx_range_size - 1;

    // Find the page containing this range (traverse sibling chain)
    uint64_t target_page = index_info_.idx_root_page;
    bool found_page = false;

    while (target_page != 0)
    {
        void *page_buffer = nullptr;
        Status status = buffer_pool->pinPage(target_page, &page_buffer, ctx);
        if (status != Status::OK || !page_buffer)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin BRIN page");
            return Status::IO_ERROR;
        }

        uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
        SBBrinPage *page = reinterpret_cast<SBBrinPage*>(page_data);

        // Check if this page covers the block range
        if (range_start >= page->brin_first_block &&
            (page->brin_last_block == 0 || range_end <= page->brin_last_block))
        {
            // This page should contain the range
            found_page = true;
            buffer_pool->unpinPage(target_page, false, ctx);
            break;
        }

        // Check if we need to add to this page (last page in chain)
        if (page->brin_right_sibling == 0 && range_start > page->brin_last_block)
        {
            // This is the last page, and we need to add a range beyond it
            found_page = true;
            buffer_pool->unpinPage(target_page, false, ctx);
            break;
        }

        // Move to next page
        uint64_t next_page = page->brin_right_sibling;
        buffer_pool->unpinPage(target_page, false, ctx);
        target_page = next_page;
    }

    if (!found_page)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Could not find BRIN page for block");
        return Status::INTERNAL_ERROR;
    }

    // Pin the target page for modification
    void *page_buffer = nullptr;
    Status status = buffer_pool->pinPage(target_page, &page_buffer, ctx);
    if (status != Status::OK || !page_buffer)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin BRIN page");
        return Status::IO_ERROR;
    }

    uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
    SBBrinPage *page = reinterpret_cast<SBBrinPage*>(page_data);

    // Find existing range or add new one
    uint8_t *range_ptr = page_data + sizeof(SBBrinPage);
    bool found = false;
    bool updated = false;

    for (uint16_t i = 0; i < page->brin_count; ++i)
    {
        SBBrinRange *range = reinterpret_cast<SBBrinRange*>(range_ptr);

        if (range->brn_start_block == range_start)
        {
            // Found the range, update min/max
            // ... (existing update logic) ...
            found = true;
            break;
        }

        size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;
        range_ptr += range_size;
    }

    if (!found)
    {
        // Add new range
        uint16_t value_len = std::min(static_cast<size_t>(value.size()), static_cast<size_t>(256));
        size_t new_range_size = sizeof(SBBrinRange) + value_len * 2;

        if (page->brin_free_space < new_range_size)
        {
            // Page full - need to split
            buffer_pool->unpinPage(target_page, false, ctx);

            status = splitPage(target_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Retry insert after split
            return insert(value, block_number, ctx);
        }

        // Create new range (existing logic)
        // ...

        // Update page metadata for block range coverage
        if (page->brin_last_block == 0 || range_end > page->brin_last_block)
        {
            page->brin_last_block = range_end;
        }
        if (page->brin_first_block == 0 || range_start < page->brin_first_block)
        {
            page->brin_first_block = range_start;
        }

        updated = true;
    }

    buffer_pool->unpinPage(target_page, updated, ctx);

    return Status::OK;
}

Status BrinIndex::splitPage(uint64_t page_num, ErrorContext* ctx)
{
    BufferPool *buffer_pool = db_->buffer_pool();
    PageManager *page_mgr = db_->page_manager();
    TransactionManager *txn_mgr = db_->transaction_manager();

    // Pin current page
    void *page_buffer = nullptr;
    Status status = buffer_pool->pinPage(page_num, &page_buffer, ctx);
    if (status != Status::OK || !page_buffer)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin BRIN page");
        return Status::IO_ERROR;
    }

    uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
    SBBrinPage *page = reinterpret_cast<SBBrinPage*>(page_data);

    // Allocate new right sibling page
    uint32_t new_page_num = 0;
    status = page_mgr->allocatePage(new_page_num, ctx);
    if (status != Status::OK)
    {
        buffer_pool->unpinPage(page_num, false, ctx);
        return status;
    }

    // Pin new page
    void *new_page_buffer = nullptr;
    status = buffer_pool->pinPage(new_page_num, &new_page_buffer, ctx);
    if (status != Status::OK || !new_page_buffer)
    {
        buffer_pool->unpinPage(page_num, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin new BRIN page");
        return Status::IO_ERROR;
    }

    uint8_t *new_page_data = static_cast<uint8_t*>(new_page_buffer);
    SBBrinPage *new_page = reinterpret_cast<SBBrinPage*>(new_page_data);
    std::memset(new_page, 0, sizeof(SBBrinPage));

    // Initialize new page
    new_page->brin_index_uuid = page->brin_index_uuid;
    new_page->brin_table_uuid = page->brin_table_uuid;
    new_page->brin_flags = 0; // Not root
    new_page->brin_count = 0;
    new_page->brin_free_space = 8192 - sizeof(SBBrinPage);
    new_page->brin_range_size = page->brin_range_size;
    new_page->brin_first_block = 0;
    new_page->brin_last_block = 0;
    new_page->brin_xmin = txn_mgr->getCurrentXid();
    new_page->brin_xmax = 0;
    new_page->brin_ranges_total = 0;
    new_page->brin_ranges_deleted = 0;

    // Set up sibling pointers
    new_page->brin_left_sibling = page_num;
    new_page->brin_right_sibling = page->brin_right_sibling;
    page->brin_right_sibling = new_page_num;

    // Split ranges (move half to new page)
    uint16_t split_point = page->brin_count / 2;
    uint8_t *range_ptr = page_data + sizeof(SBBrinPage);

    // Calculate offset to split point
    size_t split_offset = 0;
    for (uint16_t i = 0; i < split_point; ++i)
    {
        SBBrinRange *range = reinterpret_cast<SBBrinRange*>(range_ptr);
        size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;
        split_offset += range_size;
        range_ptr += range_size;
    }

    // Copy second half to new page
    size_t second_half_size = (8192 - sizeof(SBBrinPage)) - page->brin_free_space - split_offset;
    std::memcpy(new_page_data + sizeof(SBBrinPage),
               page_data + sizeof(SBBrinPage) + split_offset,
               second_half_size);

    // Update metadata
    new_page->brin_count = page->brin_count - split_point;
    new_page->brin_free_space = (8192 - sizeof(SBBrinPage)) - second_half_size;

    page->brin_count = split_point;
    page->brin_free_space = (8192 - sizeof(SBBrinPage)) - split_offset;

    // Update block range coverage (scan ranges to find first/last block)
    // ... (iterate ranges on each page to set brin_first_block, brin_last_block) ...

    // Mark pages dirty
    buffer_pool->unpinPage(page_num, true, ctx);
    buffer_pool->unpinPage(new_page_num, true, ctx);

    LOG_INFO(GENERAL, "BRIN page %lu split: %u ranges on left, %u on right (new page %u)",
             page_num, page->brin_count, new_page->brin_count, new_page_num);

    return Status::OK;
}
```

### 3.4 Testing Requirements

**Unit Tests** (`tests/unit/test_brin_multipage.cpp`):
1. [ ] Insert 200 ranges, forcing page split
2. [ ] Verify left/right sibling pointers correct after split
3. [ ] Verify ranges correctly distributed after split
4. [ ] Scan across multiple pages returns all matching ranges
5. [ ] Insert into middle page (not root, not last)
6. [ ] Vacuum across multiple pages
7. [ ] Sequential scan traverses all pages in order
8. [ ] Handle 1000+ ranges (10+ pages)

**Acceptance Criteria**:
- All 8 tests pass
- Page splits work correctly
- Sibling pointers maintain correct chain
- Scan works across multiple pages
- No data loss during split

---

## 4. Missing Feature 3: Revmap (Reverse Map)

### 4.1 Problem Statement

**Current Limitation**:
- To find which range contains block N, must linear scan all ranges
- O(n) lookup time where n = number of ranges
- INSERT/UPDATE performance degrades as index grows
- With 10,000 ranges, every insert scans 10,000 entries

**Impact**:
- Slow inserts on large indexes
- Defeats purpose of BRIN (fast inserts for append-only workloads)

### 4.2 Solution: Revmap for O(1) Range Lookup

**Architecture**:
```
Revmap (Reverse Map):

Compact array mapping block_number → range_page_offset

revmap[block_number / range_size] = (page_num, offset_in_page)

Example (range_size=128):
  Block 0-127    → revmap[0] = (Root Page, offset 0)
  Block 128-255  → revmap[1] = (Root Page, offset 128)
  Block 256-383  → revmap[2] = (Root Page, offset 256)
  ...
  Block 12800+   → revmap[100] = (Page 42, offset 0)

Lookup: O(1) array access instead of O(n) linear scan
```

**Revmap Storage**:
- Separate page type: `BRIN_REVMAP_PAGE`
- Each entry: 12 bytes (8 bytes page_num + 4 bytes offset)
- One revmap page covers ~680 ranges (~87,000 blocks with range_size=128)
- For 1 million ranges: ~1,500 revmap pages (~12 MB)

**Why This Is Important**:
- PostgreSQL BRIN uses revmap for fast lookups
- Essential for production use with large indexes
- Enables efficient INSERT/UPDATE on multi-million row tables

### 4.3 Implementation Details

**Step 1: Define Revmap Page Structure** (3-5 hours)

```cpp
// Add to brin_index.h or ondisk.h

#pragma pack(push, 1)
/**
 * BRIN Revmap page structure
 *
 * Stores (page_num, offset) pairs for fast range lookup.
 * Array index is range_index = block_number / range_size.
 */
struct SBBrinRevmapPage
{
    PageHeader revmap_header;        // Standard page header
    ID revmap_index_uuid;            // Index UUID
    uint32_t revmap_min_range_index; // Minimum range index on this page
    uint32_t revmap_max_range_index; // Maximum range index on this page
    uint16_t revmap_count;           // Number of entries
    uint16_t revmap_free_space;      // Free space
    uint64_t revmap_xmin;            // Page creation transaction
    uint64_t revmap_xmax;            // Page deletion transaction

    uint8_t revmap_padding[32];      // Reserved

    // Entries follow (array of SBBrinRevmapEntry)
};

/**
 * Revmap entry: maps range_index → (page_num, offset)
 */
struct SBBrinRevmapEntry
{
    uint64_t entry_page_num;    // BRIN data page containing range
    uint32_t entry_offset;      // Byte offset within page to SBBrinRange
    uint32_t entry_reserved;    // Reserved for future use
};
#pragma pack(pop)
```

**Step 2: Implement Revmap Lookup** (10-15 hours)

```cpp
// Add to brin_index.h private section:
Status lookupRevmap(uint32_t block_number,
                   uint64_t* page_num_out,
                   uint32_t* offset_out,
                   ErrorContext* ctx);

Status updateRevmap(uint32_t block_number,
                   uint64_t page_num,
                   uint32_t offset,
                   ErrorContext* ctx);

// Implementation:
Status BrinIndex::lookupRevmap(uint32_t block_number,
                               uint64_t* page_num_out,
                               uint32_t* offset_out,
                               ErrorContext* ctx)
{
    // Calculate range index
    uint32_t range_index = block_number / index_info_.idx_range_size;

    // Calculate which revmap page contains this range index
    const uint32_t REVMAP_ENTRIES_PER_PAGE =
        (8192 - sizeof(SBBrinRevmapPage)) / sizeof(SBBrinRevmapEntry);

    uint32_t revmap_page_index = range_index / REVMAP_ENTRIES_PER_PAGE;
    uint32_t entry_offset_in_page = range_index % REVMAP_ENTRIES_PER_PAGE;

    // Get revmap root page (stored in index metadata)
    uint32_t revmap_root = index_info_.idx_revmap_root_page;
    if (revmap_root == 0)
    {
        // Revmap not initialized yet
        *page_num_out = 0;
        *offset_out = 0;
        return Status::OK;
    }

    // Traverse to correct revmap page (revmap pages form linked list)
    uint64_t current_revmap_page = revmap_root;
    for (uint32_t i = 0; i < revmap_page_index && current_revmap_page != 0; ++i)
    {
        // Pin page, read next pointer, unpin
        // ... (implementation) ...
    }

    if (current_revmap_page == 0)
    {
        // Revmap page doesn't exist yet
        *page_num_out = 0;
        *offset_out = 0;
        return Status::OK;
    }

    // Pin revmap page
    void *page_buffer = nullptr;
    BufferPool *buffer_pool = db_->buffer_pool();
    Status status = buffer_pool->pinPage(current_revmap_page, &page_buffer, ctx);
    if (status != Status::OK || !page_buffer)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin revmap page");
        return Status::IO_ERROR;
    }

    uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
    SBBrinRevmapPage *revmap = reinterpret_cast<SBBrinRevmapPage*>(page_data);

    // Read entry
    SBBrinRevmapEntry *entries =
        reinterpret_cast<SBBrinRevmapEntry*>(page_data + sizeof(SBBrinRevmapPage));

    if (entry_offset_in_page < revmap->revmap_count)
    {
        *page_num_out = entries[entry_offset_in_page].entry_page_num;
        *offset_out = entries[entry_offset_in_page].entry_offset;
    }
    else
    {
        // Entry not yet created
        *page_num_out = 0;
        *offset_out = 0;
    }

    buffer_pool->unpinPage(current_revmap_page, false, ctx);

    return Status::OK;
}
```

**Step 3: Modify insert() to Use Revmap** (7-10 hours)

```cpp
// In insert(), replace linear scan with revmap lookup:

Status BrinIndex::insert(const std::vector<uint8_t> &value,
                        uint32_t block_number,
                        ErrorContext *ctx)
{
    // ... validation ...

    // Use revmap for O(1) lookup
    uint64_t target_page = 0;
    uint32_t target_offset = 0;
    Status status = lookupRevmap(block_number, &target_page, &target_offset, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (target_page == 0)
    {
        // Range doesn't exist yet, create it
        // ... (existing logic to find appropriate page and create range) ...

        // Update revmap after creating range
        status = updateRevmap(block_number, created_page, created_offset, ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }
    else
    {
        // Range exists at known location
        // Pin page, find range at offset, update min/max
        // ... (optimized path) ...
    }

    return Status::OK;
}
```

### 4.4 Testing Requirements

**Unit Tests** (`tests/unit/test_brin_revmap.cpp`):
1. [ ] Create index with revmap, insert 1000 ranges
2. [ ] Verify revmap lookup returns correct page/offset
3. [ ] Insert using revmap is faster than linear scan (benchmark)
4. [ ] Update existing range via revmap
5. [ ] Revmap handles multi-page revmap (10,000+ ranges)
6. [ ] Revmap correctly updated after page split
7. [ ] Scan works correctly with revmap enabled
8. [ ] Vacuum updates revmap after range removal

**Acceptance Criteria**:
- All 8 tests pass
- Revmap lookup is O(1) (< 1ms for any range index)
- Insert performance scales O(1), not O(n)
- Revmap stays consistent with actual ranges

---

## 5. Missing Feature 4: Statistics

### 5.1 Problem Statement

**Current Code** (Lines 464-500):
```cpp
Status BrinIndex::getStats(BrinStats *stats_out, ErrorContext *ctx)
{
    // ... load page ...

    stats_out->total_ranges = page->brin_ranges_total;
    stats_out->deleted_ranges = page->brin_ranges_deleted;
    stats_out->total_pages = 1; // Simplified: only root page
    stats_out->blocks_covered = page->brin_last_block - page->brin_first_block + 1;
    stats_out->avg_range_selectivity = 0.0; // TODO: Calculate  ← PLACEHOLDER!

    buffer_pool->unpinPage(index_info_.idx_root_page, false, ctx);

    return Status::OK;
}
```

**Impact**:
- Query planner cannot estimate BRIN index selectivity
- Cannot determine if BRIN is effective for a given query
- No metrics for monitoring index quality

### 5.2 Solution: Calculate Average Range Selectivity

**What is Range Selectivity?**
```
Selectivity = (Ranges that pass filter) / (Total ranges)

Example:
  1000 ranges total
  Query: WHERE value BETWEEN 100 AND 200
  50 ranges overlap [100, 200]
  Selectivity = 50 / 1000 = 0.05 (5%)

Lower selectivity = more effective index (fewer blocks scanned)
```

**Architecture**:
```
Statistics Collection:

1. Track query predicates during scans
2. Calculate overlap percentage for each scan
3. Maintain rolling average of selectivity
4. Store in catalog or index metadata
5. Query planner uses selectivity for cost estimation
```

### 5.3 Implementation Details

**Step 1: Add Selectivity Tracking** (3-5 hours)

```cpp
// Add to SBBrinIndex structure:
struct SBBrinIndex
{
    // ... existing fields ...

    double idx_avg_selectivity;      // Rolling average selectivity
    uint64_t idx_total_scans;        // Number of scans performed
    uint64_t idx_total_blocks_scanned; // Total blocks scanned
};

// Add to brin_index.h private section:
void updateSelectivityStats(uint64_t ranges_matched,
                           uint64_t total_ranges,
                           uint64_t blocks_returned);
```

**Step 2: Calculate Selectivity in scan()** (2-5 hours)

```cpp
// Modify scan() to track statistics:

Status BrinIndex::scan(const std::vector<uint8_t> *min_value,
                      const std::vector<uint8_t> *max_value,
                      uint64_t current_xid,
                      std::vector<uint32_t> *block_numbers_out,
                      ErrorContext *ctx)
{
    // ... existing scan logic ...

    uint64_t total_ranges = 0;
    uint64_t matched_ranges = 0;
    uint64_t blocks_returned = 0;

    // Scan all ranges
    uint8_t *range_ptr = page_data + sizeof(SBBrinPage);

    for (uint16_t i = 0; i < page->brin_count; ++i)
    {
        SBBrinRange *range = reinterpret_cast<SBBrinRange*>(range_ptr);
        total_ranges++;

        // Check visibility
        if (!isRangeVisible(range->brn_xmin, range->brn_xmax, current_xid, txn_mgr))
        {
            // ... skip ...
            continue;
        }

        // Extract min/max values
        // ...

        // Check if range overlaps with query
        if (BrinMinmaxOps::rangeOverlaps(range_min, range_max, min_value, max_value))
        {
            matched_ranges++;

            // Add all blocks in this range
            for (uint32_t block = range->brn_start_block;
                 block <= range->brn_end_block; ++block)
            {
                block_numbers_out->push_back(block);
                blocks_returned++;
            }
        }

        // ... next range ...
    }

    buffer_pool->unpinPage(index_info_.idx_root_page, false, ctx);

    // Update statistics
    updateSelectivityStats(matched_ranges, total_ranges, blocks_returned);

    LOG_INFO(GENERAL, "BRIN scan: %zu blocks from %lu/%lu ranges (selectivity: %.2f%%)",
             blocks_returned, matched_ranges, total_ranges,
             total_ranges > 0 ? (matched_ranges * 100.0 / total_ranges) : 0.0);

    return Status::OK;
}

void BrinIndex::updateSelectivityStats(uint64_t ranges_matched,
                                       uint64_t total_ranges,
                                       uint64_t blocks_returned)
{
    if (total_ranges == 0) return;

    double current_selectivity = static_cast<double>(ranges_matched) / total_ranges;

    // Update rolling average (exponential moving average, alpha=0.2)
    if (index_info_.idx_total_scans == 0)
    {
        index_info_.idx_avg_selectivity = current_selectivity;
    }
    else
    {
        index_info_.idx_avg_selectivity =
            0.8 * index_info_.idx_avg_selectivity + 0.2 * current_selectivity;
    }

    index_info_.idx_total_scans++;
    index_info_.idx_total_blocks_scanned += blocks_returned;

    // Persist to catalog periodically (e.g., every 100 scans)
    if (index_info_.idx_total_scans % 100 == 0)
    {
        // ... update catalog ...
    }
}
```

**Step 3: Update getStats() to Return Real Values** (1-2 hours)

```cpp
Status BrinIndex::getStats(BrinStats *stats_out, ErrorContext *ctx)
{
    // ... existing logic ...

    stats_out->total_ranges = page->brin_ranges_total;
    stats_out->deleted_ranges = page->brin_ranges_deleted;
    stats_out->total_pages = 1; // TODO: Count actual pages in multi-page implementation
    stats_out->blocks_covered = page->brin_last_block - page->brin_first_block + 1;
    stats_out->avg_range_selectivity = index_info_.idx_avg_selectivity; // REAL VALUE!

    buffer_pool->unpinPage(index_info_.idx_root_page, false, ctx);

    return Status::OK;
}
```

### 5.4 Testing Requirements

**Unit Tests** (`tests/unit/test_brin_stats.cpp`):
1. [ ] Insert 100 ranges, verify total_ranges = 100
2. [ ] Delete 20 ranges, verify deleted_ranges = 20
3. [ ] Scan with selective predicate, verify selectivity < 0.2
4. [ ] Scan with broad predicate, verify selectivity > 0.8
5. [ ] Verify avg_range_selectivity updates after each scan
6. [ ] Verify statistics persist across index reopen
7. [ ] Multi-page index: verify total_pages count
8. [ ] Vacuum updates statistics correctly

**Acceptance Criteria**:
- All 8 tests pass
- Selectivity accurately reflects query overlap
- Statistics updated in real-time during scans
- Query planner can use statistics

---

## 6. Testing Requirements

### 6.1 Unit Tests

**New Test Files**:
1. `tests/unit/test_brin_vacuum.cpp` (8 tests)
2. `tests/unit/test_brin_multipage.cpp` (8 tests)
3. `tests/unit/test_brin_revmap.cpp` (8 tests)
4. `tests/unit/test_brin_stats.cpp` (8 tests)

**Total**: 32 new tests

### 6.2 Integration Tests

**Existing Integration Tests** (`tests/integration/test_brin_index.cpp`):
- [ ] Verify all existing tests still pass after changes
- [ ] Add stress test: 100,000 inserts (time-series data), verify index works
- [ ] Add vacuum test: 50,000 inserts, 25,000 deletes, vacuum, verify space reclaimed
- [ ] Add selectivity test: queries with varying ranges, verify statistics accurate
- [ ] Add multi-page test: 1 million rows, verify correct page splits

### 6.3 Performance Benchmarks

**Benchmarks** (`tests/benchmark/benchmark_brin_index.cpp`):
- [ ] Insert throughput: sequential inserts (should be O(1) per insert)
- [ ] Insert throughput: random inserts (compare with/without revmap)
- [ ] Scan latency: selective query (10% selectivity)
- [ ] Scan latency: broad query (90% selectivity)
- [ ] Vacuum duration: 100K ranges, 50% deleted
- [ ] Space efficiency: BRIN vs B-Tree (should be 90%+ savings)
- [ ] Revmap lookup time: O(1) vs O(n) linear scan

---

## 7. Implementation Breakdown

### 7.1 Task Breakdown

| Task | Effort (hours) | Dependency |
|------|----------------|------------|
| **Feature 1: Vacuum/Compaction** | **30-40** | - |
| 1.1 Implement vacuumPage() helper | 15-20 | - |
| 1.2 Complete vacuum() with physical removal | 10-15 | 1.1 |
| 1.3 Unit tests for vacuum operations | 5-8 | 1.1, 1.2 |
| **Feature 2: Multi-Page Support** | **20-30** | - |
| 2.1 Implement splitPage() for overflow | 10-15 | - |
| 2.2 Modify insert() to handle multi-page | 5-10 | 2.1 |
| 2.3 Update scan() to traverse sibling chain | 3-5 | 2.1 |
| 2.4 Unit tests for multi-page operations | 5-8 | 2.1, 2.2, 2.3 |
| **Feature 3: Revmap** | **20-30** | - |
| 3.1 Define revmap page structure | 3-5 | - |
| 3.2 Implement revmap lookup | 10-15 | 3.1 |
| 3.3 Modify insert() to use revmap | 5-8 | 3.2 |
| 3.4 Unit tests for revmap operations | 5-8 | 3.1, 3.2, 3.3 |
| **Feature 4: Statistics** | **5-10** | - |
| 4.1 Add selectivity tracking | 3-5 | - |
| 4.2 Calculate selectivity in scan() | 2-5 | 4.1 |
| 4.3 Update getStats() | 1-2 | 4.2 |
| 4.4 Unit tests for statistics | 2-3 | 4.1, 4.2, 4.3 |
| **Integration & Performance** | **5-10** | All |
| 5.1 Integration test updates | 3-5 | All |
| 5.2 Performance benchmarks | 2-5 | All |
| **TOTAL** | **80-120** | - |

**Note**: Upper bound is conservative estimate including debugging and edge cases.

### 7.2 Estimated Total Effort

**Realistic Estimate**: 60-100 hours

**Breakdown**:
- Optimistic (experienced developer, no issues): 60 hours
- Realistic (normal development, some debugging): 75-85 hours
- Conservative (includes testing, edge cases, documentation): 100 hours

**Timeline**:
- Single developer (full-time): 1.5-2.5 weeks
- Part-time: 3-5 weeks

### 7.3 Critical Path

**Dependencies**:
1. Feature 2 (Multi-Page) should be done before Feature 3 (Revmap)
   - Revmap needs to handle multi-page scenarios
2. Feature 1 (Vacuum) can be done in parallel
3. Feature 4 (Statistics) can be done in parallel

**Recommended Order**:
1. Feature 1 (Vacuum) - CRITICAL for production (prevents unbounded growth)
2. Feature 2 (Multi-Page) - CRITICAL for large tables
3. Feature 3 (Revmap) - IMPORTANT for performance
4. Feature 4 (Statistics) - NICE TO HAVE (query planner optimization)

### 7.4 Phased Implementation Plan

**Phase 1 (Critical - 30-50 hours)**:
- Feature 1: Vacuum/Compaction (30-40 hours)
  - **Blocker**: Without vacuum, index grows unbounded
- Feature 2: Multi-Page Support (20-30 hours)
  - **Blocker**: Without multi-page, cannot index large tables

**Phase 2 (Important - 20-30 hours)**:
- Feature 3: Revmap (20-30 hours)
  - **Performance**: Insert degrades from O(1) to O(n) without revmap

**Phase 3 (Nice-to-Have - 5-10 hours)**:
- Feature 4: Statistics (5-10 hours)
  - **Optimization**: Query planner uses for cost estimation

---

## 8. MGA Compliance Checklist

**All BRIN operations must respect MGA rules:**

- [x] Current implementation uses `TransactionId` (uint64_t), not `Snapshot`
- [x] Visibility checks use `isRangeVisible()` with TIP lookups
- [x] Scan operations pass `current_xid`, not `snapshot`
- [x] Range summaries store xmin/xmax
- [ ] vacuum() must respect oldest_active_xid (TO COMPLETE)
- [x] Ranges reference stable block numbers (never change)
- [x] MGA version chains respected (ranges are versioned)

**New Code Requirements**:
- [ ] vacuumPage() must only remove ranges where `xmax < oldest_active_xid`
- [ ] isRangeVisible() must use `TransactionManager::isVersionVisible()`
- [ ] No use of `Snapshot` structures anywhere
- [ ] All transaction state checks via `TransactionManager` TIP lookups
- [ ] Revmap must handle range versioning (xmin/xmax on revmap entries)

**Reference**: See `/MGA_RULES.md` Section 4 (Visibility Rules)

---

## 9. Dependencies and Assumptions

### 9.1 External Dependencies

**Requires**:
- `TransactionManager::isVersionVisible()` - For MGA visibility checks
- `TransactionManager::getCurrentXid()` - For xmin assignment
- `TransactionManager::getOldestActiveXid()` - For vacuum
- `BufferPool::pinPage()` / `unpinPage()` - For page access
- `BufferPool::markPageDirty()` - For write-ahead logging
- `PageManager::allocatePage()` - For multi-page support

**Assumes**:
- Buffer pool is thread-safe
- Transaction manager TIP is correctly maintained
- Page manager can allocate BRIN pages

### 9.2 BRIN-Specific Requirements

**Data Type Support**:
- Must support min/max comparisons for indexed types
- Currently: numeric, date/time types
- Future: string types (lexicographic order), UUID

**Range Size**:
- Default: 128 blocks
- Configurable at index creation
- Smaller ranges = more selective, larger index
- Larger ranges = less selective, smaller index

---

## 10. Known Limitations and Future Work

### 10.1 Current Limitations (After Completion)

**Not Included in This Spec**:
- Inclusion operators (e.g., CONTAINS, OVERLAPS for ranges)
- Bloom filters per range (for NULL checks, cardinality estimates)
- Compression of range summaries (delta encoding)
- Parallel vacuum (currently single-threaded)
- Incremental revmap updates (currently rebuilt on page split)

**These are OK for Phase 1, can be added later.**

### 10.2 Future Enhancements

**Phase 2 Enhancements** (not required now):
1. **Inclusion Operators** (15-20 hours)
   - Support CONTAINS, OVERLAPS for range types
   - Enables more complex queries

2. **Bloom Filters per Range** (20-30 hours)
   - Store Bloom filter for each range
   - Faster NULL checks, cardinality estimates

3. **Range Summary Compression** (10-15 hours)
   - Delta encode min/max values
   - Saves space for sequential data

4. **Parallel Vacuum** (15-20 hours)
   - Vacuum multiple pages concurrently
   - Faster vacuum on large indexes

5. **Incremental Revmap** (10-15 hours)
   - Update only affected revmap entries on page split
   - Faster page splits

---

## 11. Acceptance Criteria

### 11.1 Functional Requirements

**Must Pass**:
- [ ] All 32 unit tests pass
- [ ] All integration tests pass
- [ ] Can insert 100,000 ranges without error
- [ ] Vacuum removes all dead ranges (100% reclaimed)
- [ ] Multi-page support handles 10,000+ ranges
- [ ] Revmap lookup is O(1) (< 1ms for any range)
- [ ] Statistics accurately reflect selectivity
- [ ] Scan returns correct results after vacuum

### 11.2 Performance Requirements

**Must Achieve**:
- [ ] Insert throughput: >100,000 inserts/sec (sequential, with revmap)
- [ ] Scan latency: <10ms for 100K range index (10% selectivity)
- [ ] Vacuum duration: <5 seconds for 100K ranges (50% dead)
- [ ] Space efficiency: >90% savings vs B-Tree (for time-series data)
- [ ] Revmap lookup: <1ms (any range index)
- [ ] Multi-page overhead: <5% performance degradation vs single-page

### 11.3 MGA Compliance Requirements

**Must Verify**:
- [ ] Deleted ranges visible to old transactions (snapshot isolation)
- [ ] Deleted ranges invisible to new transactions
- [ ] Vacuum only removes ranges invisible to all active transactions
- [ ] No `Snapshot` structures used anywhere
- [ ] All visibility checks via `TransactionManager::isVersionVisible()`
- [ ] Revmap handles range versioning correctly

---

## 12. Code Examples

### 12.1 Example: Using BRIN Index (SQL)

```sql
-- Create BRIN index on time-series table
CREATE INDEX idx_logs_timestamp
ON logs USING BRIN (timestamp)
WITH (range_size = 128);

-- Query using BRIN index
SELECT * FROM logs
WHERE timestamp BETWEEN '2025-01-01' AND '2025-01-31';

-- BRIN scans only ranges overlapping date range
-- Returns block numbers to heap scan
-- 90%+ space savings vs B-Tree
```

### 12.2 Example: Vacuum BRIN Index

```cpp
// Vacuum BRIN index
BrinIndex::VacuumStats stats;
Status status = brin_index->vacuum(&stats, &ctx);
if (status != Status::OK) {
    LOG_ERROR(GENERAL, "BRIN vacuum failed: %s", ctx.message.c_str());
}

LOG_INFO(GENERAL, "BRIN vacuum: removed %lu ranges, reclaimed %lu bytes",
         stats.ranges_removed, stats.bytes_reclaimed);
```

### 12.3 Example: Revmap Lookup

```cpp
// Look up which range contains block 5000
uint64_t page_num = 0;
uint32_t offset = 0;
Status status = brin_index->lookupRevmap(5000, &page_num, &offset, &ctx);
if (status == Status::OK && page_num != 0) {
    // Range found at page_num, offset
    LOG_DEBUG(GENERAL, "Block 5000 in range at page %lu, offset %u",
             page_num, offset);
}
```

---

## 13. Conclusion

This specification provides complete implementation details for the 4 missing critical features in the BRIN index.

**Key Takeaways**:
- **Vacuum** is most critical (30-40 hours) - prevents unbounded growth
- **Multi-Page** is most critical (20-30 hours) - enables large tables
- **Revmap** is most impactful (20-30 hours) - enables O(1) lookups
- **Statistics** is most useful (5-10 hours) - enables query planner optimization

**Completion Criteria**:
- All 32 unit tests pass
- All integration tests pass
- MGA visibility rules respected throughout
- Performance meets requirements (100K+ inserts/sec, <10ms scans)

**Next Steps**:
1. Implement Feature 1 (Vacuum) first - CRITICAL for production
2. Implement Feature 2 (Multi-Page) second - CRITICAL for large tables
3. Implement Feature 3 (Revmap) third - IMPORTANT for performance
4. Implement Feature 4 (Statistics) fourth - NICE TO HAVE
5. Integration testing and performance benchmarking

**Status**: SPECIFICATION COMPLETE ✅
**Implementation**: PENDING (60-100 hours)

---

**Document Version**: 1.0
**Created**: November 4, 2025
**Author**: Claude (AI Assistant)
**Reference**: Index Implementation Audit 2025-11-04
**MGA Compliance**: Verified against /MGA_RULES.md
