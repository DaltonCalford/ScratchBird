# SP-GiST Index - Completion Specification

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


> **✅ IMPLEMENTATION COMPLETE - November 22, 2025**
>
> This specification described work that has been **COMPLETED**.
> The SP-GiST index is now fully implemented with all features described below:
> - ✅ splitNode() for leaf overflow with partition allocation
> - ✅ remove() method with recursive INNER/LEAF handling
> - ✅ removeDeadEntries() TID-based garbage collection
> - ✅ Full MGA compliance with xmin/xmax visibility
> - ✅ Unified executor interface (static factory method)
>
> **Implementation**: `src/core/spgist_index.cpp` (1,387 lines)
> **Status**: Active and production-ready

---

**Project**: ScratchBird Database Engine
**Component**: SP-GiST (Space-Partitioned Generalized Search Tree) Index - Complete Remaining Features
**Original Status**: 75% Complete (Core functional, missing critical features)
**Original Estimated Effort**: 30-40 hours
**Priority**: CRITICAL (Delete operations incomplete - production blocker)

---

## CRITICAL: MGA COMPLIANCE REQUIRED

**BEFORE ANY WORK**: Read `/MGA_RULES.md` - Firebird MGA rules are ABSOLUTE.

**Key MGA Rules**:
- Use `TransactionId` (uint64_t), NOT `Snapshot` or `SnapshotData`
- All SP-GiST operations must respect xmin/xmax visibility
- TIP-based visibility via `TransactionManager::isVersionVisible()`
- NO PostgreSQL MVCC contamination
- Version traversal follows N2O (Newest-to-Oldest) chains

**If context is lost during compaction, re-read `/MGA_RULES.md` immediately.**

---

## Table of Contents

