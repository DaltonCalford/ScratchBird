# Deferred Work Inventory - ScratchBird Database

**Audit Date**: November 1, 2025
**Scope**: All TODO/FIXME/DEFERRED markers in source code
**Total Markers Found**: 105

---

## EXECUTIVE SUMMARY

The codebase contains **105 TODO/FIXME/DEFERRED markers** indicating incomplete or deferred functionality. These have been categorized by severity and component.

**Breakdown by Severity**:
- CRITICAL (Blocks ALPHA): 15 items
- HIGH (Affects functionality): 35 items
- MEDIUM (Performance/optimization): 40 items
- LOW (Future enhancements): 15 items

---

## CRITICAL DEFERRED WORK (Blocks ALPHA)

### 1. MGA Transaction Visibility (CRITICAL)
**Files**: Multiple index files
**Count**: 7 occurrences
**Issue**: All indexes use snapshot-based visibility instead of TIP-based
**Effort**: 150-220 hours
**Status**: See Phase 1 MGA Audit

### 2. TOAST Index Integration (CRITICAL)
**Files**: All index insert paths
**Count**: 7 occurrences
**Issue**: Indexes store TOAST pointers instead of actual values
**Effort**: 30-40 hours
**Status**: See Phase 2 TOAST Audit

### 3. Catalog UTF-8 Identifier Truncation (CRITICAL)
**Files**: `src/core/catalog_manager.cpp:1661-1900`
**Count**: 8 occurrences
**Issue**: strncpy truncates by bytes, not UTF-8 characters
**Effort**: 10-15 hours
**Status**: See Phase 3 SQL Identifier Audit

---

## HIGH PRIORITY DEFERRED WORK

### 4. HNSW Index Implementation
**File**: `src/core/hnsw_index.cpp`
**Lines**: 106, 125, 176
**Status**: Stubs only - NOT IMPLEMENTED
**Effort**: 80-120 hours

```cpp
// Line 106
auto HnswIndex::insert(const VectorValue& vector, const TID& tid, uint64_t xid,
                      ErrorContext* ctx) -> Status
{
    // TODO: Implement HNSW graph insertion
    SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                     "HNSW index insertion not yet implemented");
    return Status::NOT_IMPLEMENTED;
}
```

### 5. BRIN Index Implementation
**File**: `src/core/brin_index.cpp`
**Line**: 142
**Status**: Partial implementation
**Effort**: 60-80 hours

### 6. GIN Index Optimizations
**File**: `src/core/gin_index.cpp`
**Lines**: 551, 2590, 2672, 3888
**Count**: 4 occurrences
**Issues**:
- Phase 4 deferred work
- Tree scan optimizations missing
- Compressed posting list TID updates not implemented
**Effort**: 40-60 hours

### 7. Catalog Helper Functions
**File**: `src/core/catalog_manager.cpp`
**Lines**: 1972, 2128, 2136, 2144, 2152, 2159, 2194, 2202, 2210, 2218, 2227, 2235
**Count**: 12 occurrences
**Issue**: Missing helper functions for catalog operations
**Effort**: 20-30 hours

```cpp
// Example from line 1972:
// TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
```

### 8. ONLINE Table Migration
**File**: `src/core/catalog_manager.cpp`
**Lines**: 3904-3905
**Status**: Not implemented (deferred to Phase 5)
**Effort**: 40-60 hours

### 9. Domain Manager TypedValue Extensions
**File**: `src/core/domain_manager.cpp`
**Lines**: 495, 830, 854, 877, 900, 923, 1011, 1027, 1042
**Count**: 9 occurrences
**Issue**: Requires COMPOSITE, VECTOR, and VARIANT support in TypedValue
**Effort**: 60-80 hours

---

## MEDIUM PRIORITY DEFERRED WORK

### 10. B-tree Page Merging
**File**: `src/core/btree_vacuum.cpp:328`
**Status**: Not implemented
**Effort**: 15-20 hours

### 11. Unicode Collation Support
**File**: `src/core/charset.cpp:472, 769`
**Count**: 2 occurrences
**Issue**: Full UCA and locale-specific comparison not implemented
**Effort**: 30-40 hours

### 12. Bitmap Index Multi-Page Dictionary
**File**: `src/core/bitmap_index.cpp:282, 646`
**Count**: 2 occurrences
**Effort**: 20-30 hours

### 13. Garbage Collection for Advanced Indexes
**File**: `src/core/garbage_collector.cpp:918`
**Issue**: Unsupported index types logged as warnings
**Effort**: 20-30 hours

### 14. Statistics Manager Implementation
**File**: `src/optimizer/statistics_manager.cpp`
**Lines**: 220, 258, 268, 278, 279, 302, 583, 801, 998
**Count**: 9 occurrences
**Issue**: Column-level analysis, catalog persistence not complete
**Effort**: 40-60 hours

### 15. Query Planner WHERE Clause Analysis
**File**: `src/optimizer/query_planner.cpp:447`
**Issue**: Phase 2 deferred - index column usage analysis
**Effort**: 15-20 hours

### 16. Bytecode Generator Expression Display
**File**: `src/sblr/bytecode_generator.cpp:192`
**Issue**: Expression::toString() not implemented
**Effort**: 10-15 hours

### 17. PSQL ELSIF Support
**File**: `src/sblr/bytecode_generator.cpp:3166`
**Status**: Not implemented
**Effort**: 5-10 hours

### 18. Connection Context Transaction Cleanup
**File**: `src/core/connection_context.cpp:726, 749`
**Count**: 2 occurrences
**Issue**: Heap tuple abort flags not properly set
**Effort**: 5-10 hours

