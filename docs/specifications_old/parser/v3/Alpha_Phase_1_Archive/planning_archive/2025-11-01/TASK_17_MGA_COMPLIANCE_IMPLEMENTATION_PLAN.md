# Task 17: MGA Compliance Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: October 31, 2025
**Priority**: 🔴 CRITICAL
**Estimated Effort**: 119-175 hours (3-4 weeks)

---

## Overview

This document outlines the detailed implementation plan to make Task 17 (Expression and Filtered Indexes) fully MGA-compliant with ScratchBird's Multi-Generational Architecture.

## Current State vs. Required State

### Current (MVCC-style - WRONG)
```cpp
// Index building - NO visibility checks
auto scan = db_->storage_engine()->createScan(table_id, nullptr);
while (scan->next(&tuple, nullptr) == OK) {
    btree->insert(key_bytes, tid);  // Direct insert, no versioning
}

// Index maintenance - NO transaction safety
btree->insert(key_bytes, tid);  // Permanent, cannot rollback
```

### Required (MGA-compliant - CORRECT)
```cpp
// Index building - WITH visibility checks
auto scan = db_->storage_engine()->createScan(table_id, trans, nullptr);
while (scan->next(&tuple, nullptr) == OK) {
    if (isVisible(tuple.xmin, tuple.xmax, trans->xid)) {
        btree->insertVersioned(key_bytes, tuple.uuid, trans->xid);
    }
}

// Index maintenance - WITH transaction safety
btree->insertVersioned(key_bytes, version_uuid, trans->xid);
logIndexOperation(trans, INDEX_INSERT, index_id, key_bytes, version_uuid);
```

---

## Implementation Phases

### Phase 1: Foundation (40-55 hours)

#### 1.1 Versioned Index Entry Structure (10-15 hours)

**File**: `include/scratchbird/core/btree.h`

**New Structure**:
```cpp
// Versioned index entry for MGA support
struct VersionedIndexEntry {
    std::vector<uint8_t> key_data;      // Index key
    ID                   version_uuid;   // UUID of record version
    uint64_t             xmin;           // Transaction that created entry
    uint64_t             xmax;           // Transaction that deleted entry (0 if active)
    uint32_t             flags;          // Entry flags

    // Constructor
    VersionedIndexEntry(const std::vector<uint8_t>& key,
                       const ID& uuid,
                       uint64_t created_by)
        : key_data(key)
        , version_uuid(uuid)
        , xmin(created_by)
        , xmax(0)
        , flags(0)
    {}
};

// Index entry flags
#define IDX_ENTRY_DELETED   0x0001  // Entry is deleted
#define IDX_ENTRY_HOT       0x0002  // HOT (Heap-Only Tuple) update
#define IDX_ENTRY_DUPLICATE 0x0004  // Multiple versions for same key
```

**Integration Points**:
- Update `SBBTreeNode` to support multiple versions
- Add `btn_xmin` and `btn_xmax` fields (already present!)
- Modify B-tree insertion to store version UUID
- Modify B-tree scan to filter by visibility

#### 1.2 Transaction Logging Infrastructure (15-20 hours)

**File**: `include/scratchbird/core/transaction_manager.h`

**New Structures**:
```cpp
// Index operation types for undo logging
enum class IndexOpType : uint8_t {
    INDEX_INSERT = 0,
    INDEX_DELETE = 1,
    INDEX_UPDATE = 2
};

// Index undo record
struct IndexUndoRecord {
    IndexOpType  op_type;
    ID           index_id;
    std::vector<uint8_t> key_data;
    ID           version_uuid;
    uint64_t     xid;

    // Serialization
    std::vector<uint8_t> serialize() const;
    static IndexUndoRecord deserialize(const uint8_t* data, size_t len);
};
```

**File**: `src/core/transaction_manager.cpp`

