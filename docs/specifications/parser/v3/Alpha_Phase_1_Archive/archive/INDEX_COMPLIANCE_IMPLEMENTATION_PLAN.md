# Index System 100% Compliance Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: November 20, 2025
**Goal**: Achieve 100% compliance for all 11 index types
**Status**: In Progress

---

## Executive Summary

Based on the INDEX_SYSTEM_AUDIT_REPORT.md findings, this document outlines the specific implementation tasks required to reach 100% compliance for all 11 index types in ScratchBird.

**Current Status**: 8/11 fully production-ready, 3/11 need work
**Target**: 11/11 production-ready with full bytecode support
**Estimated Effort**: 12-16 hours

---

## Phase 1: Critical Fixes (P0) - 4-6 hours

### Issue 1.1: R-Tree Stub Wrapper (rtree_index.cpp)

**Problem**: The executor routes to `RTreeIndex` (stub with 10 TODOs) instead of `RTree` (full 1,168-line implementation).

**Root Cause**: Architectural mismatch - two separate implementations exist:
- `rtree.cpp` (real implementation, 80% complete)
- `rtree_index.cpp` (stub wrapper, 10% complete)

**Solution**: Make `RTreeIndex` a thin wrapper that delegates to `RTree`

**Implementation Steps**:
1. ✅ Add `RTree*` member to `RTreeIndex` class
2. ✅ Update constructor to create `RTree` instance
3. ✅ Delegate insert() to rtree_->insert()
4. ✅ Delegate search() to rtree_->search()
5. ✅ Delegate remove() to rtree_->remove()
6. ✅ Delegate removeDeadEntries() to rtree_->removeDeadEntries()
7. ✅ Convert between serialized key (vector<uint8_t>) and BoundingBox struct
8. ✅ Remove all 10 TODO stubs

**Files Modified**:
- `include/scratchbird/core/rtree_index.h` (add RTree* member)
- `src/core/rtree_index.cpp` (implement delegation)

**Validation**:
- Compile successfully
- R-Tree operations route to real implementation
- All TODO markers removed

**Estimated Time**: 2 hours

---

### Issue 1.2: Bytecode Support for GIN Index

**Problem**: `executor.cpp` lines 20362-20368 return `NOT_IMPLEMENTED` for GIN

**Root Cause**: Code comment says "These require special handling due to different APIs" but GinIndex DOES have standard insert/search/remove methods

**Solution**: Wire up GIN bytecode paths in executor

**Implementation Steps**:
1. ✅ Verify GinIndex has insert(value_data, value_len, tid, xmin)
2. ✅ Verify GinIndex has remove(value_data, value_len, tid, xmax)
3. ✅ Verify GinIndex has search/find methods
4. ✅ Add GIN case to executor INSERT_INDEX opcode handler
5. ✅ Add GIN case to executor INDEX_SEARCH opcode handler
6. ✅ Add GIN case to executor DELETE_INDEX opcode handler
7. ✅ Add GIN case to executor RANGE_SCAN opcode handler (return NOT_SUPPORTED for range scans)

**Code Location**: `src/sblr/executor.cpp:20362-20368, 20480-20486, 20594-20600, 20789-20795`

**API Mapping**:
```cpp
// GIN has:
Status insert(const void* value_data, size_t value_len, const TID& tid, uint64_t xmin, ErrorContext* ctx);
Status remove(const void* value_data, size_t value_len, const TID& tid, uint64_t xmax, ErrorContext* ctx);
Status removeDeadEntries(const std::vector<TID>& dead_tids, ...);

// Need to convert from:
const std::vector<uint8_t>& key → const void* value_data, size_t value_len
```

**Validation**:
- GIN INSERT via bytecode works
- GIN DELETE via bytecode works
- GIN removeDeadEntries works

**Estimated Time**: 1.5 hours

---

### Issue 1.3: Bytecode Support for HNSW Index

**Problem**: `executor.cpp` lines 20363-20368 return `NOT_IMPLEMENTED` for HNSW

**Root Cause**: Same as GIN - code assumes different API but HnswIndex has standard methods

