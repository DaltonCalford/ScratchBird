# Task 17: MGA Compliance Analysis and Required Corrections

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: October 31, 2025
**Status**: 🔴 CRITICAL - MGA Compliance Issues Identified
**Priority**: HIGH - Must align with ScratchBird's Multi-Generational Architecture

---

## Executive Summary

Task 17 (Expression and Filtered Indexes) was implemented **without proper MGA integration**. The current implementation:

- ❌ Does NOT use TransactionId from record versions
- ❌ Does NOT check version visibility 
- ❌ Does NOT respect transaction isolation levels
- ❌ Does NOT walk version chains
- ❌ Operates on "current" row state without MGA awareness

**Impact**: Expression and filtered indexes are **NOT transaction-safe** and may:
1. Index uncommitted data
2. Return wrong results under concurrent access
3. Violate isolation guarantees
4. Cause data corruption on rollback

---

## ScratchBird MGA Architecture (Baseline)

### Key MGA Concepts

**1. Version Chains with UUIDs**
- Each record update creates new version with UUID
- Versions linked via `rhd_back_version` UUID pointer
- Each version tagged with `rhd_transaction` (TransactionId)

**2. Transaction Inventory Page (TIP)**
- Tracks transaction states (ACTIVE, COMMITTED, ABORTED)
- Used for visibility checks
- 64-bit TransactionIds (no wraparound)

**3. Visibility Rules**
- `sb_check_visibility()` determines which version is visible
- Respects isolation levels (READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE)
- Own changes always visible
- Walks version chain using `sb_get_visible_version()`

**4. Garbage Collection**
- Old versions cleaned by automatic sweep
- Triggered based on `tib_sweep_threshold`
- Uses `tib_oldest_active` (OAT) to determine what's safe to clean

---

## Task 17 Current Implementation Analysis

### ❌ ISSUE 1: No Transaction ID Usage in Index Building

**Location**: `src/sblr/executor.cpp:1309-1455` (`buildExpressionIndex`)

**Current Code**:
```cpp
void Executor::buildExpressionIndex(...) {
    // ...
    auto scan = db_->storage_engine()->createScan(table_info.table_id, nullptr);
    
    while (scan->next(&tuple, nullptr) == core::Status::OK) {
        // Deserialize row into values
        std::vector<Value> row_values;
        deserializeTuple(tuple.data, tuple.data_size, columns, row_values);
        
        // PROBLEM: No visibility check!
        // PROBLEM: No transaction context!
        // PROBLEM: Indexes whatever version scan returns!
        
        // Check predicate
        if (predicate) {
            bool matches = evaluator.evaluatePredicate(predicate, row_values);
            // ...
        }
        
        // Insert into B-tree
        btree->insert(key_bytes, tid);  // Indexes current version!
    }
}
```

**Problems**:
1. **No transaction context passed to scan** - Should pass current TransactionId
2. **No visibility check** - Should use `sb_check_visibility()` 
3. **Indexes all versions** - Should only index visible, committed rows
4. **Ignores isolation level** - Should respect transaction's isolation level

**MGA-Compliant Solution**:
```cpp
void Executor::buildExpressionIndex(
    const SBTransaction* build_trans,  // ADD: Transaction context
    const core::CatalogManager::TableInfo &table_info,
    const core::ID &index_id)
{
    // Create scan WITH transaction context
    auto scan = db_->storage_engine()->createScan(
        table_info.table_id, 
        build_trans,  // Pass transaction!
        nullptr
    );
    
    while (scan->next(&tuple, nullptr) == core::Status::OK) {
        // Get record header to check visibility
        SBRecordHeader* version = (SBRecordHeader*)tuple.data;
        
        // CHECK VISIBILITY FIRST!
        VisibilityState vis = sb_check_visibility(build_trans, version);
        if (vis != VIS_VISIBLE) {
            continue;  // Skip invisible or deleted versions
        }
        
        // Only index VISIBLE versions
        std::vector<Value> row_values;
        deserializeTuple(version->rhd_data, version->rhd_length, columns, row_values);
        
        // Rest of logic...
    }
}
```

**Fix Effort**: 6-10 hours
- Add transaction parameter
- Implement visibility checks
- Update all callers
- Test with concurrent transactions

---

### ❌ ISSUE 2: No Transaction Safety in Index Maintenance