**New Methods**:
```cpp
// Log index operation for rollback
Status TransactionManager::logIndexOperation(
    uint32_t proc_id,
    uint64_t xid,
    const IndexUndoRecord& undo_record,
    ErrorContext* ctx)
{
    // Find transaction in proc array
    auto* proc = proc_array_->getProc(proc_id);
    if (!proc) {
        return Status::TransactionNotFound("...");
    }

    // Add to transaction's undo list
    proc->index_undos.push_back(undo_record);

    // Optionally write to WAL for crash recovery
    if (db_->wal_enabled()) {
        writeIndexUndoToWAL(xid, undo_record);
    }

    return Status::OK();
}

// Rollback index operations
Status TransactionManager::rollbackIndexOperations(
    uint32_t proc_id,
    uint64_t xid,
    ErrorContext* ctx)
{
    auto* proc = proc_array_->getProc(proc_id);
    if (!proc) {
        return Status::OK();  // Already cleaned up
    }

    // Replay undo records in reverse order
    for (auto it = proc->index_undos.rbegin();
         it != proc->index_undos.rend(); ++it) {
        const auto& undo = *it;

        // Get index
        auto index = db_->catalog()->getIndex(undo.index_id);
        if (!index) continue;  // Index dropped

        // Get B-tree
        auto btree = index->btree();

        // Undo operation
        switch (undo.op_type) {
            case IndexOpType::INDEX_INSERT:
                // Mark entry as deleted (set xmax = xid)
                btree->markDeleted(undo.key_data, undo.version_uuid, xid);
                break;

            case IndexOpType::INDEX_DELETE:
                // Restore entry (clear xmax)
                btree->unmarkDeleted(undo.key_data, undo.version_uuid);
                break;

            case IndexOpType::INDEX_UPDATE:
                // Rollback update (restore old entry, delete new)
                // Implementation depends on how updates are logged
                break;
        }
    }

    // Clear undo list
    proc->index_undos.clear();

    return Status::OK();
}
```

#### 1.3 Extend B-tree for Versioned Operations (15-20 hours)

**File**: `include/scratchbird/core/btree.h`

**New Methods**:
```cpp
class BTree {
public:
    // Existing methods...

    // NEW: Insert versioned index entry
    Status insertVersioned(
        const std::vector<uint8_t>& key,
        const ID& version_uuid,
        uint64_t xmin,
        ErrorContext* ctx = nullptr);

    // NEW: Mark entry as deleted (set xmax)
    Status markDeleted(
        const std::vector<uint8_t>& key,
        const ID& version_uuid,
        uint64_t xmax,
        ErrorContext* ctx = nullptr);

    // NEW: Unmark deleted entry (clear xmax)
    Status unmarkDeleted(
        const std::vector<uint8_t>& key,
        const ID& version_uuid,
        ErrorContext* ctx = nullptr);

    // NEW: Version-aware scan
    class VersionedScanIterator {
    public:
        bool next(uint64_t current_xid, ID& version_uuid_out);
    private:
        bool isEntryVisible(const SBBTreeNode* node, uint64_t current_xid);
    };

    // Create versioned scan
    std::unique_ptr<VersionedScanIterator> createVersionedScan(
        const std::vector<uint8_t>& start_key,
        const std::vector<uint8_t>& end_key);
};
```

**File**: `src/core/btree.cpp`