**Solution**: Wire up HNSW bytecode paths in executor

**Implementation Steps**:
1. ✅ Verify HnswIndex has insert(vector, tid, xmin, xmax)
2. ✅ Verify HnswIndex has remove(tid, xmax)
3. ✅ Verify HnswIndex has search(query_vector, k, current_xid, results)
4. ✅ Add HNSW case to executor INSERT_INDEX opcode handler
5. ✅ Add HNSW case to executor INDEX_SEARCH opcode handler (vector search)
6. ✅ Add HNSW case to executor DELETE_INDEX opcode handler
7. ✅ Add HNSW case to executor RANGE_SCAN opcode handler (return NOT_SUPPORTED)

**Code Location**: `src/sblr/executor.cpp:20363-20368, 20481-20486, 20595-20600, 20790-20795`

**API Mapping**:
```cpp
// HNSW has:
Status insert(const VectorValue& vector, const TID& tid, uint64_t xmin, uint64_t xmax, ErrorContext* ctx);
Status remove(const TID& tid, uint64_t xmax, ErrorContext* ctx);
Status search(const VectorValue& query_vector, uint32_t k, uint64_t current_xid, std::vector<TID>* results, ErrorContext* ctx);

// Need to parse vector from key:
const std::vector<uint8_t>& key → VectorValue (float[] or double[])
```

**Validation**:
- HNSW INSERT via bytecode works
- HNSW vector search via bytecode works
- HNSW DELETE via bytecode works

**Estimated Time**: 1.5 hours

---

### Issue 1.4: Bytecode Support for Columnstore Index

**Problem**: `executor.cpp` lines 20364-20368 return `NOT_IMPLEMENTED` for Columnstore

**Root Cause**: Columnstore has a different API focused on column operations, not row-level insert/delete

**Solution**: Wire up Columnstore bytecode paths OR document why generic bytecode doesn't apply

**Implementation Steps**:
1. ✅ Review Columnstore API (insertColumn, removeDeadEntries)
2. ✅ Determine if generic INSERT/DELETE/SEARCH bytecode makes sense for columnar storage
3. ✅ Either:
   - Option A: Implement generic ops as batch column operations
   - Option B: Document that Columnstore requires specialized bytecode (bulk load)
   - **RECOMMENDED: Option B** - Columnstore is for analytics, not OLTP row-by-row ops

**Code Location**: `src/sblr/executor.cpp:20364-20368, 20482-20486, 20596-20600, 20791-20795`

**Decision**: If Columnstore is designed for bulk analytics loads, generic row-level bytecode may not apply. Document this clearly.

**Validation**:
- Either: Columnstore operations work via bytecode
- Or: Documentation explains why Columnstore uses specialized bulk load bytecode

**Estimated Time**: 1 hour

---

## Phase 2: High Priority Fixes (P1) - 4-6 hours

### Issue 2.1: Complete Minor TODOs

**GIN Index - Line 616**: "Implement in Phase 4"
- **Context**: Likely a minor optimization or feature
- **Action**: Review code, implement or remove TODO
- **Estimated Time**: 30 minutes

**HNSW Index - Line 1460**: "Implement more sophisticated heuristic"
- **Context**: Optimization for neighbor selection
- **Action**: Review code, implement simple heuristic or document future work
- **Estimated Time**: 30 minutes

**LSM-Tree - Line 710**: "Add proper logging"
- **Context**: Missing log statement
- **Action**: Add appropriate LOG_DEBUG/LOG_INFO statement
- **Estimated Time**: 15 minutes

**Total Estimated Time**: 1.5 hours

---

### Issue 2.2: Add Missing Test Coverage

**Problem**: No dedicated tests for:
- R-Tree
- Columnstore
- LSM-Tree
- GiST
- SP-GiST

**Solution**: Create integration tests for each index type