**Location**: `src/sblr/executor.cpp:1553-1687` (`updateIndexesOnInsert`)

**Current Code**:
```cpp
void Executor::updateIndexesOnInsert(
    const core::ID &table_id,
    const core::CatalogManager::TableInfo &table_info,
    const std::vector<core::CatalogManager::ColumnInfo> &columns,
    const core::PageID page_id,
    const core::SlotID slot_id,
    const std::vector<Value> &row_values)
{
    // PROBLEM: No transaction context!
    // PROBLEM: Immediately inserts into B-tree (permanent!)
    // PROBLEM: Cannot rollback on transaction abort!
    
    for (const auto &index_info : table_indexes) {
        if (index_info.is_expression_index) {
            // Evaluate expressions
            for (auto *expr : expressions) {
                Value val = evaluator.evaluate(expr, row_values);
                key_values.push_back(val);
            }
        }
        
        // PROBLEM: Direct B-tree insert (not transactional!)
        btree->insert(key_bytes, tid);
    }
}
```

**Problems**:
1. **No transaction parameter** - Cannot log for rollback
2. **Immediate B-tree modification** - Cannot undo
3. **Indexes uncommitted data** - Violates MGA rules
4. **No version tagging** - Index entries not tagged with TransactionId

**MGA-Compliant Solution**:
```cpp
void Executor::updateIndexesOnInsert(
    SBTransaction* trans,  // ADD: Transaction context
    const core::ID &table_id,
    const core::CatalogManager::TableInfo &table_info,
    const std::vector<core::CatalogManager::ColumnInfo> &columns,
    const UUID& version_uuid,  // CHANGE: Use UUID instead of TID
    const std::vector<Value> &row_values)
{
    for (const auto &index_info : table_indexes) {
        // Evaluate expressions as before...
        
        // Create VERSIONED index entry
        IndexEntry entry;
        entry.key_data = key_bytes;
        entry.version_uuid = version_uuid;  // Point to specific version!
        entry.transaction_id = trans->tra_number;  // Tag with transaction!
        
        // Insert with transaction logging
        btree->insert_versioned(entry, trans);
        
        // Log for rollback
        log_index_operation(trans, INDEX_INSERT, index_info.index_id, entry);
    }
}
```

**Fix Effort**: 15-25 hours
- Design versioned index entries
- Implement transaction logging
- Implement rollback logic
- Update INSERT/UPDATE/DELETE maintenance
- Comprehensive testing

---

### ❌ ISSUE 3: Expression Evaluation Ignores Version Chains

**Location**: `src/sblr/expression_evaluator.cpp` (entire file)

**Current Implementation**:
The ExpressionEvaluator receives a single `row_values` vector and evaluates expressions. It has **no awareness** of:
- Which version of the row this represents
- Whether this version is visible to the current transaction
- Whether there are older/newer versions in the chain

**Problems**:
1. Cannot evaluate expressions on specific versions
2. Cannot walk version chain to find visible version
3. May evaluate expressions on uncommitted changes

**MGA-Compliant Solution**:
```cpp
class ExpressionEvaluator {
public:
    // ADD: Version-aware constructor
    ExpressionEvaluator(
        const std::vector<core::CatalogManager::ColumnInfo> &columns,
        const parser::StringPool *pool,
        const SBTransaction* trans,  // ADD: Transaction context
        const Database* dbb           // ADD: Database for version fetching
    );
    
    // ADD: Evaluate expression for specific version
    Value evaluateForVersion(
        const parser::Expression *expr,
        const UUID& version_uuid  // Specific version to evaluate
    ) {
        // Fetch version by UUID
        SBRecordHeader* version = sb_fetch_version(dbb_, version_uuid);
        
        // Check visibility
        if (sb_check_visibility(trans_, version) != VIS_VISIBLE) {
            // Walk chain to find visible version
            UUID visible_uuid = sb_get_visible_version_uuid(trans_, version_uuid);
            version = sb_fetch_version(dbb_, visible_uuid);
        }
        
        // Deserialize visible version
        std::vector<Value> row_values;
        deserialize_version(version, row_values);
        
        // Evaluate expression on VISIBLE version
        return evaluate(expr, row_values);
    }
    
private:
    const SBTransaction* trans_;
    const Database* dbb_;
};
```

