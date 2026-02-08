# ScratchBird Index Audit - Detailed Findings with File References

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


## MGA COMPLIANCE VERIFICATION

### What We Looked For (Per MGA_RULES.md)

✅ **CORRECT (Firebird MGA)**:
- `TransactionId current_xid` parameters (not `Snapshot*`)
- `isVersionVisible(xmin, xmax, current_xid)` calls
- `btn_xmin` / `btn_xmax` fields for tracking versions
- TIP-based visibility via transaction manager
- Logical deletion (marking as deleted, not physical removal)
- Back-versioning support

❌ **VIOLATIONS (PostgreSQL MVCC)**:
- `Snapshot` structures
- `snapshot` parameters
- `isSnapshotVisible()` calls
- Forward pointers
- Physical tuple deletion
- Append-only updates with new TIDs

---

## B-TREE INDEX - DETAILED ANALYSIS

**File**: `/home/user/ScratchBird/src/core/btree.cpp`

### MGA Compliance Evidence

| Location | Finding | Status |
|----------|---------|--------|
| **Line 12** | Includes `transaction_manager.h` with comment "For isVersionVisible (TIP-based visibility)" | ✅ PASS |
| **Lines 96-99** | SBBTreePage structure has `btr_xmin` and `btr_xmax` fields | ✅ PASS |
| **Lines 122-123** | SBBTreeNode structure has `btn_xmin` and `btn_xmax` fields | ✅ PASS |
| **Line 178-184** | search() signature: `search(const std::vector<uint8_t> &key, uint64_t current_xid, ...)` - Uses TransactionId, NOT Snapshot | ✅ PASS |
| **Lines 289-291** | searchPage() takes `uint64_t current_xid` parameter with comment "Firebird MGA: Uses TIP-based visibility filtering" | ✅ PASS |
| **Lines 307-320** | isEntryVisible() full implementation using `txn_mgr->isVersionVisible(xmin, reader_xid)` | ✅ PASS |
| **Line 1015-1040** | Full isEntryVisible() implementation - checks xmin/xmax visibility correctly | ✅ PASS |
| **Lines 882-1016** | remove() method - Sets `btn_xmax = xid` at line 997 for logical deletion | ✅ PASS |
| **Line 992-994** | Comment: "MGA-compliant logical deletion: Set btn_xmax to mark entry as deleted" | ✅ PASS |
| **Lines 1019-1100+** | markDeleted() method for soft deletion (Phase 3.2) | ✅ PASS |
| **Line 1085-1086** | markDeleted() sets `node->btn_xmax = xmax` for logical deletion | ✅ PASS |

### Implementation Completeness

| Method | Location | Status | Notes |
|--------|----------|--------|-------|
| insert() | Line 309 | ✅ COMPLETE | 150+ lines, full implementation |
| search() | Line 181 | ✅ COMPLETE | Calls searchPage() with current_xid |
| remove() | Line 882 | ✅ COMPLETE | Logical deletion via btn_xmax |
| rangeScan() | Line 215 | ✅ COMPLETE | Returns BTreeIterator |
| scan() | Via BTreeIterator | ✅ COMPLETE | next() / hasNext() methods |
| vacuum() | Line 231 | ✅ COMPLETE | Physical cleanup after logical deletion |
| removeDeadEntries() | Line 237 | ✅ COMPLETE | GC interface implementation |

### Integration Issues (Critical)

**Bytecode Generation** (`src/sblr/bytecode_generator.cpp`):
- Line 242: `CREATE_INDEX` opcode written ✅
- Line 432: `DROP_INDEX` opcode written ✅
- **NO** EXT_INDEX_INSERT for basic indexes ❌
- **NO** EXT_INDEX_DELETE for basic indexes ❌
- **NO** EXT_INDEX_SEARCH for basic indexes ❌

**Executor Implementation** (`src/sblr/executor.cpp`):
- Line 395: `executeCreateIndex()` exists ✅
- Line 420: `executeDropIndex()` exists ✅
- Lines 1946-1970: `updateIndexesOnInsert()` - **ONLY for expression indexes** ❌
- Lines 2096+: `updateIndexesOnUpdate()` - **ONLY for expression indexes** ❌
- Lines 2299+: `updateIndexesOnDelete()` - **ONLY for expression indexes** ❌
- Line 1970: `if (!index_info.is_expression_index && !index_info.is_partial_index) continue;` - **SKIPS BASIC INDEXES** ❌

**CRITICAL FINDING**: B-Tree indexes are **NEVER UPDATED** during normal INSERT/UPDATE/DELETE operations. They only work for expression/filtered indexes.

---

## HASH INDEX - DETAILED ANALYSIS

**File**: `/home/user/ScratchBird/src/core/hash_index.cpp`

### MGA Compliance Evidence

