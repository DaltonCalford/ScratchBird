# B-Tree Implementation Plan

**Date:** 2025-10-02  
**Status:** 🔨 IN PROGRESS  
**Objective:** Complete the B-Tree index implementation with full CRUD operations

## Current State Analysis

### What Exists:
✅ **Data Structures** (btree.h, btree_page.h):
- SBBTreePage structure (160 bytes header)
- SBBTreeNode structure (36 bytes)
- Page and node flags enums
- BTreePage helper class with add_node()

✅ **Basic Operations** (btree.cpp):
- Constructor/Destructor
- `insert()` - Works until page is full
- `search()` - Binary search working
- `remove()` - Marks nodes as deleted
- `find_leaf_page()` - Tree traversal working

### What's Missing:
❌ **Critical Operations:**
1. Page split (leaf and internal)
2. Insert into parent after split
3. Create/open index methods
4. Root page creation
5. Page merge/rebalance
6. Physical node removal (currently only marks deleted)

❌ **Advanced Features:**
7. Prefix compression
8. Range scans
9. Bulk loading
10. Vacuum/cleanup deleted nodes

## Implementation Phases

### Phase 1: Core Split Operations (Priority 1)
**Goal:** Enable unlimited inserts by implementing page splits

#### 1.1 Leaf Page Split
**File:** `src/core/btree.cpp`

```cpp
Status BTree::split_leaf_page(
    uint64_t left_page_num,
    const std::vector<uint8_t>& new_key,
    uint64_t new_tuple_id,
    ErrorContext* ctx)
{
    // 1. Allocate new right page
    // 2. Determine split point (median)
    // 3. Move second half of nodes to right page
    // 4. Update sibling pointers
    // 5. Get separator key (first key on right page)
    // 6. Call insert_into_parent()
}
```

#### 1.2 Internal Page Split
```cpp
Status BTree::split_internal_page(
    uint64_t left_page_num,
    const std::vector<uint8_t>& new_key,
    uint64_t new_child_page,
    ErrorContext* ctx)
{
    // Similar to leaf split but:
    // - Move child pointers
    // - Separator key is promoted (not copied)
}
```

#### 1.3 Insert Into Parent
```cpp
Status BTree::insert_into_parent(
    uint64_t left_page,
    const std::vector<uint8_t>& separator_key,
    uint64_t right_page,
    ErrorContext* ctx)
{
    // 1. Get parent page number
    // 2. If no parent (was root), create new root
    // 3. Try to insert separator into parent
    // 4. If parent full, split parent recursively
}
```

#### 1.4 Create New Root
```cpp
Status BTree::create_new_root(
    uint64_t left_child,
    const std::vector<uint8_t>& separator_key,
    uint64_t right_child,
    ErrorContext* ctx)
{
    // 1. Allocate new root page
    // 2. Set as internal page with single entry
    // 3. Update index_info_.idx_root_page
    // 4. Increase tree height
}
```

### Phase 2: Index Creation & Management (Priority 2)

#### 2.1 Create Index Method
```cpp
static Status BTree::create(
    Database* db,
    const UuidV7Bytes& index_uuid,
    const UuidV7Bytes& table_uuid,
    const std::vector<UuidV7Bytes>& column_uuids,
    uint32_t* root_page_out,
    ErrorContext* ctx)
{
    // 1. Allocate root page
    // 2. Initialize as leaf page (single page tree)
    // 3. Set all metadata
    // 4. Return root page number
}
```

#### 2.2 Open Index Method
```cpp
static std::unique_ptr<BTree> BTree::open(
    Database* db,
    const UuidV7Bytes& index_uuid,
    uint32_t root_page,
    ErrorContext* ctx)
{
    // 1. Load root page
    // 2. Validate index metadata
    // 3. Load SBBTreeIndex structure
    // 4. Return BTree instance
}
```

### Phase 3: Range Scans & Iteration (Priority 3)

#### 3.1 Range Scan Iterator
```cpp
class BTreeIterator {
public:
    Status seek(const std::vector<uint8_t>& start_key);
    bool next(std::vector<uint8_t>* key_out, uint64_t* tuple_id_out);
    bool has_next();
private:
    BTree* btree_;
    uint64_t current_page_;
    uint16_t current_node_;
};

Status BTree::create_scan(
    const std::vector<uint8_t>* start_key,
    const std::vector<uint8_t>* end_key,
    BTreeIterator** iter_out,
    ErrorContext* ctx);
```

### Phase 4: Prefix Compression (Priority 4)

