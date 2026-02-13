# Task 17: MGA Infrastructure Assessment

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: October 31, 2025
**Assessment**: ScratchBird MGA infrastructure is MORE complete than initially thought!

---

## Executive Summary

After detailed analysis, ScratchBird **ALREADY HAS** most MGA infrastructure in place:

✅ **Transaction Manager** - Full transaction lifecycle management
✅ **Visibility Checking** - `isVisible(xmin, xmax, current_xid)` method
✅ **B-tree Versioning** - `btn_xmin` and `btn_xmax` fields already present
✅ **TID-based Addressing** - GPID + slot for stable tuple references
✅ **Isolation Levels** - READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE support

The problem is **NOT** missing infrastructure - it's that **Task 17 doesn't use it**!

---

## What Exists (Infrastructure Already Built)

### 1. Transaction Manager (`transaction_manager.h/cpp`)

**Features**:
- Transaction lifecycle: begin, commit, rollback
- TIP (Transaction Inventory Page) for state tracking
- Visibility checks: `is TransactionVisible(xid, current_xid)`
- Isolation level support
- Group commit optimization

**Methods Available**:
```cpp
Status beginTransaction(uint32_t proc_id, uint64_t& xid_out, ...);
Status commitTransaction(uint32_t proc_id, uint64_t xid, ...);
Status rollbackTransaction(uint32_t proc_id, uint64_t xid, ...);
bool isTransactionVisible(uint64_t xid, uint64_t current_xid);
```

### 2. Storage Engine Visibility (`storage_engine.h/cpp:350-399`)

**Method**:
```cpp
bool isVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) const;
```

**Implementation**:
- Uses TransactionManager for visibility checks
- Validates XIDs to detect corruption
- "See own changes" logic (xmin == current_xid)
- Supports all isolation levels via ConnectionContext
- Falls back to READ_COMMITTED if no context

### 3. B-tree Versioning Support (`btree.h:107-127`)

**Structure** (`SBBTreeNode`):
```cpp
struct SBBTreeNode {
    uint16_t btn_flags;
    uint16_t btn_prefix_len;
    uint16_t btn_suffix_trunc;
    uint16_t btn_key_len;
    uint32_t btn_tuple_count;
    uint64_t btn_child_page;

    // Multi-version support (ALREADY EXISTS!)
    uint64_t btn_xmin;  // Node creation transaction
    uint64_t btn_xmax;  // Node deletion transaction

    // Variable data: [key_data][tuple_ids]
};
```

**Page-level Versioning** (`SBBTreePage`):
```cpp
struct SBBTreePage {
    // ... other fields ...

    // Multi-version support for MGA (ALREADY EXISTS!)
    uint64_t btr_xmin;  // Page creation transaction
    uint64_t btr_xmax;  // Page deletion transaction (0 if active)
    uint64_t btr_lsn;   // Last LSN that modified this page
};
```

### 4. Snapshot Support (`btree.h:181, 194`)

**Methods**:
```cpp
Status search(const std::vector<uint8_t> &key,
             struct Snapshot *snapshot,  // For MVCC filtering!
             std::vector<TID> *tids_out,
             ErrorContext *ctx = nullptr);

std::unique_ptr<BTreeIterator> rangeScan(
    const std::vector<uint8_t> *start_key,
    const std::vector<uint8_t> *end_key,
    struct Snapshot *snapshot,  // MVCC snapshot for visibility
    bool start_inclusive = true,
    bool end_inclusive = false,
    ErrorContext *ctx = nullptr);
```

Note: These methods accept `Snapshot*` but **Task 17 passes nullptr**!

### 5. TID Structure (`tid.h`)

**Stable Tuple Addressing**:
```cpp
struct TID {
    GPID gpid;          // Global Page ID (tablespace + page)
    uint16_t slot;      // Slot number within page

    // Firebird MGA invariant: TIDs never change
};
```

---

## What's Missing (Task 17 Gaps)

### ❌ Issue 1: No Visibility Checks in Index Building

**Location**: `src/sblr/executor.cpp:1309-1455` (`buildExpressionIndex`)

