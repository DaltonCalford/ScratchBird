# GiST Index - Completion Specification

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


> **✅ IMPLEMENTATION COMPLETE - November 22, 2025**
>
> This specification described work that has been **COMPLETED**.
> The GiST index is now fully implemented with all features described below:
> - ✅ remove() method with recursive tree traversal
> - ✅ splitPage() with operator class picksplit integration
> - ✅ removeDeadEntries() garbage collection
> - ✅ Full MGA compliance with xmin/xmax visibility
> - ✅ Unified executor interface (static factory method)
>
> **Implementation**: `src/core/gist_index.cpp` (1,351 lines)
> **Status**: Active and production-ready

---

**Project**: ScratchBird Database Engine
**Component**: GiST (Generalized Search Tree) Index - Complete Remaining Features
**Original Status**: 70% Complete (Core functional, missing critical features)
**Original Estimated Effort**: 40-60 hours
**Priority**: CRITICAL (Delete operations incomplete - production blocker)

---

## ⚠️ CRITICAL: MGA COMPLIANCE REQUIRED

**BEFORE ANY WORK**: Read `/MGA_RULES.md` - Firebird MGA rules are ABSOLUTE.

**Key MGA Rules**:
- Use `TransactionId` (uint64_t), NOT `Snapshot` or `SnapshotData`
- All GiST operations must respect xmin/xmax visibility
- TIP-based visibility via `TransactionManager::isVersionVisible()`
- NO PostgreSQL MVCC contamination
- Version traversal follows N2O (Newest-to-Oldest) chains

**If context is lost during compaction, re-read `/MGA_RULES.md` immediately.**

---

## Table of Contents