#### 4.1 Compression Helper
```cpp
uint16_t BTree::calculate_prefix_length(
    const std::vector<uint8_t>& key1,
    const std::vector<uint8_t>& key2)
{
    uint16_t prefix = 0;
    size_t min_len = std::min(key1.size(), key2.size());
    while (prefix < min_len && key1[prefix] == key2[prefix]) {
        prefix++;
    }
    return prefix;
}

void BTree::compress_key(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& prev_key,
    std::vector<uint8_t>* compressed_out,
    uint16_t* prefix_len_out);

void BTree::decompress_key(
    const std::vector<uint8_t>& compressed,
    const std::vector<uint8_t>& prev_key,
    uint16_t prefix_len,
    std::vector<uint8_t>* key_out);
```

### Phase 5: Page Maintenance (Priority 5)

#### 5.1 Physical Node Removal
```cpp
Status BTree::compact_page(uint64_t page_num, ErrorContext* ctx)
{
    // 1. Collect all live nodes
    // 2. Rebuild page without deleted nodes
    // 3. Update free space
}
```

#### 5.2 Page Merge/Rebalance
```cpp
Status BTree::try_merge_pages(
    uint64_t left_page,
    uint64_t right_page,
    ErrorContext* ctx)
{
    // 1. Check if combined size fits in one page
    // 2. Move all nodes from right to left
    // 3. Update parent
    // 4. Deallocate right page
}
```

### Phase 6: Bulk Loading (Priority 6)

```cpp
Status BTree::bulk_load(
    std::vector<std::pair<std::vector<uint8_t>, uint64_t>>& sorted_entries,
    ErrorContext* ctx)
{
    // 1. Build leaf pages left to right
    // 2. Build internal levels bottom-up
    // 3. Much faster than individual inserts
}
```

## Implementation Order

### Week 1: Core Functionality
1. ✅ Day 1: Split leaf page
2. ✅ Day 2: Split internal page  
3. ✅ Day 3: Insert into parent
4. ✅ Day 4: Create/open methods
5. ✅ Day 5: Testing & fixes

### Week 2: Advanced Features
6. Range scan iterator
7. Prefix compression
8. Page merge/rebalance
9. Bulk loading
10. Comprehensive testing

## Test Plan

### Unit Tests:
1. `test_btree_split_leaf` - Verify leaf page split
2. `test_btree_split_internal` - Verify internal split
3. `test_btree_grow_tree` - Insert until multiple splits
4. `test_btree_range_scan` - Range queries
5. `test_btree_prefix_compression` - Key compression
6. `test_btree_large_dataset` - 100K entries
7. `test_btree_duplicates` - Multiple tuple IDs per key
8. `test_btree_delete_and_compact` - Cleanup

### Integration Tests:
1. Create index via catalog
2. Bulk insert performance
3. Concurrent inserts (later with locking)
4. Recovery after crash (later with WAL)

## Data Structure Details

### Page Layout (8KB example):
```
Offset 0:    SBBTreePage header (160 bytes)
Offset 160:  Node offset array [uint16_t * count]
Offset ???:  Free space
Offset ???:  Node N data (grows from end)
...
Offset 8160: Node 1 data
```

### Node Storage Order:
- Nodes stored in reverse order from page end
- Offset array grows forward from header
- Free space is contiguous in middle

### Split Point Calculation:
```cpp
// For even distribution:
int split_idx = (total_nodes + 1) / 2;

// For write-heavy workload (append optimization):
int split_idx = (total_nodes * 2) / 3;  // 67% left, 33% right
```

## Error Handling

### Split Failures:
- Out of pages → Return PAGE_FULL
- Allocation failure → Rollback split
- Parent split fails → Recursive rollback

### Consistency:
- Use WAL for atomic splits (Phase 2+)
- Temporary INCOMPLETE flag during split
- Recovery can complete interrupted splits

## Performance Targets

### After Phase 1:
- Insert: ~10K ops/sec
- Search: ~50K ops/sec  
- Tree height < 4 for 1M entries

### After Phase 2:
- Bulk load: ~100K ops/sec
- Range scan: ~200K tuples/sec
- Compression: 30-50% space savings

## Files to Modify

1. `include/scratchbird/core/btree.h` - Add new methods
2. `src/core/btree.cpp` - Implement split logic
3. `include/scratchbird/core/btree_page.h` - Helper methods
4. `src/core/btree_page.cpp` - Page operations
5. `tests/unit/test_btree.cpp` - New test file
6. `docs/specifications/INDEX_IMPLEMENTATION_SPEC.md` - Update status

## Success Criteria

✅ Phase 1 Complete When:
- Can insert 1M entries without PAGE_FULL error
- Tree automatically grows to handle load
- Search still works after multiple splits
- All splits maintain B-tree properties

✅ Full Implementation Complete When:
- All CRUD operations work correctly
- Range scans implemented
- Prefix compression working
- 100K+ entries in tests pass
- Integration with catalog manager
- Documentation updated

## References

- Specification: `docs/specifications/LOW_LEVEL_SPECIFICATION_B-TREE_INDEX.md`
- Existing code: `src/core/btree.cpp`, `src/core/btree_page.cpp`
- Hash index for patterns: `src/core/hash_index.cpp`