1. [Current Status](#1-current-status)
2. [Missing Feature 1: picksplit() for Leaf Overflow](#2-missing-feature-1-picksplit-for-leaf-overflow)
3. [Missing Feature 2: remove() Method](#3-missing-feature-2-remove-method)
4. [Missing Feature 3: Garbage Collection](#4-missing-feature-3-garbage-collection)
5. [Testing Requirements](#5-testing-requirements)
6. [Implementation Breakdown](#6-implementation-breakdown)

---

## 1. Current Status

### What Works (75% Complete)

**File**: `src/core/spgist_index.cpp` (526 lines)

**Implemented Features**:
- ✅ SP-GiST framework (inner/leaf distinction)
- ✅ Insert with recursive descent into existing nodes
- ✅ Search with partition pruning
- ✅ quad_ops operator class (quad-tree for 2D points)
- ✅ text_ops operator class (radix tree for prefix search)
- ✅ MGA compliance (xmin/xmax visibility via `isEntryVisible()`)
- ✅ Root page allocation and initialization
- ✅ Operator class interface (choose(), pickSplit(), innerConsistent(), leafConsistent())
- ✅ Thread-safe concurrent access (std::shared_mutex)

### What's Missing (25% = 30-40 hours)

**Missing Feature 1**: picksplit() for leaf overflow (Line 384)
- **Current**: Stub that collects values but doesn't allocate pages or distribute
- **Required**: Convert leaf to inner node, allocate child pages, distribute values
- **Impact**: Leaves can overflow causing insert failures
- **Effort**: 15-20 hours

**Missing Feature 2**: remove() method (Line 400)
- **Current**: Only increments deleted_count, no actual deletion
- **Required**: Traverse tree to find entry and set xmax
- **Impact**: Cannot delete entries, causes unbounded growth
- **Effort**: 10-15 hours

**Missing Feature 3**: Garbage collection (Line 410)
- **Current**: Stub that just logs deleted count
- **Required**: Physical removal of dead entries (xmax < oldest_active_xid)
- **Impact**: Dead entries accumulate, wasting space
- **Effort**: 5-10 hours

---

## 2. Missing Feature 1: picksplit() for Leaf Overflow

### 2.1 Problem Statement

**Current Code** (Lines 351-390):
```cpp
Status SPGiSTIndex::splitNode(uint64_t page_num, ErrorContext* ctx)
{
    SBSPGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Collect all leaf values
    std::vector<std::vector<uint8_t>> values;
    uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);

    for (uint16_t i = 0; i < page->spgist_count; ++i)
    {
        SBSPGiSTLeafTuple* leaf = reinterpret_cast<SBSPGiSTLeafTuple*>(entry_ptr);

        std::vector<uint8_t> value(leaf->leaf_valueSize);
        std::memcpy(value.data(), entry_ptr + sizeof(SBSPGiSTLeafTuple),
                   leaf->leaf_valueSize);
        values.push_back(value);

        entry_ptr += leaf->leaf_size;
    }

    // Ask operator class how to partition
    std::vector<uint8_t> prefix;
    std::vector<std::vector<uint8_t>> labels;
    std::vector<size_t> assignments;

    opclass_->pickSplit(values, prefix, labels, assignments);

    // Convert leaf to inner node
    // TODO: Allocate child pages and distribute values

    LOG_DEBUG(INDEX, "SP-GiST: Split leaf page %lu into %zu partitions",
             page_num, labels.size());

    return Status::OK;
}
```

**Impact**:
- Leaf pages cannot split when full (insert fails or loops)
- Tree cannot grow beyond root leaf
- No space partitioning actually happens
- pickSplit() results are computed but not used

### 2.2 Solution: Complete Leaf-to-Inner Conversion

**Architecture**:
```
Leaf Overflow (51 values, max=50):

1. Collect all leaf entries (values + TIDs)
2. Call operator class pickSplit() to partition values
3. pickSplit returns:
   - prefix: Common prefix data (e.g., for radix tree)
   - labels: Partition identifiers (e.g., NW, NE, SW, SE for quad-tree)
   - assignments: Which partition each value belongs to
4. Allocate one leaf page per partition
5. Distribute values to new leaf pages based on assignments
6. Convert original page from LEAF to INNER
7. Write inner tuple with prefix, labels, and child page pointers
```

**Example: Quad-Tree Split**:
```
Before (Leaf with 51 points):
Page 100 [LEAF]:
  (1,1) → TID:1
  (2,2) → TID:2
  ...
  (9,9) → TID:51

After (Inner with 4 leaf children):
Page 100 [INNER]:
  Prefix: (5,5)  // Center point
  Node 0: Label=NW (x<5, y>=5) → Page 101
  Node 1: Label=NE (x>=5, y>=5) → Page 102
  Node 2: Label=SW (x<5, y<5) → Page 103
  Node 3: Label=SE (x>=5, y<5) → Page 104

Page 101 [LEAF]: Points in NW quadrant
Page 102 [LEAF]: Points in NE quadrant
Page 103 [LEAF]: Points in SW quadrant
Page 104 [LEAF]: Points in SE quadrant
```

### 2.3 Implementation Details

**Step 1: Complete splitNode() Implementation** (12-15 hours)

```cpp
// src/core/spgist_index.cpp

Status SPGiSTIndex::splitNode(uint64_t page_num, ErrorContext* ctx)
{
    SBSPGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Verify this is a leaf page
    if (static_cast<SPGiSTNodeType>(page->spgist_node_type) != SPGiSTNodeType::LEAF)
    {
        if (ctx)
        {
            ctx->code = Status::INTERNAL_ERROR;
            ctx->message = "SP-GiST splitNode called on non-leaf page";
        }
        return Status::INTERNAL_ERROR;
    }

    // 1. Collect all leaf entries
    std::vector<std::vector<uint8_t>> values;
    std::vector<TID> tids;
    std::vector<uint64_t> xmins;
    std::vector<uint64_t> xmaxs;

    uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);

    for (uint16_t i = 0; i < page->spgist_count; ++i)
    {
        SBSPGiSTLeafTuple* leaf = reinterpret_cast<SBSPGiSTLeafTuple*>(entry_ptr);

        // Extract value
        std::vector<uint8_t> value(leaf->leaf_valueSize);
        std::memcpy(value.data(), entry_ptr + sizeof(SBSPGiSTLeafTuple),
                   leaf->leaf_valueSize);
        values.push_back(value);

        // Store TID and MGA metadata
        tids.push_back(leaf->leaf_tid);
        xmins.push_back(leaf->leaf_xmin);
        xmaxs.push_back(leaf->leaf_xmax);

        entry_ptr += leaf->leaf_size;
    }

    if (values.empty())
    {
        if (ctx)
        {
            ctx->code = Status::INTERNAL_ERROR;
            ctx->message = "SP-GiST splitNode called on empty page";
        }
        return Status::INTERNAL_ERROR;
    }

    // 2. Ask operator class how to partition
    std::vector<uint8_t> prefix;
    std::vector<std::vector<uint8_t>> labels;
    std::vector<size_t> assignments;

    opclass_->pickSplit(values, prefix, labels, assignments);

    if (labels.empty())
    {
        if (ctx)
        {
            ctx->code = Status::INTERNAL_ERROR;
            ctx->message = "SP-GiST pickSplit returned zero partitions";
        }
        return Status::INTERNAL_ERROR;
    }

    // Verify assignments are valid
    for (size_t assignment : assignments)
    {
        if (assignment >= labels.size())
        {
            if (ctx)
            {
                ctx->code = Status::INTERNAL_ERROR;
                ctx->message = "SP-GiST pickSplit returned invalid assignment";
            }
            return Status::INTERNAL_ERROR;
        }
    }

    LOG_DEBUG(INDEX, "SP-GiST: Splitting leaf page %lu into %zu partitions",
             page_num, labels.size());

    // 3. Allocate child pages (one per partition)
    std::vector<uint64_t> child_pages(labels.size());
    for (size_t i = 0; i < labels.size(); ++i)
    {
        status = allocatePage(&child_pages[i], ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Initialize child as leaf page
        SBSPGiSTPage* child = nullptr;
        status = loadPage(child_pages[i], &child, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::memset(child, 0, sizeof(SBSPGiSTPage));
        initPageHeader(&child->spgist_header, PageType::SPGIST_INDEX_PAGE);
        child->spgist_index_uuid = page->spgist_index_uuid;
        child->spgist_table_uuid = page->spgist_table_uuid;
        child->spgist_flags = 0;
        child->spgist_node_type = static_cast<uint16_t>(SPGiSTNodeType::LEAF);
        child->spgist_count = 0;
        child->spgist_free_space = 8192 - sizeof(SBSPGiSTPage);
        child->spgist_opclass_id = page->spgist_opclass_id;
        child->spgist_parent_page = page_num;
        child->spgist_xmin = txn_manager_->getCurrentXid();
        child->spgist_xmax = 0;
        child->spgist_total_entries = 0;
        child->spgist_deleted_entries = 0;
    }

    // 4. Distribute values to child pages
    for (size_t i = 0; i < values.size(); ++i)
    {
        size_t partition = assignments[i];
        uint64_t child_page_num = child_pages[partition];

        SBSPGiSTPage* child = nullptr;
        status = loadPage(child_page_num, &child, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Create leaf entry in child page
        uint16_t entry_size = sizeof(SBSPGiSTLeafTuple) + values[i].size();

        if (child->spgist_free_space < entry_size)
        {
            // Child partition is full - this shouldn't happen with good pickSplit
            if (ctx)
            {
                ctx->code = Status::INTERNAL_ERROR;
                ctx->message = "SP-GiST child partition overflow during split";
            }
            return Status::INTERNAL_ERROR;
        }

        uint8_t* child_entry_ptr = reinterpret_cast<uint8_t*>(child) +
                                   sizeof(SBSPGiSTPage) +
                                   (8192 - sizeof(SBSPGiSTPage) - child->spgist_free_space);

        SBSPGiSTLeafTuple* leaf = reinterpret_cast<SBSPGiSTLeafTuple*>(child_entry_ptr);
        leaf->leaf_size = entry_size;
        leaf->leaf_valueSize = values[i].size();
        leaf->leaf_reserved = 0;
        leaf->leaf_tid = tids[i];
        leaf->leaf_xmin = xmins[i];
        leaf->leaf_xmax = xmaxs[i];

        // Copy value data
        std::memcpy(child_entry_ptr + sizeof(SBSPGiSTLeafTuple),
                   values[i].data(), values[i].size());

        child->spgist_count++;
        child->spgist_free_space -= entry_size;
        child->spgist_total_entries++;

        // Mark child page as dirty
        buffer_pool_->markPageDirty(child_page_num, ctx);
    }

    // 5. Convert original page from LEAF to INNER
    std::memset(reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage), 0,
                8192 - sizeof(SBSPGiSTPage));
    page->spgist_node_type = static_cast<uint16_t>(SPGiSTNodeType::INNER);
    page->spgist_count = 1; // One inner tuple
    page->spgist_flags &= ~static_cast<uint16_t>(SPGiSTFlags::NEEDS_REPACK);

    // 6. Write inner tuple
    auto config = opclass_->config();
    size_t label_size = config.labelSize > 0 ? config.labelSize : 4;

    uint16_t inner_size = sizeof(SBSPGiSTInnerTuple) +
                         prefix.size() +
                         (labels.size() * label_size) +
                         (labels.size() * sizeof(uint64_t));

    if (inner_size > (8192 - sizeof(SBSPGiSTPage)))
    {
        if (ctx)
        {
            ctx->code = Status::INTERNAL_ERROR;
            ctx->message = "SP-GiST inner tuple too large";
        }
        return Status::INTERNAL_ERROR;
    }

    uint8_t* inner_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);
    SBSPGiSTInnerTuple* inner = reinterpret_cast<SBSPGiSTInnerTuple*>(inner_ptr);

    inner->inner_size = inner_size;
    inner->inner_nNodes = labels.size();
    inner->inner_prefixSize = prefix.size();
    inner->inner_reserved = 0;
    inner->inner_xmin = txn_manager_->getCurrentXid();
    inner->inner_xmax = 0;

    // Write prefix
    size_t offset = sizeof(SBSPGiSTInnerTuple);
    std::memcpy(inner_ptr + offset, prefix.data(), prefix.size());
    offset += prefix.size();

    // Write labels and child page numbers
    for (size_t i = 0; i < labels.size(); ++i)
    {
        // Write label
        if (labels[i].size() != label_size)
        {
            // Pad or truncate to fixed size
            std::vector<uint8_t> padded_label(label_size, 0);
            size_t copy_size = std::min(labels[i].size(), label_size);
            std::memcpy(padded_label.data(), labels[i].data(), copy_size);
            std::memcpy(inner_ptr + offset, padded_label.data(), label_size);
        }
        else
        {
            std::memcpy(inner_ptr + offset, labels[i].data(), label_size);
        }
        offset += label_size;

        // Write child page number
        std::memcpy(inner_ptr + offset, &child_pages[i], sizeof(uint64_t));
        offset += sizeof(uint64_t);
    }

    page->spgist_free_space = (8192 - sizeof(SBSPGiSTPage)) - inner_size;

    // Mark original page as dirty
    buffer_pool_->markPageDirty(page_num, ctx);

    LOG_INFO(INDEX, "SP-GiST: Leaf page %lu split into %zu children",
             page_num, labels.size());

    return Status::OK;
}
```

**Step 2: Fix insertRecursive() to Retry After Split** (3-5 hours)

Currently, insertRecursive() calls splitNode() but doesn't retry the insert. Need to:

```cpp
// In insertRecursive(), around line 132:
if (page->spgist_free_space < entry_size)
{
    // Need to split leaf into inner node
    status = splitNode(page_num, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Retry insert after split
    return insertRecursive(page_num, value, tid, current_xid, level, ctx);
}
```

### 2.4 Testing Requirements

**Unit Tests** (`tests/unit/test_spgist_split.cpp`):
1. [ ] Insert 51 values into SP-GiST leaf (forcing split)
2. [ ] Verify leaf converted to inner with correct prefix
3. [ ] Verify all child pages are leaves
4. [ ] Verify all values distributed correctly to partitions
5. [ ] Verify TIDs preserved during split
6. [ ] Verify xmin/xmax preserved during split
7. [ ] Search after split returns all original values
8. [ ] Insert after split continues to work correctly
9. [ ] Multi-level split (split then split child)
10. [ ] Concurrent inserts during split

**Acceptance Criteria**:
- All 10 tests pass
- Leaf-to-inner conversion works correctly
- All values distributed to correct partitions
- No data loss during split
- Tree can grow to arbitrary depth

---

## 3. Missing Feature 2: remove() Method

### 3.1 Problem Statement

**Current Code** (Lines 392-404):
```cpp
Status SPGiSTIndex::remove(const std::vector<uint8_t>& value,
                          const TID& tid,
                          uint64_t current_xid,
                          ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    // Logical deletion: find entry and set xmax
    // TODO: Implement entry lookup and deletion

    deleted_count_++;
    return Status::OK;
}
```

**Impact**:
- Entries are never actually found
- No tree traversal to locate matching entry
- xmax is never set, so entries remain visible forever
- deleted_count_ increments without doing anything useful
- Index grows unbounded

### 3.2 Solution: Full Tree Traversal for Entry Removal

**Architecture**:
```
Delete Request: value=Point(5,5), tid=(page=10, line=3)

1. Start at root
2. If INNER:
   - Use choose() to determine which child might contain the entry
   - Recursively descend into that child
3. If LEAF:
   - Scan entries for exact match (value + TID)
   - Set entry->leaf_xmax = current_xid
   - Mark page as HAS_GARBAGE
   - Increment deleted_count_
4. Return success/not found
```

**Why TID is Required**:
- SP-GiST can have duplicate values (multiple tuples with same indexed value)
- Must match both value AND TID to identify exact entry
- Similar to GiST index removal

### 3.3 Implementation Details

**Step 1: Add removeRecursive() Helper** (3-4 hours)

```cpp
// Add to spgist_index.h private section:
Status removeRecursive(uint64_t page_num,
                      const std::vector<uint8_t>& value,
                      const TID& tid,
                      uint64_t current_xid,
                      bool* found,
                      ErrorContext* ctx);
```

**Step 2: Implement remove() with Tree Traversal** (7-11 hours)

```cpp
// src/core/spgist_index.cpp

Status SPGiSTIndex::remove(const std::vector<uint8_t>& value,
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
            ctx->message = "SP-GiST index is empty";
        }
        return Status::NOT_FOUND;
    }

    bool found = false;
    Status status = removeRecursive(root_page_, value, tid, current_xid, &found, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (!found)
    {
        // Entry not found - this is not an error (idempotent delete)
        LOG_DEBUG(INDEX, "SP-GiST entry not found for deletion: TID %s",
                 tid.toString().c_str());
        return Status::OK;
    }

    deleted_count_++;
    LOG_DEBUG(INDEX, "SP-GiST entry deleted: TID %s, xid %lu",
             tid.toString().c_str(), current_xid);

    return Status::OK;
}

Status SPGiSTIndex::removeRecursive(uint64_t page_num,
                                    const std::vector<uint8_t>& value,
                                    const TID& tid,
                                    uint64_t current_xid,
                                    bool* found,
                                    ErrorContext* ctx)
{
    *found = false;

    SBSPGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    SPGiSTNodeType node_type = static_cast<SPGiSTNodeType>(page->spgist_node_type);

    if (node_type == SPGiSTNodeType::LEAF)
    {
        // Scan leaf entries for matching value + TID
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);

        for (uint16_t i = 0; i < page->spgist_count; ++i)
        {
            SBSPGiSTLeafTuple* leaf = reinterpret_cast<SBSPGiSTLeafTuple*>(entry_ptr);

            // Skip already deleted entries
            if (leaf->leaf_xmax != 0)
            {
                entry_ptr += leaf->leaf_size;
                continue;
            }

            // Skip entries not visible to current transaction
            if (!isEntryVisible(leaf->leaf_xmin, leaf->leaf_xmax, current_xid))
            {
                entry_ptr += leaf->leaf_size;
                continue;
            }

            // Check if TID matches
            if (leaf->leaf_tid != tid)
            {
                entry_ptr += leaf->leaf_size;
                continue;
            }

            // Extract leaf value
            std::vector<uint8_t> leaf_value(leaf->leaf_valueSize);
            std::memcpy(leaf_value.data(), entry_ptr + sizeof(SBSPGiSTLeafTuple),
                       leaf->leaf_valueSize);

            // Check if value matches (exact comparison)
            if (leaf_value.size() == value.size() &&
                std::memcmp(leaf_value.data(), value.data(), value.size()) == 0)
            {
                // Found the entry - perform logical deletion
                leaf->leaf_xmax = current_xid;
                page->spgist_flags |= static_cast<uint16_t>(SPGiSTFlags::HAS_GARBAGE);
                page->spgist_deleted_entries++;
                *found = true;

                LOG_DEBUG(INDEX, "SP-GiST entry marked deleted on page %lu: TID %s, xmax=%lu",
                         page_num, tid.toString().c_str(), current_xid);

                // Mark page as dirty
                buffer_pool_->markPageDirty(page_num, ctx);

                return Status::OK;
            }

            entry_ptr += leaf->leaf_size;
        }

        // Not found on this leaf
        return Status::OK;
    }
    else // SPGiSTNodeType::INNER
    {
        // Extract inner node information
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);

        if (page->spgist_count == 0)
        {
            // Empty inner node
            return Status::OK;
        }

        SBSPGiSTInnerTuple* inner = reinterpret_cast<SBSPGiSTInnerTuple*>(entry_ptr);

        // Extract prefix
        std::vector<uint8_t> prefix(inner->inner_prefixSize);
        std::memcpy(prefix.data(), entry_ptr + sizeof(SBSPGiSTInnerTuple),
                   inner->inner_prefixSize);

        // Extract node labels
        auto config = opclass_->config();
        size_t label_size = config.labelSize > 0 ? config.labelSize : 4;
        size_t offset = sizeof(SBSPGiSTInnerTuple) + inner->inner_prefixSize;

        std::vector<SPGiSTNodeLabel> node_labels;
        for (uint16_t i = 0; i < inner->inner_nNodes; ++i)
        {
            SPGiSTNodeLabel label;
            label.data.resize(label_size);
            std::memcpy(label.data.data(), entry_ptr + offset, label_size);
            offset += label_size;

            std::memcpy(&label.child_page, entry_ptr + offset, sizeof(uint64_t));
            offset += sizeof(uint64_t);

            node_labels.push_back(label);
        }

        // Ask operator class which child to search
        SPGiSTTraversal traversal = opclass_->choose(prefix, node_labels, value);

        if (traversal.match_type == SPGiSTMatchType::MATCH_NODE &&
            traversal.node_index < node_labels.size())
        {
            // Descend into specific child
            uint64_t child_page = node_labels[traversal.node_index].child_page;
            status = removeRecursive(child_page, value, tid, current_xid, found, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            if (*found)
            {
                // Found and deleted in child
                return Status::OK;
            }
        }

        // If choose() says MATCH_ADD_NODE or MATCH_SPLIT, the entry doesn't exist
        // (these are for insert, not search)

        // Not found
        return Status::OK;
    }
}
```

### 3.4 Testing Requirements

**Unit Tests** (`tests/unit/test_spgist_remove.cpp`):
1. [ ] Insert 100 entries, delete all, verify all have xmax set
2. [ ] Delete non-existent entry (should succeed with NOT_FOUND)
3. [ ] Delete already deleted entry (idempotent)
4. [ ] Delete entry from multi-level tree (depth 3+)
5. [ ] Verify deleted entries are not returned by search()
6. [ ] Verify deleted entries are still returned to old transactions (MGA)
7. [ ] Delete entry with duplicate value (same value, different TID)
8. [ ] Concurrent insert/delete on same tree

**Acceptance Criteria**:
- All 8 tests pass
- Deleted entries have xmax set correctly
- Search operations do not return deleted entries (for newer transactions)
- MGA visibility rules respected (old transactions see pre-delete state)

---

## 4. Missing Feature 3: Garbage Collection

### 4.1 Problem Statement

**Current Code** (Lines 406-415):
```cpp
Status SPGiSTIndex::removeDeadEntries(uint64_t oldest_active_xid, ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    // TODO: Traverse tree and physically remove entries where xmax < oldest_active_xid
    LOG_INFO(INDEX, "SP-GiST garbage collection: %lu dead entries to remove",
             deleted_count_);

    return Status::OK;
}
```

**Impact**:
- Dead entries (xmax < oldest_active_xid) are never physically removed
- Pages remain bloated with deleted entries
- Free space not reclaimed
- Performance degrades as tree traversal must skip dead entries

### 4.2 Solution: Recursive Tree Garbage Collection

**Architecture**:
```
Garbage Collection Process:

1. Start at root page
2. For each page:
   a. If INNER: recurse into children first
   b. If LEAF or INNER: scan entries for xmax < oldest_active_xid
   c. Physically remove dead entries (compact page)
   d. Recalculate free space
3. Clear HAS_GARBAGE flag if all garbage removed
4. Update statistics (deleted_count_, total_entries)
```

**Considerations**:
- Must not remove entries visible to active transactions
- Must maintain tree structure
- Should be interruptible (long-running operation)
- Should handle concurrent reads

### 4.3 Implementation Details

**Step 1: Implement removeDeadEntries()** (4-7 hours)

```cpp
// src/core/spgist_index.cpp

Status SPGiSTIndex::removeDeadEntries(uint64_t oldest_active_xid, ErrorContext* ctx)
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
        LOG_DEBUG(INDEX, "SP-GiST garbage collection: no dead entries");
        return Status::OK;
    }

    LOG_INFO(INDEX, "SP-GiST garbage collection starting: %lu dead entries, OAT=%lu",
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

    LOG_INFO(INDEX, "SP-GiST garbage collection complete: %lu entries removed",
             removed_count);

    return Status::OK;
}

// Add to spgist_index.h private section:
Status gcRecursive(uint64_t page_num,
                  uint64_t oldest_active_xid,
                  uint64_t* removed_count,
                  ErrorContext* ctx);

// Implementation:
Status SPGiSTIndex::gcRecursive(uint64_t page_num,
                                uint64_t oldest_active_xid,
                                uint64_t* removed_count,
                                ErrorContext* ctx)
{
    SBSPGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    SPGiSTNodeType node_type = static_cast<SPGiSTNodeType>(page->spgist_node_type);
    bool has_garbage = (page->spgist_flags & static_cast<uint16_t>(SPGiSTFlags::HAS_GARBAGE)) != 0;

    // 1. If inner node, recurse into children first
    if (node_type == SPGiSTNodeType::INNER)
    {
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);

        if (page->spgist_count > 0)
        {
            SBSPGiSTInnerTuple* inner = reinterpret_cast<SBSPGiSTInnerTuple*>(entry_ptr);

            // Check if inner tuple itself is dead
            bool inner_dead = (inner->inner_xmax != 0) && (inner->inner_xmax < oldest_active_xid);
            if (inner_dead)
            {
                // Inner tuple is dead - all children are unreachable
                // This is rare, but can happen if entire subtree is deleted
                // For now, just clear the page (orphaned children will be reclaimed by VACUUM)
                page->spgist_count = 0;
                page->spgist_free_space = 8192 - sizeof(SBSPGiSTPage);
                page->spgist_flags &= ~static_cast<uint16_t>(SPGiSTFlags::HAS_GARBAGE);
                page->spgist_deleted_entries = 0;
                buffer_pool_->markPageDirty(page_num, ctx);
                return Status::OK;
            }

            // Extract child page numbers and recurse
            auto config = opclass_->config();
            size_t label_size = config.labelSize > 0 ? config.labelSize : 4;
            size_t offset = sizeof(SBSPGiSTInnerTuple) + inner->inner_prefixSize;

            for (uint16_t i = 0; i < inner->inner_nNodes; ++i)
            {
                offset += label_size; // Skip label

                uint64_t child_page;
                std::memcpy(&child_page, entry_ptr + offset, sizeof(uint64_t));
                offset += sizeof(uint64_t);

                // Recurse into child
                status = gcRecursive(child_page, oldest_active_xid, removed_count, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }
        }

        // Inner nodes themselves don't have entries to collect
        return Status::OK;
    }

    // 2. LEAF node: scan for dead entries
    if (!has_garbage)
    {
        // No garbage on this leaf, skip
        return Status::OK;
    }

    std::vector<size_t> live_entry_offsets;
    uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);

    for (uint16_t i = 0; i < page->spgist_count; ++i)
    {
        SBSPGiSTLeafTuple* leaf = reinterpret_cast<SBSPGiSTLeafTuple*>(entry_ptr);
        size_t entry_offset = entry_ptr - reinterpret_cast<uint8_t*>(page);

        bool is_dead = (leaf->leaf_xmax != 0) && (leaf->leaf_xmax < oldest_active_xid);

        if (is_dead)
        {
            // This entry is garbage
            (*removed_count)++;
            LOG_DEBUG(INDEX, "GC: Removing dead entry on page %lu: xmin=%lu, xmax=%lu",
                     page_num, leaf->leaf_xmin, leaf->leaf_xmax);
        }
        else
        {
            // This entry is still alive
            live_entry_offsets.push_back(entry_offset);
        }

        entry_ptr += leaf->leaf_size;
    }

    // 3. If we removed any entries, compact the page
    if (live_entry_offsets.size() < page->spgist_count)
    {
        // Create temporary buffer to hold live entries
        std::vector<uint8_t> temp_buffer;
        temp_buffer.reserve(8192 - sizeof(SBSPGiSTPage));

        uint8_t* page_bytes = reinterpret_cast<uint8_t*>(page);

        // Copy live entries to temporary buffer
        for (size_t offset : live_entry_offsets)
        {
            SBSPGiSTLeafTuple* leaf = reinterpret_cast<SBSPGiSTLeafTuple*>(page_bytes + offset);
            uint16_t entry_size = leaf->leaf_size;

            // Copy entire entry
            temp_buffer.insert(temp_buffer.end(),
                              page_bytes + offset,
                              page_bytes + offset + entry_size);
        }

        // Clear entry area
        size_t entry_area_start = sizeof(SBSPGiSTPage);
        size_t entry_area_size = 8192 - sizeof(SBSPGiSTPage);
        std::memset(page_bytes + entry_area_start, 0, entry_area_size);

        // Copy compacted entries back
        std::memcpy(page_bytes + entry_area_start, temp_buffer.data(), temp_buffer.size());

        // Update page metadata
        page->spgist_count = live_entry_offsets.size();
        page->spgist_free_space = entry_area_size - temp_buffer.size();
        page->spgist_flags &= ~static_cast<uint16_t>(SPGiSTFlags::HAS_GARBAGE);
        page->spgist_deleted_entries = 0;

        // Mark page as dirty
        buffer_pool_->markPageDirty(page_num, ctx);

        LOG_DEBUG(INDEX, "GC: Page %lu compacted: %lu live entries (was %u)",
                 page_num, live_entry_offsets.size(), page->spgist_count);
    }

    return Status::OK;
}
```

### 4.4 Testing Requirements

**Unit Tests** (`tests/unit/test_spgist_gc.cpp`):
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
1. `tests/unit/test_spgist_split.cpp` (10 tests)
2. `tests/unit/test_spgist_remove.cpp` (8 tests)
3. `tests/unit/test_spgist_gc.cpp` (8 tests)

**Total**: 26 new tests

### 5.2 Integration Tests

**Existing Integration Tests** (`tests/integration/test_spgist_index.cpp`):
- [ ] Verify all existing tests still pass after changes
- [ ] Add stress test: 10,000 inserts, 5,000 deletes, GC, verify correctness
- [ ] Add concurrency test: 4 threads inserting, 2 threads deleting, 1 thread GC
- [ ] Add MGA test: delete entry, verify old transaction still sees it
- [ ] Test quad-tree operator class with geometric queries
- [ ] Test text-ops operator class with prefix search

### 5.3 Performance Benchmarks

**Benchmarks** (`tests/benchmark/benchmark_spgist_index.cpp`):
- [ ] Measure insert throughput (entries/sec) before and after completion
- [ ] Measure delete throughput (entries/sec)
- [ ] Measure search latency with varying levels of dead entries
- [ ] Measure GC duration with 100K entries (50% dead)
- [ ] Compare SP-GiST quad-tree vs R-Tree for point queries
- [ ] Compare SP-GiST text-ops vs B-Tree for prefix search (LIKE 'abc%')

---

## 6. Implementation Breakdown

### 6.1 Task Breakdown

| Task | Effort (hours) | Dependency |
|------|----------------|------------|
| **Feature 1: picksplit() for Leaf Overflow** | **15-20** | - |
| 1.1 Complete splitNode() implementation | 12-15 | - |
| 1.2 Fix insertRecursive() to retry after split | 3-5 | 1.1 |
| 1.3 Unit tests for leaf splits | 2-3 | 1.1, 1.2 |
| **Feature 2: remove() Method** | **10-15** | - |
| 2.1 Add removeRecursive() signature | 0.5 | - |
| 2.2 Implement remove() with tree traversal | 3-4 | 2.1 |
| 2.3 Implement removeRecursive() leaf search | 4-6 | 2.2 |
| 2.4 Implement removeRecursive() inner descent | 3-4 | 2.3 |
| 2.5 Unit tests for remove operations | 2-3 | 2.2, 2.3, 2.4 |
| **Feature 3: Garbage Collection** | **5-10** | - |
| 3.1 Implement removeDeadEntries() main logic | 2-3 | - |
| 3.2 Implement gcRecursive() traversal | 3-5 | 3.1 |
| 3.3 Unit tests for garbage collection | 2-3 | 3.1, 3.2 |
| **Integration & Performance** | **3-5** | All |
| 4.1 Integration test updates | 2-3 | All |
| 4.2 Performance benchmarks | 1-2 | All |
| **TOTAL** | **33-50** | - |

### 6.2 Estimated Total Effort

**Realistic Estimate**: 30-40 hours (includes buffer time for debugging)

**Timeline**:
- Single developer (full-time): 1 week
- Part-time: 2 weeks

### 6.3 Critical Path

**Critical Path** (longest dependency chain):
1. Feature 1 (picksplit) must be completed for tree to grow beyond root
2. Feature 2 (remove) depends on Feature 1 being complete (need multi-level tree for testing)
3. Feature 3 (GC) depends on Feature 2 being complete (need entries to collect)
4. Integration tests depend on all features

**Recommended Order**:
1. Feature 1 (picksplit) - Unblocks tree growth
2. Feature 2 (remove) - Unblocks deletion
3. Feature 3 (GC) - Cleanup and optimization
4. Integration tests and benchmarks

---

## 7. MGA Compliance Checklist

**All SP-GiST operations must respect MGA rules:**

- [x] Current implementation uses `TransactionId` (uint64_t), not `Snapshot`
- [x] Visibility checks use `isEntryVisible()` with TIP lookups
- [x] Search operations pass `current_xid`, not `snapshot`
- [ ] remove() must set leaf_xmax correctly (TO IMPLEMENT)
- [ ] removeDeadEntries() must respect oldest_active_xid (TO IMPLEMENT)
- [x] Index entries store stable TIDs (never change)
- [x] MGA version chains respected (N2O)

**New Code Requirements**:
- [ ] splitNode() must preserve xmin/xmax during redistribution
- [ ] removeRecursive() must use `isEntryVisible(leaf_xmin, leaf_xmax, current_xid)`
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
- Operator class implementations (quad_ops, text_ops) - For pickSplit(), choose()

**Assumes**:
- Buffer pool is thread-safe
- Transaction manager TIP is correctly maintained
- Operator class methods are deterministic
- pickSplit() always returns valid partitions (no empty partitions)

### 8.2 Operator Class Requirements

**Each operator class must implement**:
- `choose()` - Already required, assumed working
- **`pickSplit()`** - Currently stub, MUST be properly implemented
- `innerConsistent()` - Already required, assumed working
- `leafConsistent()` - Already required, assumed working
- `config()` - Already required, assumed working

**pickSplit() Requirements**:
- Must return at least 2 partitions
- Partitions must be non-overlapping (SP-GiST invariant)
- All values must be assigned to some partition
- assignments.size() must equal values.size()
- labels.size() must be >= 2

**If pickSplit() is not correctly implemented, splits will fail.**

---

## 9. Known Limitations and Future Work

### 9.1 Current Limitations (After Completion)

**Not Included in This Spec**:
- Inner node split when too many children (future enhancement)
- Page merging when pages become sparse (future optimization)
- Compressed prefixes (future optimization)
- Bulk loading (future optimization)
- Range queries (future enhancement)

**These are OK for Phase 1, can be added later.**

### 9.2 Future Enhancements

**Phase 2 Enhancements** (not required now):
1. **Inner Node Split** (15-20 hours)
   - When inner node exceeds maxInnerNodes children
   - Split inner node into multiple inner nodes
   - Complex but rarely needed

2. **Page Merging** (20-30 hours)
   - When page drops below 30% full, merge with sibling
   - Reduces tree height, improves cache locality

3. **Prefix Compression** (10-15 hours)
   - Compress common prefixes in radix trees
   - Saves space, reduces I/O

4. **Bulk Loading** (20-30 hours)
   - Build SP-GiST index from sorted data
   - 10-100x faster than individual inserts

5. **Range Queries** (15-20 hours)
   - Support range scans (e.g., all points in box)
   - Currently only supports point queries

---

## 10. Acceptance Criteria

### 10.1 Functional Requirements

**Must Pass**:
- [ ] All 26 unit tests pass
- [ ] All integration tests pass
- [ ] Can insert 10,000 entries without error
- [ ] Can delete 5,000 entries (50%)
- [ ] Garbage collection removes all dead entries
- [ ] Search returns correct results after inserts/deletes
- [ ] Tree can grow to depth 5+ via splits
- [ ] Concurrent reads during writes work correctly

### 10.2 Performance Requirements

**Must Achieve**:
- [ ] Insert throughput: >10,000 entries/sec (single thread)
- [ ] Search latency: <1ms for 100K entry index (avg)
- [ ] Delete throughput: >5,000 entries/sec
- [ ] GC duration: <10 seconds for 100K entries (50% dead)
- [ ] Tree height: O(log n) for balanced operator classes (quad-tree)
- [ ] Prefix search: <0.5ms for 100K strings (text-ops)

### 10.3 MGA Compliance Requirements

**Must Verify**:
- [ ] Deleted entries visible to old transactions (snapshot isolation)
- [ ] Deleted entries invisible to new transactions
- [ ] GC only removes entries invisible to all active transactions
- [ ] No `Snapshot` structures used anywhere
- [ ] All visibility checks via `TransactionManager::isVersionVisible()`
- [ ] xmin/xmax correctly preserved during splits

---

## 11. Code Examples for Operator Classes

### 11.1 Example: Quad-Tree pickSplit() Implementation

This shows what a complete quad-tree operator class pickSplit() should look like:

```cpp
// Quad-tree operator class (for 2D points)
class QuadTreeOperatorClass : public SPGiSTOperatorClass
{
public:
    void pickSplit(
        const std::vector<std::vector<uint8_t>>& values,
        std::vector<uint8_t>& prefix,
        std::vector<std::vector<uint8_t>>& labels,
        std::vector<size_t>& assignments) const override
    {
        // Assume values are 2D points (8 bytes: 4 bytes x, 4 bytes y)

        // 1. Calculate centroid as prefix
        double sum_x = 0, sum_y = 0;
        for (const auto& value : values)
        {
            float x, y;
            std::memcpy(&x, value.data(), sizeof(float));
            std::memcpy(&y, value.data() + sizeof(float), sizeof(float));
            sum_x += x;
            sum_y += y;
        }

        float center_x = sum_x / values.size();
        float center_y = sum_y / values.size();

        // Store centroid as prefix (8 bytes)
        prefix.resize(8);
        std::memcpy(prefix.data(), &center_x, sizeof(float));
        std::memcpy(prefix.data() + sizeof(float), &center_y, sizeof(float));

        // 2. Create 4 quadrant labels (NW=0, NE=1, SW=2, SE=3)
        labels.resize(4);
        for (int i = 0; i < 4; ++i)
        {
            labels[i].resize(4);
            uint32_t quadrant = i;
            std::memcpy(labels[i].data(), &quadrant, sizeof(uint32_t));
        }

        // 3. Assign each point to a quadrant
        assignments.resize(values.size());
        for (size_t i = 0; i < values.size(); ++i)
        {
            float x, y;
            std::memcpy(&x, values[i].data(), sizeof(float));
            std::memcpy(&y, values[i].data() + sizeof(float), sizeof(float));

            // Determine quadrant
            if (x < center_x && y >= center_y)
                assignments[i] = 0; // NW
            else if (x >= center_x && y >= center_y)
                assignments[i] = 1; // NE
            else if (x < center_x && y < center_y)
                assignments[i] = 2; // SW
            else
                assignments[i] = 3; // SE
        }
    }
};
```

### 11.2 Example: Radix Tree pickSplit() Implementation

This shows what a radix tree operator class pickSplit() should look like:

```cpp
// Radix tree operator class (for strings/text prefix search)
class RadixTreeOperatorClass : public SPGiSTOperatorClass
{
public:
    void pickSplit(
        const std::vector<std::vector<uint8_t>>& values,
        std::vector<uint8_t>& prefix,
        std::vector<std::vector<uint8_t>>& labels,
        std::vector<size_t>& assignments) const override
    {
        // 1. Find common prefix
        prefix = findCommonPrefix(values);

        // 2. Group by next character after prefix
        std::map<char, std::vector<size_t>> char_groups;

        for (size_t i = 0; i < values.size(); ++i)
        {
            const auto& value = values[i];

            if (value.size() <= prefix.size())
            {
                // Value IS the prefix (or shorter) - use null byte
                char_groups['\0'].push_back(i);
            }
            else
            {
                // Get next character after prefix
                char next_char = value[prefix.size()];
                char_groups[next_char].push_back(i);
            }
        }

        // 3. Create labels (one per character group)
        labels.clear();
        std::vector<char> chars;

        for (const auto& [ch, indices] : char_groups)
        {
            chars.push_back(ch);

            std::vector<uint8_t> label(1);
            label[0] = static_cast<uint8_t>(ch);
            labels.push_back(label);
        }

        // 4. Assign values to partitions
        assignments.resize(values.size());
        for (size_t partition = 0; partition < chars.size(); ++partition)
        {
            for (size_t idx : char_groups[chars[partition]])
            {
                assignments[idx] = partition;
            }
        }
    }

private:
    std::vector<uint8_t> findCommonPrefix(const std::vector<std::vector<uint8_t>>& values) const
    {
        if (values.empty()) return {};

        std::vector<uint8_t> prefix = values[0];

        for (size_t i = 1; i < values.size(); ++i)
        {
            const auto& value = values[i];

            size_t min_len = std::min(prefix.size(), value.size());
            size_t j = 0;
            while (j < min_len && prefix[j] == value[j])
            {
                ++j;
            }

            prefix.resize(j);
            if (prefix.empty()) break;
        }

        return prefix;
    }
};
```

---

## 12. Conclusion

This specification provides complete implementation details for the 3 missing critical features in the SP-GiST index.

**Key Takeaways**:
- **picksplit()** is most critical (15-20 hours) - enables tree growth
- **remove()** is most impactful (10-15 hours) - enables deletion
- **Garbage collection** is most straightforward (5-10 hours) - enables space reclamation

**Completion Criteria**:
- All 26 unit tests pass
- All integration tests pass
- MGA visibility rules respected throughout
- Performance meets requirements
- Operator classes work correctly with splits

**Next Steps**:
1. Implement Feature 1 (picksplit) first - unblocks tree growth
2. Implement Feature 2 (remove) second - unblocks deletion
3. Implement Feature 3 (GC) third - completes the feature set
4. Integration testing and performance benchmarking

**Status**: SPECIFICATION COMPLETE ✅
**Implementation**: PENDING (30-40 hours)