**Implementation**:
```cpp
Status BTree::insertVersioned(
    const std::vector<uint8_t>& key,
    const ID& version_uuid,
    uint64_t xmin,
    ErrorContext* ctx)
{
    // Find insertion point
    auto [page, slot] = findInsertionPoint(key);

    // Check if key already exists with different version
    // (this supports multiple versions for same key)
    if (keyExists(page, key)) {
        // Multiple versions allowed - insert as duplicate
        return insertDuplicate(page, key, version_uuid, xmin, ctx);
    }

    // Insert new entry
    SBBTreeNode node;
    node.btn_flags = 0;
    node.btn_key_len = key.size();
    node.btn_tuple_count = 1;
    node.btn_xmin = xmin;
    node.btn_xmax = 0;  // Not deleted

    // Store version UUID in tuple data
    std::vector<uint8_t> node_data = serializeVersionedNode(node, key, version_uuid);

    return insertNodeData(page, slot, node_data, ctx);
}

Status BTree::markDeleted(
    const std::vector<uint8_t>& key,
    const ID& version_uuid,
    uint64_t xmax,
    ErrorContext* ctx)
{
    // Find entry
    auto [page, node] = findEntry(key, version_uuid);
    if (!node) {
        return Status::NotFound("Index entry not found");
    }

    // Set xmax (deletion transaction)
    node->btn_xmax = xmax;
    node->btn_flags |= IDX_ENTRY_DELETED;

    // Mark page dirty
    markPageDirty(page);

    return Status::OK();
}

bool BTree::VersionedScanIterator::isEntryVisible(
    const SBBTreeNode* node,
    uint64_t current_xid)
{
    // Entry created by uncommitted transaction
    if (node->btn_xmin > current_xid) {
        return false;
    }

    // Entry deleted by this or earlier transaction
    if (node->btn_xmax != 0 && node->btn_xmax <= current_xid) {
        // Need to check if deleting transaction committed
        // For now, simple check:
        return false;
    }

    // Entry is visible
    return true;
}
```

---

### Phase 2: Integration (45-65 hours)

#### 2.1 Add Visibility Checks to Index Building (6-10 hours)

**File**: `src/sblr/executor.cpp:1309-1455` (buildExpressionIndex)

**Current Code**:
```cpp
void Executor::buildExpressionIndex(...) {
    auto scan = db_->storage_engine()->createScan(table_info.table_id, nullptr);
    while (scan->next(&tuple, nullptr) == core::Status::OK) {
        // PROBLEM: No visibility check!
        std::vector<Value> row_values;
        deserializeTuple(tuple.data, tuple.data_size, columns, row_values);
        btree->insert(key_bytes, tid);
    }
}
```

**Fixed Code**:
```cpp
void Executor::buildExpressionIndex(
    uint32_t proc_id,  // ADD: Process ID
    uint64_t xid,      // ADD: Transaction ID
    const core::CatalogManager::TableInfo &table_info,
    const core::ID &index_id)
{
    // Get transaction from proc array
    auto* proc = db_->transaction_manager()->getProcArray()->getProc(proc_id);
    if (!proc) {
        throw std::runtime_error("Transaction not found");
    }

    // Create scan WITH transaction context
    auto scan = db_->storage_engine()->createScan(table_info.table_id, nullptr);

    while (scan->next(&tuple, nullptr) == core::Status::OK) {
        // CHECK VISIBILITY
        if (!db_->storage_engine()->isVisible(
                tuple.xmin, tuple.xmax, xid)) {
            continue;  // Skip invisible version
        }

        // Deserialize VISIBLE version
        std::vector<Value> row_values;
        deserializeTuple(tuple.data, tuple.data_size, columns, row_values);

        // Evaluate expressions/predicates
        // ... (existing code) ...

        // Insert into B-tree WITH versioning
        btree->insertVersioned(key_bytes, tuple.uuid, xid);
    }
}
```

#### 2.2 Add Transaction Safety to Index Maintenance (15-25 hours)

**Files**:
- `src/sblr/executor.cpp:1553-1687` (updateIndexesOnInsert)
- `src/sblr/executor.cpp:1689-1866` (updateIndexesOnUpdate)
- `src/sblr/executor.cpp:1868-1990` (updateIndexesOnDelete)

**Key Changes**:
1. Add `proc_id` and `xid` parameters to all three methods
2. Use `insertVersioned()` instead of `insert()`
3. Log all index operations for rollback
4. Use version UUIDs instead of TIDs