**Test Requirements** (per MGA_RULES.md and INDEX_GC_PROTOCOL.md):
1. ✅ Insert entries with xmin
2. ✅ Search with current_xid (TIP-based visibility)
3. ✅ Remove entries with xmax (logical deletion)
4. ✅ Verify MGA compliance (no snapshots, uses TIP)
5. ✅ Test removeDeadEntries (GC protocol)
6. ✅ Verify stable TIDs
7. ✅ Test concurrent operations (if index supports)

**Test Files to Create**:
- `tests/integration/test_rtree_index.cpp` (80 lines)
- `tests/integration/test_columnstore_index.cpp` (80 lines)
- `tests/integration/test_lsm_tree_index.cpp` (80 lines)
- `tests/integration/test_gist_index.cpp` (80 lines)
- `tests/integration/test_spgist_index.cpp` (80 lines)

**Template**:
```cpp
TEST(RTreeIndexTest, InsertAndSearch) {
    // Create index
    // Insert with xmin=100
    // Search with current_xid=101 (should see)
    // Search with current_xid=99 (should NOT see)
}

TEST(RTreeIndexTest, LogicalDelete) {
    // Insert entry
    // Remove with xmax=102
    // Search with current_xid=103 (should NOT see)
}

TEST(RTreeIndexTest, GarbageCollection) {
    // Insert and delete entries
    // Call removeDeadEntries()
    // Verify entries removed
}

TEST(RTreeIndexTest, MGACompliance) {
    // Verify no Snapshot* parameters
    // Verify uses TransactionId parameters
    // Verify calls getTransactionState()
}
```

**Estimated Time**: 3 hours (40 minutes per index × 5 indexes)

---

## Phase 3: Medium Priority (P2) - 2-4 hours

### Issue 3.1: LSM-Tree Maturity

**Current Status**: 70% ready
- ✅ PUT/GET/REMOVE implemented
- ✅ Memtable + SSTable architecture
- ✅ Compaction logic
- ✅ MGA compliance (xmin/xmax)
- ⚠️ One minor TODO (line 710: logging)
- ⚠️ Less mature than other indexes

**Goal**: Bring to 100% production readiness

**Tasks**:
1. ✅ Complete logging TODO (see Issue 2.1)
2. ✅ Review compaction strategy (ensure optimal)
3. ✅ Add performance tests (compaction under load)
4. ✅ Verify MGA compliance in all code paths
5. ✅ Document LSM-specific tuning parameters
6. ✅ Add integration tests (see Issue 2.2)

**Estimated Time**: 2 hours

---

## Phase 4: Documentation Updates - 1-2 hours

### Issue 4.1: Update INDEX_SYSTEM_AUDIT_REPORT.md

**Updates Needed**:
1. ✅ Change "8/11 production-ready" → "11/11 production-ready"
2. ✅ Change "8/11 bytecode support" → "11/11 bytecode support" (or 10/11 if Columnstore exempt)
3. ✅ Update R-Tree status from "10-80%" → "100%"
4. ✅ Update LSM-Tree status from "70%" → "100%"
5. ✅ Update bytecode section to remove NOT_IMPLEMENTED items
6. ✅ Update test coverage section to show 11/11 have tests
7. ✅ Update **Conclusion** section with new accurate statement
8. ✅ Change overall grade from "B+ (87%)" → "A (100%)"

**Estimated Time**: 1 hour

---

## Implementation Timeline

| Phase | Tasks | Priority | Estimated Time | Assignee |
|-------|-------|----------|----------------|----------|
| **Phase 1** | R-Tree wrapper fix | P0 | 2 hours | Claude |
| **Phase 1** | GIN bytecode | P0 | 1.5 hours | Claude |
| **Phase 1** | HNSW bytecode | P0 | 1.5 hours | Claude |
| **Phase 1** | Columnstore bytecode/doc | P0 | 1 hour | Claude |
| **Phase 2** | Complete minor TODOs | P1 | 1.5 hours | Claude |
| **Phase 2** | Add test coverage | P1 | 3 hours | Claude |
| **Phase 3** | LSM-Tree maturity | P2 | 2 hours | Claude |
| **Phase 4** | Update audit report | Documentation | 1 hour | Claude |
| **TOTAL** | | | **13.5 hours** | |