**Fix Effort**: 12-18 hours
- Add transaction/database context
- Implement version fetching
- Implement visibility checks
- Update all callers

---

### ❌ ISSUE 4: Predicate Evaluation Doesn't Respect Isolation Levels

**Location**: `src/optimizer/predicate_matcher.cpp` (query planner)

**Current Implementation**:
PredicateMatcher compares two predicates for implication, but has **no awareness** of:
- Transaction isolation levels
- Which predicates should be evaluated at what snapshot
- READ_COMMITTED vs REPEATABLE_READ vs SERIALIZABLE semantics

**Problem**:
Query planner may use filtered index that's only correct for certain isolation levels.

**Example**:
```sql
-- Session 1 (SERIALIZABLE):
BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
SELECT * FROM orders WHERE created_at > '2024-10-01';

-- Session 2 (concurrent):
INSERT INTO orders (created_at) VALUES ('2024-10-15');
COMMIT;

-- Session 1 should NOT see new row (SERIALIZABLE)
-- But filtered index may include it if evaluated at READ_COMMITTED!
```

**MGA-Compliant Solution**:
Filtered index predicates must be evaluated with respect to the **transaction's snapshot**, not current database state.

**Fix Effort**: 8-12 hours
- Add isolation level checks
- Ensure predicates evaluated at correct snapshot
- Add test cases for each isolation level

---

### ❌ ISSUE 5: No Rollback Support for Index Operations

**Location**: All index maintenance code

**Current Implementation**:
```cpp
// When transaction aborts, indexes are NOT cleaned up!
void Executor::executeAbort() {
    // PROBLEM: No index rollback!
    // Indexes still contain entries from aborted transaction
    // Leads to index corruption and wrong query results
}
```

**MGA-Compliant Solution**:
```cpp
void Executor::executeAbort(SBTransaction* trans) {
    // Walk transaction's version list
    for (UUID version_uuid : trans->tra_versions) {
        // For each version created by this transaction:
        // 1. Find all index entries pointing to this version
        // 2. Remove them from B-trees
        
        for (IndexInfo& index : affected_indexes) {
            // Undo index operations logged for this transaction
            rollback_index_operations(trans, index.index_id);
        }
    }
    
    // Mark transaction as ABORTED in TIP
    tip_set_state(trans->tra_number, TX_ABORTED);
}
```

**Fix Effort**: 20-30 hours (same as transaction safety fix)

---

## Required MGA Components Not Yet Implemented

### 1. **Index Entry Versioning**

**Need**: Index entries must be versioned like table rows

**Design**:
```cpp
struct VersionedIndexEntry {
    std::vector<uint8_t> key_data;      // Index key
    UUID                 version_uuid;   // Points to specific record version
    TransactionId        created_by;     // Transaction that created this entry
    TransactionId        deleted_by;     // Transaction that deleted (or 0)
    uint32_t             flags;          // Entry flags
};

// B-tree must support multiple entries for same key (different versions)
// Scan must filter by visibility
```

**Fix Effort**: 25-35 hours
- Design versioned B-tree structure
- Implement version-aware scan
- Implement garbage collection for old index entries
- Update all index operations

---

### 2. **Transaction Logging for Index Operations**

**Need**: Log index changes for rollback

**Design**:
```cpp
enum IndexOpType {
    INDEX_INSERT,
    INDEX_DELETE,
    INDEX_UPDATE
};

struct IndexUndoRecord {
    IndexOpType  op_type;
    core::ID     index_id;
    std::vector<uint8_t> key_data;
    UUID         version_uuid;
    TransactionId transaction_id;
};

// Add to transaction's undo list
void log_index_operation(SBTransaction* trans, IndexUndoRecord undo) {
    trans->tra_index_undos.push_back(undo);
}

// On abort, replay undo records
void rollback_index_operations(SBTransaction* trans) {
    for (auto& undo : trans->tra_index_undos) {
        switch (undo.op_type) {
            case INDEX_INSERT:
                // Remove inserted entry
                btree->delete_versioned(undo.key_data, undo.version_uuid);
                break;
            case INDEX_DELETE:
                // Restore deleted entry
                btree->insert_versioned(undo.key_data, undo.version_uuid);
                break;
            // ...
        }
    }
}
```

**Fix Effort**: 15-20 hours

---

### 3. **Visibility-Aware Index Scans**

**Need**: Index scans must check version visibility