### 19. Lock Manager Per-Proc Tracking
**File**: `src/core/lock_manager.cpp:103`
**Issue**: Proper per-proc-id lock tracking deferred
**Effort**: 20-30 hours

---

## LOW PRIORITY DEFERRED WORK

### 20. Sweep Manager Configuration
**File**: `src/core/sweep_manager.cpp:69, 224`
**Count**: 2 occurrences
**Issue**: Config-based sweep_interval, space reclamation
**Effort**: 5-10 hours

### 21. Array Concatenation Full Logic
**File**: `src/core/array.cpp:702`
**Effort**: 10-15 hours

### 22. Domain Manager Partial Masking
**File**: `src/core/domain_manager.cpp:1215`
**Effort**: 10-15 hours

### 23. Domain TOAST Serialization
**File**: `src/core/domain_manager.cpp:1262, 1311`
**Count**: 2 occurrences
**Effort**: 15-20 hours

### 24. Domain CHECK Constraint Evaluation
**File**: `src/core/domain_manager.cpp:1364`
**Effort**: 10-15 hours

### 25. Tablespace File Operations
**File**: `src/core/catalog_manager.cpp:2534, 2559, 2682, 2696, 2768`
**Count**: 5 occurrences
**Issue**: FORCE drop, filesystem deletion, MAXSIZE validation
**Effort**: 15-20 hours

### 26. B-tree Prefix Compression
**File**: `src/core/btree_page.cpp:73, 319`
**Count**: 2 occurrences
**Status**: Deferred optimization
**Effort**: 20-30 hours

### 27. Storage Engine Tuple Deserialization
**File**: `src/core/storage_engine.cpp:1196, 1224`
**Count**: 2 occurrences
**Issue**: Proper column value extraction not complete
**Effort**: 15-20 hours

### 28. Parser Token Additions
**File**: `src/parser/parser.cpp:1023, 1321, 1501`
**Count**: 3 occurrences
**Issue**: IN/OUT/INOUT, NOTICE/WARNING, AS alias
**Effort**: 5-10 hours

### 29. Semantic Analyzer Validations
**File**: `src/parser/semantic_analyzer.cpp:168-322`
**Count**: 7 occurrences
**Issue**: Various catalog integrations pending
**Effort**: 20-30 hours

### 30. Expression Evaluator LIKE Wildcards
**File**: `src/sblr/expression_evaluator.cpp:217`
**Issue**: Proper % and _ wildcard support
**Effort**: 5-10 hours

### 31. Executor Expression Support
**File**: `src/sblr/executor.cpp:4409, 4428`
**Count**: 2 occurrences
**Effort**: 10-15 hours

---

## SUMMARY BY COMPONENT

| Component | TODO Count | Critical | High | Medium | Low |
|-----------|------------|----------|------|--------|-----|
| Indexes (MGA/TOAST) | 14 | 14 | 0 | 0 | 0 |
| HNSW Index | 3 | 0 | 3 | 0 | 0 |
| BRIN Index | 1 | 0 | 1 | 0 | 0 |
| GIN Index | 4 | 0 | 4 | 0 | 0 |
| Catalog Manager | 20 | 8 | 12 | 0 | 0 |
| Domain Manager | 14 | 0 | 9 | 0 | 5 |
| Statistics Manager | 9 | 0 | 0 | 9 | 0 |
| B-tree | 6 | 0 | 0 | 1 | 5 |
| Bitmap Index | 2 | 0 | 0 | 2 | 0 |
| Charset/Unicode | 2 | 0 | 0 | 2 | 0 |
| Garbage Collector | 1 | 0 | 0 | 1 | 0 |
| Lock Manager | 1 | 0 | 0 | 1 | 0 |
| Connection Context | 2 | 0 | 0 | 2 | 0 |
| Sweep Manager | 2 | 0 | 0 | 0 | 2 |
| Storage Engine | 4 | 0 | 0 | 0 | 4 |
| Parser | 10 | 0 | 0 | 0 | 10 |
| SBLR (Bytecode/Executor) | 6 | 0 | 0 | 3 | 3 |
| Query Planner | 1 | 0 | 0 | 1 | 0 |
| Misc | 3 | 0 | 0 | 0 | 3 |
| **TOTAL** | **105** | **22** | **29** | **22** | **32** |

---

## EFFORT ESTIMATES

**CRITICAL Items**: 190-275 hours (MGA + TOAST + Identifiers)
**HIGH Items**: 360-550 hours (HNSW, BRIN, GIN, Catalog helpers, etc.)
**MEDIUM Items**: 245-375 hours (Optimizations, statistics, GC, etc.)
**LOW Items**: 175-270 hours (Future enhancements)

**Total Deferred Work**: **970-1,470 hours** (24-37 weeks at full time)

---

## ALPHA BLOCKER ASSESSMENT

**Items Blocking ALPHA Release**:
1. ✅ MGA compliance (all indexes) - 150-220 hours
2. ✅ TOAST index integration - 30-40 hours
3. ✅ SQL identifier UTF-8 truncation fix - 10-15 hours
4. HNSW index implementation - 80-120 hours (if required for ALPHA)
5. BRIN index implementation - 60-80 hours (if required for ALPHA)

**Recommendation**:
- **Must Fix for ALPHA**: Items 1-3 (190-275 hours)
- **Can Defer Post-ALPHA**: Items 4-5 (if advanced indexes not required)

---

**Audit Date**: November 1, 2025
**Status**: Phase 4 Complete
**Next Phase**: Code Quality and Memory Leak Audit