**Example (updateIndexesOnInsert)**:
```cpp
void Executor::updateIndexesOnInsert(
    uint32_t proc_id,                    // ADD
    uint64_t xid,                        // ADD
    const core::ID &table_id,
    const core::CatalogManager::TableInfo &table_info,
    const std::vector<core::CatalogManager::ColumnInfo> &columns,
    const core::ID& version_uuid,        // CHANGE: Use UUID not TID
    const std::vector<Value> &row_values)
{
    for (const auto &index_info : table_indexes) {
        // Evaluate expressions
        // ... (existing code) ...

        // Insert versioned entry
        btree->insertVersioned(key_bytes, version_uuid, xid);

        // Log for rollback
        IndexUndoRecord undo;
        undo.op_type = IndexOpType::INDEX_INSERT;
        undo.index_id = index_info.index_id;
        undo.key_data = key_bytes;
        undo.version_uuid = version_uuid;
        undo.xid = xid;

        db_->transaction_manager()->logIndexOperation(
            proc_id, xid, undo, nullptr);
    }
}
```

#### 2.3 Update Expression Evaluator for Versions (12-18 hours)

**File**: `include/scratchbird/sblr/expression_evaluator.h`

**Current**:
```cpp
class ExpressionEvaluator {
public:
    ExpressionEvaluator(
        const std::vector<core::CatalogManager::ColumnInfo> &columns,
        const parser::StringPool *pool);

    Value evaluate(const parser::Expression *expr,
                  const std::vector<Value> &row_values);
};
```

**Required**:
```cpp
class ExpressionEvaluator {
public:
    ExpressionEvaluator(
        const std::vector<core::CatalogManager::ColumnInfo> &columns,
        const parser::StringPool *pool,
        const core::Database* db,          // ADD
        uint64_t xid);                     // ADD

    // Evaluate expression for specific version
    Value evaluateForVersion(
        const parser::Expression *expr,
        const core::ID& version_uuid);     // ADD

    // Evaluate predicate for version
    bool evaluatePredicateForVersion(
        const parser::Expression *predicate,
        const core::ID& version_uuid);     // ADD

private:
    const core::Database* db_;
    uint64_t xid_;

    // Fetch visible version
    core::ID fetchVisibleVersion(const core::ID& version_uuid);
};
```

**Implementation**:
```cpp
Value ExpressionEvaluator::evaluateForVersion(
    const parser::Expression *expr,
    const core::ID& version_uuid)
{
    // Fetch version by UUID
    auto version_data = db_->storage_engine()->fetchVersion(version_uuid);
    if (!version_data) {
        throw std::runtime_error("Version not found");
    }

    // Check visibility
    if (!db_->storage_engine()->isVisible(
            version_data->xmin, version_data->xmax, xid_)) {
        // Walk chain to find visible version
        core::ID visible_uuid = fetchVisibleVersion(version_uuid);
        version_data = db_->storage_engine()->fetchVersion(visible_uuid);
    }

    // Deserialize visible version
    std::vector<Value> row_values;
    deserializeVersion(version_data, row_values);

    // Evaluate expression on VISIBLE version
    return evaluate(expr, row_values);
}
```

#### 2.4 Implement Rollback Support (20-30 hours)

**File**: `src/sblr/executor.cpp`

**New Method**:
```cpp
void Executor::executeRollback(uint32_t proc_id, uint64_t xid) {
    // Rollback index operations
    db_->transaction_manager()->rollbackIndexOperations(
        proc_id, xid, nullptr);

    // Rollback table changes (existing code)
    // ...

    // Mark transaction as ABORTED
    db_->transaction_manager()->rollbackTransaction(
        proc_id, xid, nullptr);
}
```

**Integration Point**: Hook into existing ROLLBACK statement execution

---

### Phase 3: Query Planner (18-25 hours)

#### 3.1 Add Isolation Level Handling (8-12 hours)

**File**: `src/optimizer/query_planner.cpp`

**Current**:
```cpp
void QueryPlanner::generateIndexScanPaths(...) {
    // No isolation level checks
    if (canUseIndex(expr, index_expr)) {
        addPath(new IndexScanPath(index_id));
    }
}
```