---

## Success Criteria

### Technical Criteria

1. ✅ **R-Tree**: All TODOs removed, delegates to rtree.cpp
2. ✅ **Bytecode**: GIN, HNSW, Columnstore either work or are documented as requiring specialized ops
3. ✅ **TODOs**: All minor TODOs completed or removed
4. ✅ **Tests**: 11/11 indexes have dedicated integration tests
5. ✅ **LSM-Tree**: Reaches 100% maturity
6. ✅ **MGA Compliance**: All indexes maintain 100% compliance (zero snapshot contamination)

### Documentation Criteria

1. ✅ **Audit Report**: Updated with accurate "11/11 production-ready" claim
2. ✅ **Bytecode Section**: Accurately reflects which indexes support generic bytecode
3. ✅ **Test Coverage**: Shows 11/11 indexes have tests
4. ✅ **Overall Grade**: Updated to reflect 100% compliance

### Validation Criteria

1. ✅ **Build**: All code compiles without errors
2. ✅ **Tests**: All new tests pass
3. ✅ **Existing Tests**: No regressions in existing tests
4. ✅ **Code Review**: All changes follow MGA_RULES.md
5. ✅ **Performance**: No degradation in index performance

---

## Risks and Mitigation

### Risk 1: Columnstore Bytecode Mismatch

**Risk**: Columnstore API doesn't fit row-level insert/delete paradigm
**Impact**: HIGH - May not be able to implement generic bytecode
**Mitigation**: Document that Columnstore requires specialized bulk load bytecode
**Fallback**: Mark Columnstore as "10/11 generic bytecode" with clear documentation

### Risk 2: Test Writing Time Overrun

**Risk**: Creating 5 new test files may take longer than estimated
**Impact**: MEDIUM - Delays Phase 2 completion
**Mitigation**: Use test template, copy-paste-modify pattern
**Fallback**: Prioritize R-Tree and LSM-Tree tests, defer GiST/SP-GiST

### Risk 3: R-Tree Delegation Issues

**Risk**: BoundingBox conversion may have edge cases
**Impact**: MEDIUM - R-Tree operations may fail at runtime
**Mitigation**: Thorough testing of serialization/deserialization
**Fallback**: Complete rtree_index.cpp stubs instead of delegation

---

## Appendix A: Code Locations

### R-Tree Files
- `include/scratchbird/core/rtree.h` (full implementation header)
- `src/core/rtree.cpp` (full implementation, 1,168 lines)
- `include/scratchbird/core/rtree_index.h` (wrapper header)
- `src/core/rtree_index.cpp` (wrapper stub, 409 lines)

### Executor Bytecode
- `src/sblr/executor.cpp:20362-20368` (INSERT_INDEX - GIN/HNSW/Columnstore)
- `src/sblr/executor.cpp:20480-20486` (INDEX_SEARCH - GIN/HNSW/Columnstore)
- `src/sblr/executor.cpp:20594-20600` (DELETE_INDEX - GIN/HNSW/Columnstore)
- `src/sblr/executor.cpp:20789-20795` (RANGE_SCAN - GIN/HNSW/Columnstore)

### Index Implementations
- `src/core/gin_index.cpp` (4,432 lines)
- `src/core/hnsw_index.cpp` (1,771 lines)
- `src/core/columnstore_index.cpp` (391 lines)
- `src/core/lsm_tree_index.cpp` (848 lines)
- `src/core/gist_index.cpp` (1,340 lines)
- `src/core/spgist_index.cpp` (1,379 lines)

### Test Files
- `tests/integration/test_index_mga_compliance.cpp` (existing)
- `tests/integration/test_hash_index.cpp` (existing)
- `tests/integration/test_bitmap_index_gc.cpp` (existing)
- `tests/integration/test_gin_index_gc.cpp` (existing)
- `tests/integration/test_brin_index.cpp` (existing)
- `tests/integration/test_hnsw_index.cpp` (existing)

---

**Document Status**: ACTIVE
**Last Updated**: November 20, 2025
**Next Review**: After Phase 1 completion