**Current Code**:
```cpp
void Executor::buildExpressionIndex(...) {
    auto scan = db_->storage_engine()->createScan(table_info.table_id, nullptr);

    while (scan->next(&tuple, nullptr) == core::Status::OK) {
        // PROBLEM: No visibility check!
        // PROBLEM: Indexes ALL versions (committed and uncommitted)

        deserializeTuple(tuple.data, tuple.data_size, columns, row_values);

        // Evaluate expression/predicate
        // ...

        btree->insert(key_bytes, tid);  // Indexes everything!
    }
}
```

**What's Wrong**:
- Doesn't call `isVisible()` to check tuple visibility
- Doesn't pass transaction context to scan
- Indexes uncommitted data
- Indexes deleted tuples

**Fix Required**:
```cpp
void Executor::buildExpressionIndex(
    uint64_t xid,  // ADD: Transaction context
    ...) {

    while (scan->next(&tuple, nullptr) == core::Status::OK) {
        // Extract xmin/xmax from tuple header
        auto* hdr = reinterpret_cast<const HeapTupleHeader*>(tuple.data);

        // CHECK VISIBILITY
        if (!db_->storage_engine()->isVisible(hdr->xmin, hdr->xmax, xid)) {
            continue;  // Skip invisible tuple
        }

        // Only index VISIBLE tuples
        deserializeTuple(tuple.data, tuple.data_size, columns, row_values);
        btree->insert(key_bytes, tuple.tid);
    }
}
```

### ❌ Issue 2: No Transaction Context in Index Maintenance

**Location**: `src/sblr/executor.cpp:1553-1687` (`updateIndexesOnInsert`)

**Current Code**:
```cpp
void Executor::updateIndexesOnInsert(
    const core::ID &table_id,
    const core::CatalogManager::TableInfo &table_info,
    const std::vector<core::CatalogManager::ColumnInfo> &columns,
    const core::PageID page_id,    // PROBLEM: Uses page_id/slot_id
    const core::SlotID slot_id,    // PROBLEM: Not transaction ID!
    const std::vector<Value> &row_values)
{
    // Build TID
    core::TID tid(core::makeGPID(core::PRIMARY_TABLESPACE_ID,
                                 static_cast<uint64_t>(page_id)),
                 slot_id);

    // Insert into B-tree
    btree->insert(key_bytes, tid);  // NO transaction context!
}
```

**What's Wrong**:
- No `xid` parameter (no transaction context)
- Cannot log operation for rollback
- B-tree insert is permanent (cannot undo)
- No visibility tagging on index entries

**Fix Required**:
```cpp
void Executor::updateIndexesOnInsert(
    uint64_t xid,  // ADD: Transaction context
    const core::ID &table_id,
    ...
    const std::vector<Value> &row_values)
{
    // Insert with transaction context
    btree->insert(key_bytes, tid);

    // Set xmin on inserted index entry
    // (B-tree already has btn_xmin field!)
    // Implementation needed: btree->setEntryXmin(key_bytes, tid, xid);

    // Log for rollback
    // Implementation needed: logIndexOperation(xid, INSERT, index_id, key, tid);
}
```

### ❌ Issue 3: No Rollback Support

**Location**: Missing entirely

**What's Needed**:
1. Transaction logging for index operations
2. Undo records for INSERT/DELETE/UPDATE
3. Rollback handler to replay undo operations
4. Integration with TransactionManager::rollbackTransaction()

**Implementation**:
```cpp
struct IndexUndoRecord {
    enum OpType { INSERT, DELETE, UPDATE } op_type;
    ID index_id;
    std::vector<uint8_t> key;
    TID tid;
    uint64_t xid;
};

// In TransactionManager or new IndexManager
std::vector<IndexUndoRecord> tra_index_undos;  // Per-transaction undo list

void rollbackIndexOperations(uint64_t xid) {
    for (auto it = tra_index_undos.rbegin(); it != tra_index_undos.rend(); ++it) {
        if (it->xid == xid) {
            switch (it->op_type) {
                case INSERT:
                    // Mark entry as deleted (set btn_xmax = xid)
                    btree->markDeleted(it->key, it->tid, xid);
                    break;
                case DELETE:
                    // Restore entry (clear btn_xmax)
                    btree->unmarkDeleted(it->key, it->tid);
                    break;
            }
        }
    }
}
```

### ❌ Issue 4: Expression Evaluator Lacks Transaction Context

**Location**: `include/scratchbird/sblr/expression_evaluator.h`

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