**Required**:
```cpp
void QueryPlanner::generateIndexScanPaths(
    uint64_t xid,                        // ADD
    IsolationLevel isolation,            // ADD
    ...)
{
    // For filtered indexes, check if predicate matches snapshot
    if (index_info.is_partial_index) {
        // For SERIALIZABLE, predicate must match snapshot time
        if (isolation == IsolationLevel::SERIALIZABLE) {
            if (!predicateMatchesSnapshot(
                    index_info.predicate, xid)) {
                continue;  // Cannot use this index
            }
        }
    }

    // Check expression/predicate match
    if (canUseIndex(expr, index_expr)) {
        addPath(new IndexScanPath(index_id, xid));
    }
}
```

#### 3.2 Implement Visibility-Aware Index Scans (10-13 hours)

**File**: `src/core/btree.cpp`

**New Class**:
```cpp
class BTree::VersionedScanIterator {
public:
    VersionedScanIterator(
        BTree* btree,
        const std::vector<uint8_t>& start_key,
        const std::vector<uint8_t>& end_key,
        uint64_t xid,
        Database* db)
        : btree_(btree)
        , start_key_(start_key)
        , end_key_(end_key)
        , xid_(xid)
        , db_(db)
        , current_page_(nullptr)
        , current_slot_(0)
    {}

    bool next(ID& version_uuid_out) {
        while (true) {
            // Get next entry from B-tree
            if (!btree_scan_->next(&current_node_)) {
                return false;  // No more entries
            }

            // Check index entry visibility
            if (!isIndexEntryVisible(current_node_)) {
                continue;  // Skip invisible index entry
            }

            // Extract version UUID
            version_uuid_out = extractVersionUUID(current_node_);

            // Fetch actual record version
            auto version_data = db_->storage_engine()->fetchVersion(
                version_uuid_out);

            // Check record version visibility
            if (db_->storage_engine()->isVisible(
                    version_data->xmin,
                    version_data->xmax,
                    xid_)) {
                return true;  // Found visible version
            }

            // Record version not visible, continue
        }
    }

private:
    bool isIndexEntryVisible(const SBBTreeNode* node) {
        // Check xmin (creation)
        if (node->btn_xmin > xid_) {
            return false;  // Created after our snapshot
        }

        // Check xmax (deletion)
        if (node->btn_xmax != 0) {
            // Entry deleted - check if deletion visible
            TransactionState state;
            db_->transaction_manager()->getTransactionState(
                node->btn_xmax, state, nullptr);

            if (state == TransactionState::COMMITTED &&
                node->btn_xmax <= xid_) {
                return false;  // Deletion is visible
            }
        }

        return true;  // Entry is visible
    }
};
```

---

## Testing Strategy

### Unit Tests (Phase 4: 30-40 hours)

#### Test 1: Versioned Index Entries
```cpp
TEST(VersionedBTree, InsertMultipleVersions) {
    // Create index
    auto btree = createIndex();

    // Insert two versions of same key
    btree->insertVersioned(key1, uuid1, xid1);
    btree->insertVersioned(key1, uuid2, xid2);

    // Scan with xid1 - should see only first version
    auto scan = btree->createVersionedScan(key1, key1, xid1);
    ID uuid_out;
    ASSERT_TRUE(scan->next(uuid_out));
    EXPECT_EQ(uuid_out, uuid1);
    ASSERT_FALSE(scan->next(uuid_out));  // No more
}
```

#### Test 2: Transaction Rollback
```cpp
TEST(TransactionRollback, IndexOperationsRolledBack) {
    // Begin transaction
    uint64_t xid;
    tx_mgr->beginTransaction(proc_id, xid, nullptr);

    // Insert record
    auto uuid = insertRecord(table_id, data, xid);

    // Update indexes
    updateIndexesOnInsert(proc_id, xid, table_id, ..., uuid, row_values);

    // Verify index entry exists
    ASSERT_TRUE(indexContains(index_id, key, uuid));

    // Rollback
    tx_mgr->rollbackTransaction(proc_id, xid, nullptr);

    // Verify index entry marked deleted
    auto entry = findIndexEntry(index_id, key, uuid);
    EXPECT_EQ(entry->xmax, xid);
}
```