1. [Current Status](#1-current-status)
2. [Missing Feature 1: remove() Method](#2-missing-feature-1-remove-method)
3. [Missing Feature 2: picksplit() Implementation](#3-missing-feature-2-picksplit-implementation)
4. [Missing Feature 3: Garbage Collection](#4-missing-feature-3-garbage-collection)
5. [Testing Requirements](#5-testing-requirements)
6. [Implementation Breakdown](#6-implementation-breakdown)

---

## 1. Current Status

### What Works (70% Complete)

**File**: `src/core/gist_index.cpp` (633 lines)

**Implemented Features**:
- ✅ GiST framework with operator class interface
- ✅ Insert with recursive descent
- ✅ Search with subtree pruning (consistent() method)
- ✅ k-NN search with priority queue
- ✅ box_ops operator class (geometric boxes)
- ✅ MGA compliance (xmin/xmax visibility via `isEntryVisible()`)
- ✅ Root page allocation and initialization
- ✅ chooseSubtree() with penalty calculation
- ✅ Operator class registry
- ✅ Thread-safe concurrent access (std::shared_mutex)

### What's Missing (30% = 40-60 hours)

**Missing Feature 1**: remove() method (Line 323-336)
- **Current**: Only logical delete (sets xmax, increments counter)
- **Required**: Actually traverse tree and remove entry
- **Impact**: Cannot physically delete entries, causes unbounded growth
- **Effort**: 15-20 hours

**Missing Feature 2**: picksplit() implementation (Lines 448-466)
- **Current**: Stub that just allocates right page, doesn't distribute entries
- **Required**: Quadratic split algorithm with penalty calculation
- **Impact**: Page splits don't work correctly, tree becomes unbalanced
- **Effort**: 15-20 hours

**Missing Feature 3**: Garbage collection (Line 508-517)
- **Current**: Stub that just logs deleted count
- **Required**: Physical removal of dead entries (xmax < oldest_active_xid)
- **Impact**: Dead entries accumulate, wasting space
- **Effort**: 10-15 hours

---

## 2. Missing Feature 1: remove() Method

### 2.1 Problem Statement

**Current Code** (Lines 323-336):
```cpp
Status GiSTIndex::remove(const GiSTPredicate& predicate,
                        const TID& tid,
                        uint64_t current_xid,
                        ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    // Logical deletion: find entry and set xmax
    // TODO: Implement entry lookup and deletion
    // For now, just increment deleted count

    deleted_count_++;
    return Status::OK;
}
```

**Impact**:
- Entries are never actually found and marked deleted
- No tree traversal to locate matching entry
- xmax is never set, so entries remain visible forever
- deleted_count_ increments without doing anything useful

### 2.2 Solution: Full Tree Traversal for Entry Removal

**Architecture**:
```
Delete Request: predicate=Box(10,10,20,20), tid=(page=5, line=3)

1. Start at root
2. Use consistent() to identify which subtrees might contain the entry
3. Recursively descend into matching subtrees
4. At leaf level, scan for exact match (predicate + TID)
5. Set entry->entry_xmax = current_xid
6. Mark page as having garbage (GiSTFlags::HAS_GARBAGE)
7. Increment deleted_count_
```

**Why This Is Complex**:
- GiST indexes don't have unique keys (multiple entries can have same predicate)
- Must search by both predicate AND TID to find exact entry
- Need to handle case where entry is not found (already deleted, or never existed)
- Must maintain tree structure during logical deletion

### 2.3 Implementation Details

**Step 1: Add removeRecursive() Helper** (5-7 hours)

```cpp
// Add to gist_index.h private section:
Status removeRecursive(uint64_t page_num,
                      const GiSTPredicate& predicate,
                      const TID& tid,
                      uint64_t current_xid,
                      bool* found,
                      ErrorContext* ctx);
```

**Step 2: Implement remove() with Tree Traversal** (10-13 hours)

```cpp
// src/core/gist_index.cpp

Status GiSTIndex::remove(const GiSTPredicate& predicate,
                        const TID& tid,
                        uint64_t current_xid,
                        ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    if (root_page_ == 0)
    {
        // Empty index
        if (ctx)
        {
            ctx->code = Status::NOT_FOUND;
            ctx->message = "GiST index is empty";
        }
        return Status::NOT_FOUND;
    }

    bool found = false;
    Status status = removeRecursive(root_page_, predicate, tid,
                                    current_xid, &found, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (!found)
    {
        // Entry not found - this is not an error (idempotent delete)
        LOG_DEBUG(INDEX, "GiST entry not found for deletion: TID %s",
                 tid.toString().c_str());
        return Status::OK;
    }

    deleted_count_++;
    LOG_DEBUG(INDEX, "GiST entry deleted: TID %s, xid %lu",
             tid.toString().c_str(), current_xid);

    return Status::OK;
}

Status GiSTIndex::removeRecursive(uint64_t page_num,
                                  const GiSTPredicate& predicate,
                                  const TID& tid,
                                  uint64_t current_xid,
                                  bool* found,
                                  ErrorContext* ctx)
{
    *found = false;

    SBGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    bool is_leaf = (page->gist_flags & static_cast<uint16_t>(GiSTFlags::LEAF)) != 0;

    if (is_leaf)
    {
        // Scan leaf for matching entry
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
        bool page_modified = false;

        for (uint16_t i = 0; i < page->gist_count; ++i)
        {
            SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);

            // Skip already deleted entries
            if (entry->entry_xmax != 0)
            {
                entry_ptr += entry->entry_size;
                continue;
            }

            // Skip entries not visible to current transaction
            if (!isEntryVisible(entry->entry_xmin, entry->entry_xmax, current_xid))
            {
                entry_ptr += entry->entry_size;
                continue;
            }

            // Check if TID matches
            if (entry->entry_row_id != tid)
            {
                entry_ptr += entry->entry_size;
                continue;
            }

            // Extract predicate and compare
            GiSTPredicate entry_pred;
            entry_pred.opclass_id = page->gist_opclass_id;
            entry_pred.data.resize(entry->entry_pred_size);
            std::memcpy(entry_pred.data.data(), entry_ptr + sizeof(SBGiSTEntry),
                       entry->entry_pred_size);

            // Use operator class to check if predicates are equal
            if (opclass_->same(entry_pred, predicate))
            {
                // Found the entry - perform logical deletion
                entry->entry_xmax = current_xid;
                page->gist_flags |= static_cast<uint16_t>(GiSTFlags::HAS_GARBAGE);
                page->gist_deleted_entries++;
                page_modified = true;
                *found = true;

                LOG_DEBUG(INDEX, "GiST entry marked deleted on page %lu: TID %s, xmax=%lu",
                         page_num, tid.toString().c_str(), current_xid);

                // Mark page as dirty
                buffer_pool_->markPageDirty(page_num, ctx);

                return Status::OK;
            }

            entry_ptr += entry->entry_size;
        }

        // Not found on this leaf
        return Status::OK;
    }
    else
    {
        // Internal node - check which subtrees might contain the entry
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);

        for (uint16_t i = 0; i < page->gist_count; ++i)
        {
            SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);

            // Skip deleted entries
            if (!isEntryVisible(entry->entry_xmin, entry->entry_xmax, current_xid))
            {
                entry_ptr += entry->entry_size;
                continue;
            }

            // Extract predicate
            GiSTPredicate entry_pred;
            entry_pred.opclass_id = page->gist_opclass_id;
            entry_pred.data.resize(entry->entry_pred_size);
            std::memcpy(entry_pred.data.data(), entry_ptr + sizeof(SBGiSTEntry),
                       entry->entry_pred_size);

            // Use consistent() to check if this subtree might contain the entry
            // We use a query that represents the predicate we're looking for
            if (opclass_->consistent(entry_pred, predicate.data, GiSTStrategy::OVERLAPS))
            {
                // Recurse into this child
                uint64_t child_page = entry->entry_child_page;
                status = removeRecursive(child_page, predicate, tid, current_xid,
                                        found, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                if (*found)
                {
                    // Found and deleted in child subtree
                    return Status::OK;
                }
                // Continue searching other subtrees
            }

            entry_ptr += entry->entry_size;
        }

        // Not found in any subtree
        return Status::OK;
    }
}
```

### 2.4 Testing Requirements

**Unit Tests** (`tests/unit/test_gist_remove.cpp`):
1. [ ] Insert 100 entries, delete all, verify all have xmax set
2. [ ] Delete non-existent entry (should succeed with NOT_FOUND)
3. [ ] Delete already deleted entry (idempotent)
4. [ ] Delete entry from multi-level tree (depth 3+)
5. [ ] Verify deleted entries are not returned by search()
6. [ ] Verify deleted entries are still returned to old transactions (MGA)
7. [ ] Concurrent insert/delete on same tree
8. [ ] Delete entry with duplicate predicate (same predicate, different TID)

**Acceptance Criteria**:
- All 8 tests pass
- Deleted entries have xmax set correctly
- Search operations do not return deleted entries (for newer transactions)
- MGA visibility rules respected (old transactions see pre-delete state)

---

## 3. Missing Feature 2: picksplit() Implementation

### 3.1 Problem Statement

**Current Code** (Lines 418-466):
```cpp
Status GiSTIndex::splitPage(uint64_t page_num,
                            GiSTPredicate* left_pred,
                            GiSTPredicate* right_pred,
                            uint64_t* new_right_page,
                            ErrorContext* ctx)
{
    // ... collect entries ...

    // Use operator class to pick split
    std::vector<size_t> left_indices, right_indices;
    opclass_->picksplit(entries, left_indices, right_indices);

    // Allocate new page for right entries
    status = allocatePage(new_right_page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // TODO: Distribute entries to left and right pages
    // TODO: Compute union predicates for both pages

    LOG_DEBUG(INDEX, "GiST page %lu split into %lu and %lu",
             page_num, page_num, *new_right_page);

    return Status::OK;
}
```

**Impact**:
- Page splits don't actually distribute entries
- Left page keeps all entries (causing infinite loop on insert)
- Right page is allocated but empty (wasted space)
- Union predicates not computed (parent pointers broken)

### 3.2 Solution: Quadratic Split Algorithm

**Architecture**:
```
Page Overflow (101 entries, max=100):

1. Collect all entries from overflowing page
2. Call operator class picksplit() to divide entries
3. picksplit uses quadratic algorithm:
   a. Find "seed" entries (most distant pair)
   b. Assign remaining entries to left/right based on penalty
4. Write left entries back to original page
5. Write right entries to new page
6. Compute union predicate for each page
7. Return union predicates for parent to insert
```

**Quadratic Split Algorithm** (O(n²) time):
```
1. Find seed pair:
   - For each pair (i,j): calculate waste = area(union(i,j)) - area(i) - area(j)
   - Pick pair with maximum waste (most distant)

2. Assign remaining entries:
   - For each remaining entry:
     - Calculate penalty for adding to left: penalty_left = penalty(left_union, entry)
     - Calculate penalty for adding to right: penalty_right = penalty(right_union, entry)
     - Assign to side with lower penalty
     - Update union predicate for that side
```

### 3.3 Implementation Details

**Step 1: Complete splitPage() Entry Distribution** (8-10 hours)

```cpp
// src/core/gist_index.cpp

Status GiSTIndex::splitPage(uint64_t page_num,
                            GiSTPredicate* left_pred,
                            GiSTPredicate* right_pred,
                            uint64_t* new_right_page,
                            ErrorContext* ctx)
{
    SBGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    bool is_leaf = (page->gist_flags & static_cast<uint16_t>(GiSTFlags::LEAF)) != 0;
    uint16_t level = page->gist_level;

    // 1. Collect all entries from page
    std::vector<GiSTPredicate> predicates;
    std::vector<TID> tids;           // For leaf entries
    std::vector<uint64_t> child_pages; // For internal entries
    std::vector<uint64_t> xmins;
    std::vector<uint64_t> xmaxs;

    uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
    for (uint16_t i = 0; i < page->gist_count; ++i)
    {
        SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);

        GiSTPredicate pred;
        pred.opclass_id = page->gist_opclass_id;
        pred.data.resize(entry->entry_pred_size);
        std::memcpy(pred.data.data(), entry_ptr + sizeof(SBGiSTEntry),
                   entry->entry_pred_size);
        predicates.push_back(pred);

        if (is_leaf)
        {
            tids.push_back(entry->entry_row_id);
        }
        else
        {
            child_pages.push_back(entry->entry_child_page);
        }

        xmins.push_back(entry->entry_xmin);
        xmaxs.push_back(entry->entry_xmax);

        entry_ptr += entry->entry_size;
    }

    // 2. Use operator class to pick split
    std::vector<size_t> left_indices, right_indices;
    opclass_->picksplit(predicates, left_indices, right_indices);

    if (left_indices.empty() || right_indices.empty())
    {
        if (ctx)
        {
            ctx->code = Status::INTERNAL_ERROR;
            ctx->message = "GiST picksplit produced empty partition";
        }
        return Status::INTERNAL_ERROR;
    }

    // 3. Allocate new right page
    status = allocatePage(new_right_page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    SBGiSTPage* right_page = nullptr;
    status = loadPage(*new_right_page, &right_page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // 4. Initialize right page
    std::memset(right_page, 0, sizeof(SBGiSTPage));
    initPageHeader(&right_page->gist_header, PageType::GIST_INDEX_PAGE);
    right_page->gist_index_uuid = page->gist_index_uuid;
    right_page->gist_table_uuid = page->gist_table_uuid;
    right_page->gist_flags = page->gist_flags & ~static_cast<uint16_t>(GiSTFlags::ROOT);
    right_page->gist_level = level;
    right_page->gist_opclass_id = page->gist_opclass_id;
    right_page->gist_xmin = txn_manager_->getCurrentXid();
    right_page->gist_xmax = 0;
    right_page->gist_count = 0;
    right_page->gist_free_space = 8192 - sizeof(SBGiSTPage);

    // Set up sibling pointers
    right_page->gist_left_sibling = page_num;
    right_page->gist_right_sibling = page->gist_right_sibling;
    page->gist_right_sibling = *new_right_page;

    // 5. Redistribute entries
    // Clear original page's entries (keep header)
    page->gist_count = 0;
    page->gist_free_space = 8192 - sizeof(SBGiSTPage);

    // Write left entries to original page
    status = writeEntriesToPage(page, left_indices, predicates,
                                is_leaf ? &tids : &child_pages,
                                xmins, xmaxs, is_leaf, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Write right entries to new page
    status = writeEntriesToPage(right_page, right_indices, predicates,
                                is_leaf ? &tids : &child_pages,
                                xmins, xmaxs, is_leaf, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // 6. Compute union predicates for both pages
    std::vector<GiSTPredicate> left_preds, right_preds;
    for (size_t idx : left_indices)
    {
        left_preds.push_back(predicates[idx]);
    }
    for (size_t idx : right_indices)
    {
        right_preds.push_back(predicates[idx]);
    }

    *left_pred = opclass_->unionPredicates(left_preds);
    *right_pred = opclass_->unionPredicates(right_preds);

    // 7. Mark pages as dirty
    buffer_pool_->markPageDirty(page_num, ctx);
    buffer_pool_->markPageDirty(*new_right_page, ctx);

    LOG_DEBUG(INDEX, "GiST page %lu split: %zu entries left, %zu entries right",
             page_num, left_indices.size(), right_indices.size());

    return Status::OK;
}
```

**Step 2: Add writeEntriesToPage() Helper** (3-4 hours)

```cpp
// Add to gist_index.h private section:
Status writeEntriesToPage(SBGiSTPage* page,
                         const std::vector<size_t>& indices,
                         const std::vector<GiSTPredicate>& predicates,
                         const void* tid_or_child_data,
                         const std::vector<uint64_t>& xmins,
                         const std::vector<uint64_t>& xmaxs,
                         bool is_leaf,
                         ErrorContext* ctx);

// Implementation in gist_index.cpp:
Status GiSTIndex::writeEntriesToPage(SBGiSTPage* page,
                                     const std::vector<size_t>& indices,
                                     const std::vector<GiSTPredicate>& predicates,
                                     const void* tid_or_child_data,
                                     const std::vector<uint64_t>& xmins,
                                     const std::vector<uint64_t>& xmaxs,
                                     bool is_leaf,
                                     ErrorContext* ctx)
{
    const std::vector<TID>* tids = is_leaf ?
        static_cast<const std::vector<TID>*>(tid_or_child_data) : nullptr;
    const std::vector<uint64_t>* child_pages = !is_leaf ?
        static_cast<const std::vector<uint64_t>*>(tid_or_child_data) : nullptr;

    uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);

    for (size_t idx : indices)
    {
        const GiSTPredicate& pred = predicates[idx];
        uint16_t entry_size = sizeof(SBGiSTEntry) + pred.data.size();

        if (page->gist_free_space < entry_size)
        {
            if (ctx)
            {
                ctx->code = Status::INTERNAL_ERROR;
                ctx->message = "Page overflow during split distribution";
            }
            return Status::INTERNAL_ERROR;
        }

        // Create entry
        SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);
        entry->entry_size = entry_size;
        entry->entry_flags = 0;
        entry->entry_pred_size = pred.data.size();
        entry->entry_reserved = 0;

        if (is_leaf)
        {
            entry->entry_row_id = (*tids)[idx];
        }
        else
        {
            entry->entry_child_page = (*child_pages)[idx];
        }

        entry->entry_xmin = xmins[idx];
        entry->entry_xmax = xmaxs[idx];

        // Copy predicate data
        std::memcpy(entry_ptr + sizeof(SBGiSTEntry), pred.data.data(),
                   pred.data.size());

        entry_ptr += entry_size;
        page->gist_count++;
        page->gist_free_space -= entry_size;
    }

    return Status::OK;
}
```

**Step 3: Implement Quadratic picksplit in box_ops** (4-6 hours)

```cpp
// This would be in the box_ops operator class implementation
// (assumed to be in gist_box_ops.cpp or similar)

void BoxGiSTOperatorClass::picksplit(const std::vector<GiSTPredicate>& entries,
                                     std::vector<size_t>& left_indices,
                                     std::vector<size_t>& right_indices) const
{
    left_indices.clear();
    right_indices.clear();

    if (entries.size() < 2)
    {
        // Degenerate case
        left_indices.push_back(0);
        return;
    }

    // 1. Find seed pair (quadratic algorithm)
    size_t seed1 = 0, seed2 = 1;
    double max_waste = -1.0;

    for (size_t i = 0; i < entries.size(); ++i)
    {
        for (size_t j = i + 1; j < entries.size(); ++j)
        {
            // Calculate waste = area(union(i,j)) - area(i) - area(j)
            std::vector<GiSTPredicate> pair = {entries[i], entries[j]};
            GiSTPredicate union_pred = unionPredicates(pair);

            double area_union = calculateArea(union_pred);
            double area_i = calculateArea(entries[i]);
            double area_j = calculateArea(entries[j]);
            double waste = area_union - area_i - area_j;

            if (waste > max_waste)
            {
                max_waste = waste;
                seed1 = i;
                seed2 = j;
            }
        }
    }

    // 2. Initialize partitions with seeds
    left_indices.push_back(seed1);
    right_indices.push_back(seed2);

    GiSTPredicate left_union = entries[seed1];
    GiSTPredicate right_union = entries[seed2];

    // 3. Assign remaining entries
    std::vector<bool> assigned(entries.size(), false);
    assigned[seed1] = true;
    assigned[seed2] = true;

    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (assigned[i]) continue;

        // Calculate penalty for each side
        double penalty_left = penalty(left_union, entries[i]);
        double penalty_right = penalty(right_union, entries[i]);

        if (penalty_left < penalty_right)
        {
            left_indices.push_back(i);
            std::vector<GiSTPredicate> temp = {left_union, entries[i]};
            left_union = unionPredicates(temp);
        }
        else
        {
            right_indices.push_back(i);
            std::vector<GiSTPredicate> temp = {right_union, entries[i]};
            right_union = unionPredicates(temp);
        }

        assigned[i] = true;
    }
}
```

### 3.4 Testing Requirements

**Unit Tests** (`tests/unit/test_gist_split.cpp`):
1. [ ] Insert 150 entries into GiST index, forcing multiple splits
2. [ ] Verify tree height increases correctly after splits
3. [ ] Verify all entries are present after split (no data loss)
4. [ ] Verify left/right pages have reasonable balance (~50/50)
5. [ ] Verify union predicates correctly cover all child entries
6. [ ] Verify sibling pointers are correct after split
7. [ ] Search after split returns all matching entries
8. [ ] Concurrent inserts during split (stress test)

**Acceptance Criteria**:
- All 8 tests pass
- Page splits distribute entries correctly
- Tree remains balanced (height = O(log n))
- No entries lost during split
- Search operations work correctly after split

---

## 4. Missing Feature 3: Garbage Collection

### 4.1 Problem Statement

**Current Code** (Lines 508-517):
```cpp
Status GiSTIndex::removeDeadEntries(uint64_t oldest_active_xid, ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    // TODO: Traverse tree and physically remove entries where xmax < oldest_active_xid
    LOG_INFO(INDEX, "GiST garbage collection: %lu dead entries to remove",
             deleted_count_);

    return Status::OK;
}
```

**Impact**:
- Dead entries (xmax < oldest_active_xid) are never physically removed
- Pages remain bloated with deleted entries
- Free space not reclaimed
- Performance degrades over time as tree traversal must skip dead entries

### 4.2 Solution: Recursive Tree Garbage Collection

**Architecture**:
```
Garbage Collection Process:

1. Start at root page
2. For each page:
   a. Scan entries looking for xmax < oldest_active_xid
   b. Physically remove dead entries (compact page)
   c. Recalculate free space
   d. If internal node, recurse into children
3. If page becomes too empty (<30% full), consider merge with sibling
4. Clear HAS_GARBAGE flag if all garbage removed
5. Update statistics (deleted_count_, total_entries)
```

**Considerations**:
- Must not remove entries visible to active transactions
- Must maintain tree structure (don't break parent pointers)
- Should be interruptible (long-running operation)
- Should handle concurrent reads (use shared_lock where possible)

### 4.3 Implementation Details

**Step 1: Implement removeDeadEntries()** (8-12 hours)

```cpp
// src/core/gist_index.cpp

Status GiSTIndex::removeDeadEntries(uint64_t oldest_active_xid, ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    if (root_page_ == 0)
    {
        // Empty index
        return Status::OK;
    }

    if (deleted_count_ == 0)
    {
        // Nothing to collect
        LOG_DEBUG(INDEX, "GiST garbage collection: no dead entries");
        return Status::OK;
    }

    LOG_INFO(INDEX, "GiST garbage collection starting: %lu dead entries, OAT=%lu",
             deleted_count_, oldest_active_xid);

    uint64_t removed_count = 0;
    Status status = gcRecursive(root_page_, oldest_active_xid, &removed_count, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Update statistics
    deleted_count_ -= removed_count;
    entry_count_ -= removed_count;

    LOG_INFO(INDEX, "GiST garbage collection complete: %lu entries removed",
             removed_count);

    return Status::OK;
}

Status GiSTIndex::gcRecursive(uint64_t page_num,
                              uint64_t oldest_active_xid,
                              uint64_t* removed_count,
                              ErrorContext* ctx)
{
    SBGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    bool is_leaf = (page->gist_flags & static_cast<uint16_t>(GiSTFlags::LEAF)) != 0;
    bool has_garbage = (page->gist_flags & static_cast<uint16_t>(GiSTFlags::HAS_GARBAGE)) != 0;

    if (!has_garbage && is_leaf)
    {
        // No garbage on this leaf, skip
        return Status::OK;
    }

    // 1. If internal node, recurse into children first
    if (!is_leaf)
    {
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
        for (uint16_t i = 0; i < page->gist_count; ++i)
        {
            SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);

            if (entry->entry_xmax == 0 || entry->entry_xmax >= oldest_active_xid)
            {
                // Entry is still alive, recurse into child
                uint64_t child_page = entry->entry_child_page;
                status = gcRecursive(child_page, oldest_active_xid, removed_count, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }

            entry_ptr += entry->entry_size;
        }
    }

    // 2. Scan this page for dead entries
    std::vector<size_t> live_entry_offsets;
    uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
    size_t page_data_start = sizeof(SBGiSTPage);

    for (uint16_t i = 0; i < page->gist_count; ++i)
    {
        SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);
        size_t entry_offset = entry_ptr - reinterpret_cast<uint8_t*>(page);

        bool is_dead = (entry->entry_xmax != 0) && (entry->entry_xmax < oldest_active_xid);

        if (is_dead)
        {
            // This entry is garbage, don't include in live list
            (*removed_count)++;
            LOG_DEBUG(INDEX, "GC: Removing dead entry on page %lu: xmin=%lu, xmax=%lu",
                     page_num, entry->entry_xmin, entry->entry_xmax);
        }
        else
        {
            // This entry is still alive
            live_entry_offsets.push_back(entry_offset);
        }

        entry_ptr += entry->entry_size;
    }

    // 3. If we removed any entries, compact the page
    if (live_entry_offsets.size() < page->gist_count)
    {
        status = compactPageEntries(page, live_entry_offsets, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Clear HAS_GARBAGE flag if page is now clean
        if (page->gist_count == live_entry_offsets.size())
        {
            page->gist_flags &= ~static_cast<uint16_t>(GiSTFlags::HAS_GARBAGE);
        }

        page->gist_deleted_entries = 0;

        // Mark page as dirty
        buffer_pool_->markPageDirty(page_num, ctx);

        LOG_DEBUG(INDEX, "GC: Page %lu compacted: %lu live entries (was %u)",
                 page_num, live_entry_offsets.size(), page->gist_count);
    }

    return Status::OK;
}
```

**Step 2: Add compactPageEntries() Helper** (2-3 hours)

```cpp
// Add to gist_index.h private section:
Status compactPageEntries(SBGiSTPage* page,
                         const std::vector<size_t>& live_entry_offsets,
                         ErrorContext* ctx);

// Implementation:
Status GiSTIndex::compactPageEntries(SBGiSTPage* page,
                                     const std::vector<size_t>& live_entry_offsets,
                                     ErrorContext* ctx)
{
    // Create temporary buffer to hold live entries
    std::vector<uint8_t> temp_buffer;
    temp_buffer.reserve(8192 - sizeof(SBGiSTPage));

    uint8_t* page_bytes = reinterpret_cast<uint8_t*>(page);

    // Copy live entries to temporary buffer
    for (size_t offset : live_entry_offsets)
    {
        SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(page_bytes + offset);
        uint16_t entry_size = entry->entry_size;

        // Copy entire entry (header + predicate data)
        temp_buffer.insert(temp_buffer.end(),
                          page_bytes + offset,
                          page_bytes + offset + entry_size);
    }

    // Clear entry area
    size_t entry_area_start = sizeof(SBGiSTPage);
    size_t entry_area_size = 8192 - sizeof(SBGiSTPage);
    std::memset(page_bytes + entry_area_start, 0, entry_area_size);

    // Copy compacted entries back
    std::memcpy(page_bytes + entry_area_start, temp_buffer.data(), temp_buffer.size());

    // Update page metadata
    page->gist_count = live_entry_offsets.size();
    page->gist_free_space = entry_area_size - temp_buffer.size();

    return Status::OK;
}
```

### 4.4 Testing Requirements

**Unit Tests** (`tests/unit/test_gist_gc.cpp`):
1. [ ] Insert 100 entries, delete 50, run GC, verify 50 removed
2. [ ] GC with no dead entries (should be no-op)
3. [ ] GC removes only entries with xmax < oldest_active_xid
4. [ ] GC preserves entries visible to active transactions
5. [ ] Verify page free_space increases after GC
6. [ ] Verify HAS_GARBAGE flag cleared after GC
7. [ ] Multi-level tree GC (recursion test)
8. [ ] Concurrent reads during GC (should see consistent state)

**Acceptance Criteria**:
- All 8 tests pass
- Dead entries physically removed from pages
- Free space correctly reclaimed
- Active transactions not affected by GC
- Statistics (deleted_count_, entry_count_) updated correctly

---

## 5. Testing Requirements

### 5.1 Unit Tests

**New Test Files**:
1. `tests/unit/test_gist_remove.cpp` (8 tests)
2. `tests/unit/test_gist_split.cpp` (8 tests)
3. `tests/unit/test_gist_gc.cpp` (8 tests)

**Total**: 24 new tests

### 5.2 Integration Tests

**Existing Integration Tests** (`tests/integration/test_gist_index.cpp`):
- [ ] Verify all existing tests still pass after changes
- [ ] Add stress test: 10,000 inserts, 5,000 deletes, GC, verify correctness
- [ ] Add concurrency test: 4 threads inserting, 2 threads deleting, 1 thread GC
- [ ] Add MGA test: delete entry, verify old transaction still sees it

### 5.3 Performance Benchmarks

**Benchmarks** (`tests/benchmark/benchmark_gist_index.cpp`):
- [ ] Measure insert throughput (entries/sec) before and after completion
- [ ] Measure delete throughput (entries/sec)
- [ ] Measure search latency with varying levels of dead entries
- [ ] Measure GC duration with 100K entries (50% dead)
- [ ] Compare GiST k-NN performance vs R-Tree

---

## 6. Implementation Breakdown

### 6.1 Task Breakdown

| Task | Effort (hours) | Dependency |
|------|----------------|------------|
| **Feature 1: remove() Method** | **15-20** | - |
| 1.1 Add removeRecursive() signature | 0.5 | - |
| 1.2 Implement remove() with tree traversal | 6-8 | 1.1 |
| 1.3 Implement removeRecursive() leaf search | 4-6 | 1.2 |
| 1.4 Implement removeRecursive() internal descent | 3-4 | 1.3 |
| 1.5 Unit tests for remove operations | 2-3 | 1.2, 1.3, 1.4 |
| **Feature 2: picksplit() Implementation** | **15-20** | - |
| 2.1 Complete splitPage() entry distribution | 8-10 | - |
| 2.2 Add writeEntriesToPage() helper | 3-4 | 2.1 |
| 2.3 Implement quadratic picksplit in box_ops | 4-6 | 2.1 |
| 2.4 Unit tests for page splits | 2-3 | 2.1, 2.2, 2.3 |
| **Feature 3: Garbage Collection** | **10-15** | - |
| 3.1 Implement removeDeadEntries() main logic | 4-6 | - |
| 3.2 Implement gcRecursive() traversal | 4-6 | 3.1 |
| 3.3 Add compactPageEntries() helper | 2-3 | 3.2 |
| 3.4 Unit tests for garbage collection | 2-3 | 3.1, 3.2, 3.3 |
| **Integration & Performance** | **3-5** | All |
| 4.1 Integration test updates | 2-3 | All |
| 4.2 Performance benchmarks | 1-2 | All |
| **TOTAL** | **43-60** | - |

### 6.2 Estimated Total Effort

**Realistic Estimate**: 40-60 hours (includes buffer time for debugging)

**Timeline**:
- Single developer (full-time): 1-1.5 weeks
- Part-time: 2-3 weeks

### 6.3 Critical Path

**Critical Path** (longest dependency chain):
1. Feature 2 (picksplit) must be completed for Feature 1 (remove) to work correctly on split pages
2. Feature 1 (remove) must be completed for Feature 3 (GC) to have entries to collect
3. Integration tests depend on all features

**Recommended Order**:
1. Feature 2 (picksplit) - Unblocks tree growth
2. Feature 1 (remove) - Unblocks deletion
3. Feature 3 (GC) - Cleanup and optimization
4. Integration tests and benchmarks

---

## 7. MGA Compliance Checklist

**All GiST operations must respect MGA rules:**

- [x] Current implementation uses `TransactionId` (uint64_t), not `Snapshot`
- [x] Visibility checks use `isEntryVisible()` with TIP lookups
- [x] Search operations pass `current_xid`, not `snapshot`
- [ ] remove() must set entry_xmax correctly (TO IMPLEMENT)
- [ ] removeDeadEntries() must respect oldest_active_xid (TO IMPLEMENT)
- [x] Index entries store stable TIDs (never change)
- [x] MGA version chains respected (N2O)

**New Code Requirements**:
- [ ] removeRecursive() must use `isEntryVisible(entry_xmin, entry_xmax, current_xid)`
- [ ] gcRecursive() must only remove entries where `xmax < oldest_active_xid`
- [ ] No use of `Snapshot` structures anywhere
- [ ] All transaction state checks via `TransactionManager` TIP lookups

**Reference**: See `/MGA_RULES.md` Section 4 (Visibility Rules)

---

## 8. Dependencies and Assumptions

### 8.1 External Dependencies

**Requires**:
- `TransactionManager::isVersionVisible()` - For MGA visibility checks
- `TransactionManager::getCurrentXid()` - For xmin/xmax assignment
- `BufferPool::pinPage()` / `unpinPage()` - For page access
- `BufferPool::markPageDirty()` - For write-ahead logging
- Operator class implementations (box_ops, etc.) - For picksplit(), penalty()

**Assumes**:
- Buffer pool is thread-safe
- Transaction manager TIP is correctly maintained
- Operator class methods are deterministic

### 8.2 Operator Class Requirements

**Each operator class must implement**:
- `consistent()` - Already required, assumed working
- `union()` - Already required, assumed working
- `penalty()` - Already required, assumed working
- **`picksplit()`** - Currently stub, MUST be implemented for Feature 2
- `same()` - Already required, assumed working
- `distance()` - Optional, for k-NN queries

**If picksplit() is not implemented in an operator class, that operator class cannot be used.**

---

## 9. Known Limitations and Future Work

### 9.1 Current Limitations (After Completion)

**Not Included in This Spec**:
- Page merging when pages become sparse (future optimization)
- R*-Tree style forced reinsert (future optimization)
- Compressed predicates (future optimization)
- Bulk loading (future optimization)
- Concurrent page splits (currently uses exclusive lock)

**These are OK for Phase 1, can be added later.**

### 9.2 Future Enhancements

**Phase 2 Enhancements** (not required now):
1. **Page Merging** (20-30 hours)
   - When page drops below 30% full, merge with sibling
   - Reduces tree height, improves cache locality

2. **R*-Tree Reinsert** (15-20 hours)
   - On overflow, reinsert 30% of entries before splitting
   - Improves index quality, reduces splits

3. **Predicate Compression** (10-15 hours)
   - Compress large predicates (e.g., polygons)
   - Saves space, reduces I/O

4. **Bulk Loading** (20-30 hours)
   - Build GiST index from sorted data
   - 10-100x faster than individual inserts

---

## 10. Acceptance Criteria

### 10.1 Functional Requirements

**Must Pass**:
- [ ] All 24 unit tests pass
- [ ] All integration tests pass
- [ ] Can insert 10,000 entries without error
- [ ] Can delete 5,000 entries (50%)
- [ ] Garbage collection removes all dead entries
- [ ] Search returns correct results after inserts/deletes
- [ ] k-NN queries return correct nearest neighbors
- [ ] Concurrent reads during writes work correctly

### 10.2 Performance Requirements

**Must Achieve**:
- [ ] Insert throughput: >10,000 entries/sec (single thread)
- [ ] Search latency: <1ms for 100K entry index (avg)
- [ ] Delete throughput: >5,000 entries/sec
- [ ] GC duration: <10 seconds for 100K entries (50% dead)
- [ ] Tree height: O(log n) with branching factor ~50

### 10.3 MGA Compliance Requirements

**Must Verify**:
- [ ] Deleted entries visible to old transactions (snapshot isolation)
- [ ] Deleted entries invisible to new transactions
- [ ] GC only removes entries invisible to all active transactions
- [ ] No `Snapshot` structures used anywhere
- [ ] All visibility checks via `TransactionManager::isVersionVisible()`

---

## 11. Conclusion

This specification provides complete implementation details for the 3 missing critical features in the GiST index.

**Key Takeaways**:
- **remove()** is most critical (15-20 hours) - enables actual deletion
- **picksplit()** is most complex (15-20 hours) - enables tree growth
- **Garbage collection** is most impactful (10-15 hours) - enables space reclamation

**Completion Criteria**:
- All 24 unit tests pass
- All integration tests pass
- MGA visibility rules respected throughout
- Performance meets requirements

**Next Steps**:
1. Implement Feature 2 (picksplit) first - unblocks tree growth
2. Implement Feature 1 (remove) second - unblocks deletion
3. Implement Feature 3 (GC) third - completes the feature set
4. Integration testing and performance benchmarking

**Status**: SPECIFICATION COMPLETE ✅
**Implementation**: PENDING (40-60 hours)