**What's Missing**:
- No transaction context (no xid)
- Cannot fetch tuples by TID to check visibility
- Cannot walk version chains

**Fix Required**:
```cpp
class ExpressionEvaluator {
public:
    ExpressionEvaluator(
        const std::vector<core::CatalogManager::ColumnInfo> &columns,
        const parser::StringPool *pool,
        const core::Database* db,     // ADD
        uint64_t xid);                // ADD

    // Evaluate on visible version of tuple
    Value evaluateForTuple(
        const parser::Expression *expr,
        const TID& tid);              // ADD

private:
    const core::Database* db_;
    uint64_t xid_;

    // Fetch visible version and evaluate
    Value fetchAndEvaluate(const parser::Expression *expr, const TID& tid);
};
```

---

## Revised Implementation Plan

### Phase 1: Add Transaction Context (20-30 hours)

Since infrastructure exists, focus on **using it**:

1. **Add xid parameter to all index methods** (6-8h)
   - buildExpressionIndex(uint64_t xid, ...)
   - updateIndexesOnInsert(uint64_t xid, ...)
   - updateIndexesOnUpdate(uint64_t xid, ...)
   - updateIndexesOnDelete(uint64_t xid, ...)

2. **Add visibility checks to index building** (4-6h)
   - Extract xmin/xmax from tuple headers
   - Call isVisible() before indexing
   - Skip invisible tuples

3. **Pass Snapshot to B-tree operations** (6-8h)
   - Create Snapshot from current transaction
   - Pass to btree->search() and btree->rangeScan()
   - Let B-tree filter by visibility

4. **Update ExpressionEvaluator** (4-6h)
   - Add db and xid to constructor
   - Add methods to fetch tuples by TID
   - Check visibility before evaluating

### Phase 2: Transaction Logging (15-20 hours)

1. **Design IndexUndoRecord** (2-3h)
   - Define structure
   - Add serialization methods

2. **Implement logging in index maintenance** (6-8h)
   - Log INSERT/DELETE/UPDATE operations
   - Store in per-transaction undo list

3. **Implement rollback handler** (4-6h)
   - Replay undo records on abort
   - Mark entries as deleted (set btn_xmax)
   - Clean up undo list

4. **Integration with TransactionManager** (3-4h)
   - Hook into rollbackTransaction()
   - Test rollback scenarios

### Phase 3: B-tree Enhancements (10-15 hours)

1. **Implement markDeleted() method** (3-4h)
   - Set btn_xmax on B-tree node
   - Don't physically delete

2. **Implement unmarkDeleted() method** (2-3h)
   - Clear btn_xmax on restore

3. **Enhance visibility-aware scan** (4-6h)
   - Use Snapshot parameter
   - Filter entries by btn_xmin/btn_xmax
   - Integrate with isVisible()

4. **Testing** (2-3h)
   - Unit tests for versioned operations

---

## Comparison: Original vs. Revised Estimates

| Category | Original Estimate | Revised Estimate | Reduction |
|----------|------------------|------------------|-----------|
| Phase 1: Foundation | 40-55h | 20-30h | -50% |
| Phase 2: Integration | 45-65h | 15-20h | -67% |
| Phase 3: Query Planner | 18-25h | 10-15h | -44% |
| Phase 4: Testing | 30-40h | 20-30h | -25% |
| **TOTAL** | **119-175h** | **65-95h** | **-46%** |

**Why the reduction?**:
- B-tree already has xmin/xmax fields
- isVisible() already implemented
- TransactionManager already handles visibility
- Snapshot support already in B-tree API
- Just need to USE existing infrastructure!

---

## Immediate Next Steps

1. ✅ Document existing infrastructure (this document)
2. Start Phase 1.1: Add xid parameters
3. Start Phase 1.2: Add visibility checks
4. Start Phase 1.3: Pass Snapshot to B-tree
5. Test with simple INSERT/SELECT scenario

---

## Confidence Level

**HIGH** ✅

The infrastructure is solid. Task 17 just needs to:
- Pass transaction IDs
- Call visibility checks
- Use existing Snapshot API
- Add transaction logging

This is much simpler than building infrastructure from scratch!

---

**Last Updated**: October 31, 2025
**Assessment By**: AI Assistant
**Confidence**: HIGH (reviewed actual source code)