#### Test 3: Isolation Levels
```cpp
TEST(IsolationLevels, SerializableSeesSnapshot) {
    // TX1: Create filtered index
    CREATE INDEX idx ON users (email) WHERE active = true;

    // TX2: Begin SERIALIZABLE
    beginTransaction(xid2, SERIALIZABLE);
    SELECT * FROM users WHERE active = true;  // Uses index

    // TX3: Insert new active user
    beginTransaction(xid3, READ_COMMITTED);
    INSERT INTO users (email, active) VALUES ('new@test.com', true);
    COMMIT;

    // TX2: Re-query (should NOT see new row - snapshot isolation)
    SELECT * FROM users WHERE active = true;
    // Should not include 'new@test.com' even though it matches predicate
}
```

---

## Implementation Order

### Week 1 (40 hours)
- [ ] Day 1-2: Versioned index entry structure (10h)
- [ ] Day 3-4: Transaction logging infrastructure (15h)
- [ ] Day 5: Extend B-tree for versioned operations (15h)

### Week 2 (45 hours)
- [ ] Day 1: Add visibility checks to index building (8h)
- [ ] Day 2-3: Transaction safety in maintenance (18h)
- [ ] Day 4-5: Update expression evaluator (15h)
- [ ] Weekend: Buffer time (4h)

### Week 3 (40 hours)
- [ ] Day 1-3: Implement rollback support (24h)
- [ ] Day 4-5: Add isolation level handling (10h)
- [ ] Weekend: Buffer time (6h)

### Week 4 (30 hours)
- [ ] Day 1-2: Visibility-aware scans (12h)
- [ ] Day 3-5: Testing and debugging (18h)

---

## Risk Assessment

### High Risk Areas
1. **B-tree modification complexity** - Versioning changes core data structure
2. **Transaction coordinator integration** - Complex locking scenarios
3. **Performance regression** - Visibility checks add overhead

### Mitigation Strategies
1. Implement feature flag to enable/disable versioned indexes
2. Extensive unit testing before integration
3. Performance benchmarking at each phase
4. Rollback plan if issues discovered

---

## Success Criteria

### Phase 1
- [ ] B-tree supports versioned entries
- [ ] Transaction logging functional
- [ ] Unit tests pass

### Phase 2
- [ ] Index building checks visibility
- [ ] INSERT/UPDATE/DELETE maintain indexes with transactions
- [ ] Expression evaluator handles versions
- [ ] Rollback works correctly

### Phase 3
- [ ] Query planner respects isolation levels
- [ ] Index scans filter by visibility
- [ ] All isolation levels work correctly

### Phase 4
- [ ] All unit tests pass
- [ ] Integration tests pass
- [ ] Concurrent transaction tests pass
- [ ] Performance acceptable (< 10% regression)

---

## Files to Modify

### Headers
- `include/scratchbird/core/btree.h` - Versioned structures
- `include/scratchbird/core/transaction_manager.h` - Index undo logging
- `include/scratchbird/sblr/executor.h` - Updated signatures
- `include/scratchbird/sblr/expression_evaluator.h` - Version-aware API

### Implementation
- `src/core/btree.cpp` - Versioned operations
- `src/core/transaction_manager.cpp` - Undo logging
- `src/sblr/executor.cpp` - All index maintenance
- `src/sblr/expression_evaluator.cpp` - Version fetching
- `src/optimizer/query_planner.cpp` - Isolation levels

### Tests
- `tests/unit/test_versioned_btree.cpp` - NEW
- `tests/unit/test_index_rollback.cpp` - NEW
- `tests/unit/test_index_isolation.cpp` - NEW

---

## Next Actions

1. ✅ Review this plan with team
2. Create feature branch `task-17-mga-compliance`
3. Start with Phase 1.1 (versioned index entries)
4. Implement incrementally with tests
5. Code review after each phase

---

**Document Version**: 1.0
**Last Updated**: October 31, 2025
**Estimated Completion**: November 28, 2025 (4 weeks from start)