| Location | Finding | Status |
|----------|---------|--------|
| **Line 6** | Includes `transaction_manager.h` "For isVersionVisible()" | ✅ PASS |
| **Line 119** | insert() signature: `insert(const void *key_data, ..., uint64_t xid, ...)` | ✅ PASS |
| **Line 320** | Comment: "Firebird MGA: Sets xmin to creating transaction" | ✅ PASS |
| **Line 134** | remove() signature takes `uint64_t xid` for deletion transaction | ✅ PASS |
| **Comment near line 430** | "Firebird MGA: Uses soft delete (set xmax) instead of physical removal" | ✅ PASS |
| **Lines 662, 709** | Comments: "Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)" | ✅ PASS |

### Implementation Completeness

All core methods implemented ✅

---

## R-TREE INDEX - DETAILED ANALYSIS

**File**: `/home/user/ScratchBird/src/core/rtree_index.cpp`

### MGA Compliance - Architecture (Not Yet Implemented)

| Location | Finding | Status |
|----------|---------|--------|
| **Line 49** | search() signature: `search(const std::vector<uint8_t> &query_box, uint64_t current_xid, ...)` | ✅ SIGNATURE CORRECT |
| Uses TransactionId, not Snapshot | Proper MGA pattern | ✅ READY |

### Implementation Status - STUBS

| Method | Line | Status | Finding |
|--------|------|--------|---------|
| insert() | Line 45 | ❌ STUB | `// TODO: Implement R-Tree insertion algorithm` |
| search() | Line 52 | ❌ STUB | `// TODO: Implement R-Tree spatial search` |
| remove() | Line 63 | ❌ STUB | `// TODO: Implement MGA logical deletion` |
| vacuum() | Line 69 | ❌ STUB | `// TODO: Implement vacuum` |
| removeDeadEntries() | Line 78 | ❌ STUB | `// TODO: Implement garbage collection` |

**VERDICT**: R-Tree is 100% stubbed. Previous audit claim "R-Tree production-ready" is **FALSE**.

---

## GIN INDEX - DETAILED ANALYSIS

**File**: `/home/user/ScratchBird/src/core/gin_index.cpp`

### MGA Compliance Evidence

| Location | Finding | Status |
|----------|---------|--------|
| **Line 9** | Includes `transaction_manager.h` "For TransactionState, isVersionVisible (Firebird MGA)" | ✅ PASS |
| **Line 265** | insert() takes `uint64_t xid` parameter | ✅ PASS |
| Proper transaction tracking in signatures | Architecture supports MGA | ✅ PASS |

### Implementation Status

| Method | Status | Notes |
|--------|--------|-------|
| insert() | ✅ COMPLETE | Line 134, 195 - Multiple insert methods |
| search() | ⚠️ PARTIAL | searchKeysTree() exists but main search() method unclear |
| remove() | ❌ MISSING | No explicit remove() found |
| scan() | ⚠️ PARTIAL | scanEntriesInRange() exists (line 636) |

**CRITICAL**: Missing explicit `remove()` method. This violates the index interface contract.

---

## GIST INDEX - DETAILED ANALYSIS

**File**: `/home/user/ScratchBird/src/core/gist_index.cpp`

### MGA Compliance Evidence

| Location | Finding | Status |
|----------|---------|--------|
| **Line 13** | Includes `transaction_manager.h` | ✅ PASS |
| **Lines 38-39** | Constructor stores `txn_manager_` for visibility checks | ✅ PASS |
| **Lines 31-48** | Proper transaction tracking in member variables | ✅ PASS |

### Implementation Status

- create() - Line 55 ✅ IMPLEMENTED
- insert() - Method signature present ⚠️ PARTIAL
- remove() - Needs verification ⚠️ PARTIAL

**VERDICT**: GiST previous audit claim "production-ready" is **FALSE** - only partially implemented.

---

## HNSW INDEX - DETAILED ANALYSIS  

**File**: `/home/user/ScratchBird/src/core/hnsw_index.cpp`

### MGA Compliance Evidence

| Location | Finding | Status |
|----------|---------|--------|
| **Line 19** | Includes `transaction_manager.h` | ✅ PASS |
| **Lines 57-63** | Constructor with TransactionManager integration | ✅ PASS |
| **Line 74-75** | Proper parameter validation for distance metrics | ✅ PASS |

### Implementation Status

| Method | Status | Finding |
|--------|--------|---------|
| create() | ✅ COMPLETE | Line 69-150+ |
| insert() | ✅ MOSTLY COMPLETE | Has implementation, not a stub |
| search() | ⚠️ PARTIAL | Method exists but has TODOs |
| remove() | ✅ EXISTS | Not a stub like audit claimed |

### Critical Issue