**Design**:
```cpp
class VersionedIndexScan {
public:
    bool next(SBTransaction* trans, UUID& version_uuid_out) {
        while (btree_scan->next(&entry)) {
            VersionedIndexEntry* idx_entry = (VersionedIndexEntry*)entry;
            
            // Check if index entry is visible
            if (idx_entry->deleted_by != 0) {
                // Entry deleted - check if deletion visible
                if (is_visible(trans, idx_entry->deleted_by)) {
                    continue;  // Skip deleted entry
                }
            }
            
            // Check if creation is visible
            if (!is_visible(trans, idx_entry->created_by)) {
                continue;  // Skip uncommitted entry
            }
            
            // Fetch actual record version
            SBRecordHeader* version = sb_fetch_version(dbb, idx_entry->version_uuid);
            
            // Check record version visibility
            if (sb_check_visibility(trans, version) == VIS_VISIBLE) {
                version_uuid_out = idx_entry->version_uuid;
                return true;
            }
        }
        return false;  // No more visible entries
    }
};
```

**Fix Effort**: 18-25 hours

---

## Summary of MGA Compliance Issues

| Issue | Severity | Impact | Fix Effort |
|-------|----------|--------|------------|
| **1. No visibility checks in index building** | 🔴 CRITICAL | Indexes uncommitted data | 6-10 hours |
| **2. No transaction safety in maintenance** | 🔴 CRITICAL | Cannot rollback, data corruption | 15-25 hours |
| **3. Expression evaluation ignores versions** | 🔴 CRITICAL | Wrong results, isolation violations | 12-18 hours |
| **4. No isolation level respect** | 🔴 CRITICAL | Wrong query results | 8-12 hours |
| **5. No rollback support** | 🔴 CRITICAL | Index corruption on abort | 20-30 hours |
| **6. Index entry versioning not implemented** | 🔴 CRITICAL | Foundation for all above | 25-35 hours |
| **7. Transaction logging missing** | 🔴 CRITICAL | Cannot undo operations | 15-20 hours |
| **8. Visibility-aware scans missing** | 🔴 CRITICAL | Returns wrong rows | 18-25 hours |

**Total Fix Effort**: **119-175 hours** (3-4 weeks full-time)

---

## Recommended Action Plan

### Phase 1: Foundation (40-55 hours)
1. Implement versioned index entries (25-35 hours)
2. Implement transaction logging (15-20 hours)

### Phase 2: Integration (45-65 hours)
3. Add visibility checks to index building (6-10 hours)
4. Add transaction safety to maintenance (15-25 hours)
5. Update expression evaluator (12-18 hours)
6. Implement rollback support (12-12 hours)

### Phase 3: Query Planner (18-25 hours)
7. Add isolation level handling (8-12 hours)
8. Implement visibility-aware scans (10-13 hours)

### Phase 4: Testing (30-40 hours)
9. Concurrent transaction tests
10. Isolation level tests
11. Rollback tests
12. Long-running transaction tests

**Total Estimated Effort**: **133-185 hours**

---

## Immediate Actions Required

### 🔴 BLOCKER: Do NOT use in production until MGA compliance is fixed

**Current Status**: Expression and filtered indexes are **NOT transaction-safe**

**Risks if deployed**:
1. Data corruption on transaction rollback
2. Wrong query results under concurrent access
3. Isolation level violations
4. Index entries pointing to garbage-collected versions
5. Potential crashes due to missing versions

### ✅ Safe Usage (Temporary Workaround)

Task 17 features CAN be used safely ONLY if:
1. **Single-user mode** (no concurrent transactions)
2. **No transaction rollbacks** (auto-commit only)
3. **No long-running transactions** (no version chain builds)
4. **Regular REBUILD INDEX** (to clean up inconsistencies)

---

## Conclusion

Task 17 implementation is **functionally correct** for MVCC-style databases but **fundamentally incompatible** with ScratchBird's MGA architecture. A complete rewrite of transaction handling is required before this can be considered production-ready.

**Recommendation**: Mark as "Experimental - Not MGA-compliant" in release notes and prioritize MGA compliance fixes.

---

**Last Updated**: October 31, 2025
**Analysis By**: AI Assistant (based on MGA specification review)
**Confidence**: HIGH (critical architectural mismatch identified)
**Priority**: CRITICAL (must fix before production use)