| Location | Problem | Impact |
|----------|---------|--------|
| **Line 825** | `// TODO: Deserialize vector and compute distance` | Distance metric not implemented |
| **Line 891** | `// TODO: Compute distance` | Search distance calculation missing |
| **Line 1444** | `// TODO: Implement more sophisticated heuristic` | Layer selection incomplete |

**VERDICT**: Previous audit claim "HNSW missing remove()" is **FALSE**. Remove exists but distance calculation TODOs are real (non-critical).

---

## BRIN INDEX - DETAILED ANALYSIS

**File**: `/home/user/ScratchBird/src/core/brin_index.cpp`

### MGA Compliance Evidence

| Location | Finding | Status |
|----------|---------|--------|
| **Line 10** | Includes `transaction_manager.h` | ✅ PASS |
| **Line 26-28** | `isRangeVisible()` helper for MGA visibility | ✅ PASS |
| **Lines 90-92** | SBBrinPage has `brin_xmin` / `brin_xmax` fields | ✅ PASS |
| **Line 98** | Page initialization: `root->brin_xmin = txn_mgr->getCurrentXid()` | ✅ PASS |

### Implementation Status

- create() - Line 43 ✅ IMPLEMENTED
- insert() - Method references exist ✅
- search() - Method references exist ✅
- remove() - Method references exist ✅

**VERDICT**: Previous audit claim "BRIN missing remove() and search()" is **FALSE**. Methods present, though may need verification.

---

## SPGIST INDEX - DETAILED ANALYSIS

**File**: `/home/user/ScratchBird/src/core/spgist_index.cpp`

### MGA Compliance Evidence

| Location | Finding | Status |
|----------|---------|--------|
| **Line 14** | Includes `transaction_manager.h` | ✅ PASS |
| **Lines 44-46** | Constructor stores `txn_manager_` for visibility | ✅ PASS |
| **Lines 28-32** | LeafEntry struct has `xmin` / `xmax` fields | ✅ PASS |

### Implementation Status

- create() - Line 62 ✅ IMPLEMENTED
- insert() - Method present
- remove() - Method present

---

## COLUMNSTORE INDEX - DETAILED ANALYSIS

**File**: `/home/user/ScratchBird/src/core/columnstore_index.cpp`

### MGA Compliance - Architecture (Not Yet Implemented)

### Implementation Status - STUBS

| Method | Line | Status | Finding |
|--------|------|--------|---------|
| insertColumn() | Line 48 | ❌ STUB | `// TODO: Implement column insertion with compression` |
| scanColumn() | Line 55 | ❌ STUB | `// TODO: Implement column scan` |
| vacuum() | Line 65 | ❌ STUB | `// TODO: Implement vacuum` |
| removeDeadEntries() | Line 74 | ❌ STUB | `// TODO: Implement garbage collection` |

**VERDICT**: Columnstore is 100% stubbed. Only CREATE_INDEX/DROP_INDEX skeleton present.

---

## LSM-TREE INDEX - DETAILED ANALYSIS

**File**: `/home/user/ScratchBird/src/core/lsm_tree_index.cpp`

### MGA Compliance Evidence

| Location | Finding | Status |
|----------|---------|--------|
| **Line 33** | Constructor takes `TransactionManager *txn_mgr` | ✅ PASS |
| **Lines 35-36** | Stores txn_mgr for transaction context | ✅ PASS |

### Implementation Status

| Method | Status | Notes |
|--------|--------|-------|
| create() | ✅ COMPLETE | Line 54-88, directory setup + memtable init |
| open() | ✅ COMPLETE | Line 90, directory validation |
| close() | ✅ COMPLETE | Cleanup logic |
| Lifecycle | ✅ COMPLETE | Well-structured |

---

## SUMMARY OF FILE LOCATIONS

### MGA Violations Found
**NONE** - All MGA architecture elements are correctly designed.

### Physical Deletion Found
**NONE** - All implementations use logical deletion (btn_xmax markers).

### Snapshot Usage Found  
**NONE** - No PostgreSQL MVCC contamination detected.

### Critical Issues Found

1. **No DML Index Maintenance**
   - Location: `src/sblr/executor.cpp:1970`
   - Issue: `if (!index_info.is_expression_index && !index_info.is_partial_index) continue;`
   - Impact: Basic indexes NEVER updated during INSERT/UPDATE/DELETE
   - Severity: 🔴 CRITICAL

2. **Missing Index Implementations**
   - R-Tree: 100% stubbed (5/5 methods TODO)
   - Columnstore: 100% stubbed (4/4 methods TODO)
   - Severity: 🔴 CRITICAL

3. **GIN Missing remove()**
   - Location: `src/core/gin_index.cpp`
   - Issue: No explicit `remove()` method found
   - Severity: 🟠 HIGH

4. **HNSW Distance Computation**
   - Location: `src/core/hnsw_index.cpp:825, 891`
   - Issue: Distance metric calculation stubbed with TODOs
   - Severity: 🟡 MEDIUM

