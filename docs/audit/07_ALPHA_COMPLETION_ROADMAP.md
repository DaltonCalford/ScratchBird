# ALPHA Completion Roadmap - ScratchBird Database

**Date**: November 1, 2025
**Purpose**: Comprehensive roadmap to complete ALPHA release
**Status**: Based on Phases 1-4 comprehensive audit findings

---

## EXECUTIVE SUMMARY

The comprehensive code audit identified **CRITICAL architectural issues** that must be resolved before ALPHA release. While the codebase has excellent foundations (back-versioning, stable TIDs, TIP implementation), it suffers from **systematic PostgreSQL MVCC contamination** across all components.

**Overall Assessment**: **70% Complete** (needs 30% more work to reach ALPHA)

**Critical Blockers**:
1. ❌ MGA Compliance: 0/7 indexes compliant (150-220 hours)
2. ❌ TOAST MGA Integration: 17% compliant (30-40 hours)
3. ❌ SQL Identifier UTF-8: Critical bugs (10-15 hours)
4. ⚠️ HNSW/BRIN: Not implemented (140-200 hours if required)

---

## AUDIT SUMMARY

### Phase 1: MGA Compliance Audit
**Status**: ⚠️ **50% COMPLIANT**

**Compliant Components** (✅):
- TIP Implementation (100%)
- Back-Versioning (100%)
- Stable TIDs (100%)
- Transaction Markers (OIT/OAT/OST) (100%)

**Non-Compliant Components** (❌):
- TransactionManager API (0%) - Has Snapshot structure
- All 7 Index Types (0%) - Use snapshot-based visibility

**Details**: See `docs/audit/01_MGA_COMPLIANCE_AUDIT.md`

---

### Phase 2: TOAST Implementation Audit
**Status**: ⚠️ **17% MGA COMPLIANT**

**Working Features** (✅):
- Core TOAST operations (100%)
- Compression (LZ4) (100%)
- Chunk storage (100%)
- Heap integration (100%)

**Critical Issues** (❌):
- TOAST chunks lack on-disk xmin/xmax tracking
- Uses snapshot-based visibility instead of TIP
- No garbage collection for orphaned chunks
- All 7 indexes index TOAST pointers instead of values

**Details**: See `docs/audit/02_TOAST_IMPLEMENTATION_AUDIT.md`

---

### Phase 3: SQL Identifier Audit
**Status**: ⚠️ **CRITICAL BUGS FOUND**

**Compliant Components** (✅):
- Parser validates 128 UTF-8 characters (100%)
- UTF8Utils correctly counts characters (100%)
- In-memory structures unlimited (100%)

**Critical Bugs** (❌):
- Catalog persistence truncates by BYTES, not CHARACTERS
- Fixed 128-byte arrays can't store 128 multi-byte characters
- Data corruption risk for non-ASCII identifiers

**Details**: See `docs/audit/03_SQL_IDENTIFIER_AUDIT.md`

---

### Phase 4: Deferred Work Inventory
**Status**: **105 TODO/FIXME markers found**

**Breakdown**:
- CRITICAL (blocks ALPHA): 22 items
- HIGH (affects functionality): 29 items
- MEDIUM (performance): 22 items
- LOW (future enhancements): 32 items

**Total Estimated Effort**: 970-1,470 hours

**Details**: See `docs/audit/04_DEFERRED_WORK_INVENTORY.md`

---

## CRITICAL PATH TO ALPHA

### PHASE A: Foundation Fixes (CRITICAL - 190-275 hours)

Must be completed before ALPHA release.

#### A.1 MGA Compliance Restoration (150-220 hours)
**Priority**: 🔴 **CRITICAL**

**Tasks**:
1. Remove Snapshot structure from TransactionManager (20-30 hours)
   - Files: `include/scratchbird/core/transaction_manager.h`
   - Files: `src/core/transaction_manager.cpp`
   - Remove: `Snapshot` struct, `getSnapshot()`, `isSnapshotVisible()`
   - Add: `isVersionVisible(version_xid, reader_xid)` with TIP lookups

2. Fix B-tree visibility (30-40 hours)
   - Files: `src/core/btree.cpp`, `include/scratchbird/core/btree.h`
   - Replace: `Snapshot*` parameters → `TransactionId current_xid`
   - Replace: `isSnapshotVisible()` calls → TIP lookups

3. Fix remaining 6 index types (100-150 hours)
   - Hash Index (15-20 hours)
   - Bitmap Index (20-30 hours)
   - GIN Index (30-40 hours)
   - BRIN Index (15-20 hours)
   - HNSW Index (10-15 hours - if implemented)
   - R-tree Index (10-15 hours)

**Acceptance Criteria**:
- All indexes use `getTransactionState()` for TIP lookups
- No `Snapshot*` parameters in any index API
- All visibility checks use: `state == TX_COMMITTED && version_xid < current_xid`

---

#### A.2 TOAST Index Integration (30-40 hours)
**Priority**: 🔴 **CRITICAL**

**Tasks**:
1. Add TOAST detection to all index insert paths (20-30 hours)
   - Detect: `if (isToastPointer(key_data))`
   - Detoast: `toast_mgr->detoastValue(pointer, &actual_value, xmin)`
   - Index actual value instead of pointer
   - Apply to all 7 index types

2. Add TOAST MGA compliance (10-10 hours)
   - Redesign TOAST chunk format to include xmin/xmax
   - Implement TIP-based visibility for TOAST chunks

**Acceptance Criteria**:
- Indexes correctly index actual values, not TOAST pointers
- TOAST chunks use TIP-based visibility
- No snapshot-based TOAST visibility

---

#### A.3 SQL Identifier UTF-8 Fix (10-15 hours)
**Priority**: 🔴 **CRITICAL**

**Tasks**:
1. Fix catalog write truncation (5-8 hours)
   - Files: `src/core/catalog_manager.cpp` (lines 1661-1900)
   - Replace: `strncpy(dest, src, 127)` → `UTF8Utils::truncate(src, 127)`
   - Add validation before write

2. Increase catalog storage arrays (5-7 hours)
   - Change: `char name[128]` → `char name[512]`
   - Update all catalog record structs

**Acceptance Criteria**:
- 128 UTF-8 characters supported (up to 512 bytes)
- No mid-character truncation
- No data corruption for multi-byte identifiers

---

### PHASE B: TOAST Garbage Collection (HIGH - 30-40 hours)

**Priority**: 🟠 **HIGH**

**Tasks**:
1. Implement orphan TOAST chunk detection (15-20 hours)
2. Add TOAST table vacuum support (10-15 hours)
3. Implement crash recovery for TOAST (5-5 hours)

**Acceptance Criteria**:
- Garbage collector cleans orphaned TOAST chunks
- TOAST tables don't grow indefinitely
- Crash recovery handles partial TOAST deletions

---

### PHASE C: Advanced Index Implementation (HIGH - 140-200 hours if required)

**Priority**: 🟠 **HIGH** (Only if ALPHA requires full index support)

**Tasks**:
1. HNSW Index Implementation (80-120 hours)
   - Graph insertion algorithm
   - Approximate nearest neighbor search
   - Node deletion with link updates

2. BRIN Index Implementation (60-80 hours)
   - Range summary creation
   - Block-level filtering
   - Dead entry removal

**Decision Point**:
- ❓ **Can ALPHA ship without HNSW/BRIN?**
- If YES: Defer to post-ALPHA (saves 140-200 hours)
- If NO: Add to critical path

---

### PHASE D: Code Quality Improvements (MEDIUM - 60-90 hours)

**Priority**: 🟡 **MEDIUM**

**Tasks**:
1. Memory leak audit (20-30 hours)
   - RAII compliance check
   - Resource cleanup verification
   - Smart pointer usage audit

2. Exception safety (20-30 hours)
   - Verify all error paths clean up resources
   - Check transaction abort handling

3. Lock ordering verification (20-30 hours)
   - Deadlock prevention analysis
   - Lock acquisition consistency

**Acceptance Criteria**:
- Zero memory leaks under Valgrind
- All error paths properly clean up
- No deadlock potential

---

### PHASE E: Feature Parity Analysis (MEDIUM - 40-60 hours)

**Priority**: 🟡 **MEDIUM**

**Tasks**:
1. Type system comparison (10-15 hours)
   - Compare against MySQL/PostgreSQL/MSSQL/Firebird
   - Document missing types

2. SQL function inventory (15-20 hours)
   - List implemented vs missing functions
   - Prioritize critical functions

3. SQL syntax coverage (15-25 hours)
   - Document unsupported SQL features
   - Assess impact on ALPHA

**Deliverable**: Feature parity matrix showing gaps

---

## EFFORT BREAKDOWN

| Phase | Priority | Effort (hours) | Can Defer? |
|-------|----------|----------------|------------|
| **A. Foundation Fixes** | 🔴 CRITICAL | 190-275 | ❌ NO |
| A.1 MGA Compliance | 🔴 CRITICAL | 150-220 | ❌ NO |
| A.2 TOAST Index | 🔴 CRITICAL | 30-40 | ❌ NO |
| A.3 SQL Identifier UTF-8 | 🔴 CRITICAL | 10-15 | ❌ NO |
| **B. TOAST GC** | 🟠 HIGH | 30-40 | ⚠️ MAYBE |
| **C. Advanced Indexes** | 🟠 HIGH | 140-200 | ✅ YES |
| **D. Code Quality** | 🟡 MEDIUM | 60-90 | ✅ YES |
| **E. Feature Parity** | 🟡 MEDIUM | 40-60 | ✅ YES |
| **TOTAL (All)** | - | 460-665 | - |
| **TOTAL (Critical Only)** | - | 190-275 | - |

---

## ALPHA RELEASE SCENARIOS

### Scenario 1: Minimal ALPHA (Critical Only)
**Effort**: 190-275 hours (5-7 weeks)

**Includes**:
- ✅ MGA compliance (all indexes TIP-based)
- ✅ TOAST index integration
- ✅ SQL identifier UTF-8 fixes

**Excludes**:
- ❌ TOAST garbage collection (manual cleanup required)
- ❌ HNSW/BRIN indexes (defer to Beta)
- ❌ Full code quality audit
- ❌ Complete feature parity

**Risk**: MEDIUM
- TOAST orphans will accumulate (requires periodic manual cleanup)
- Limited to B-tree, Hash, Bitmap, GIN, R-tree indexes
- Some memory leaks may persist

---

### Scenario 2: Standard ALPHA (Critical + TOAST GC)
**Effort**: 220-315 hours (6-8 weeks)

**Includes**:
- ✅ All from Scenario 1
- ✅ TOAST garbage collection

**Excludes**:
- ❌ HNSW/BRIN indexes
- ❌ Full code quality audit
- ❌ Complete feature parity

**Risk**: LOW-MEDIUM
- Production-ready TOAST
- Solid index support (5 of 7 types)
- Memory leaks may persist

**Recommendation**: ⭐ **RECOMMENDED FOR ALPHA**

---

### Scenario 3: Complete ALPHA (All Phases)
**Effort**: 460-665 hours (12-17 weeks)

**Includes**:
- ✅ All from Scenario 2
- ✅ HNSW/BRIN indexes
- ✅ Full code quality audit
- ✅ Feature parity analysis

**Risk**: VERY LOW
- Enterprise-ready
- All 7 index types
- Zero memory leaks
- Known feature gaps documented

---

## RECOMMENDED APPROACH

### ⭐ **RECOMMENDATION: Scenario 2 (Standard ALPHA)**

**Timeline**: 6-8 weeks (220-315 hours)

**Rationale**:
1. Addresses all CRITICAL architectural issues
2. Provides production-ready TOAST
3. Delivers 5 solid index types (sufficient for most use cases)
4. Reasonable timeline
5. Low-medium risk

**Post-ALPHA Roadmap**:
- Beta 1: HNSW/BRIN indexes (140-200 hours)
- Beta 2: Code quality improvements (60-90 hours)
- Beta 3: Feature parity completion (40-60 hours)

---

## IMPLEMENTATION PLAN

### Week 1-2: MGA TransactionManager + B-tree
- Remove Snapshot structure
- Implement isVersionVisible() with TIP
- Fix B-tree visibility
- **Deliverable**: B-tree MGA compliant

### Week 3-4: Remaining Indexes (Hash, Bitmap, GIN, R-tree)
- Fix Hash index (15-20 hours)
- Fix Bitmap index (20-30 hours)
- Fix GIN index (30-40 hours)
- Fix R-tree index (10-15 hours)
- **Deliverable**: All basic indexes MGA compliant

### Week 5: TOAST Index Integration
- Add TOAST detection to all indexes
- Detoast before indexing
- Update TOAST chunk format with xmin/xmax
- **Deliverable**: Indexes handle TOAST correctly

### Week 6: SQL Identifier UTF-8 + TOAST GC
- Fix catalog truncation bugs
- Increase catalog storage arrays
- Implement TOAST garbage collection
- **Deliverable**: UTF-8 support + TOAST cleanup

### Week 7-8: Testing + Bug Fixes
- Comprehensive MGA testing
- TOAST stress testing
- UTF-8 identifier testing
- Fix discovered bugs
- **Deliverable**: ALPHA Release Candidate

---

## TESTING REQUIREMENTS

### MGA Compliance Tests
- [ ] All indexes use TIP lookups
- [ ] No Snapshot parameters in APIs
- [ ] Correct visibility semantics
- [ ] Transaction isolation levels work
- [ ] OIT/OAT/OST markers updated

### TOAST Tests
- [ ] Indexes handle TOASTed values correctly
- [ ] TOAST chunks use TIP visibility
- [ ] Garbage collector cleans orphans
- [ ] Multi-byte TOAST values work
- [ ] Compression working

### UTF-8 Identifier Tests
- [ ] 128-character ASCII identifiers work
- [ ] 128-character multi-byte identifiers work
- [ ] No truncation bugs
- [ ] No data corruption
- [ ] Round-trip integrity

### Integration Tests
- [ ] Concurrent transactions with TOAST
- [ ] Index builds on TOASTed columns
- [ ] UTF-8 table/column names
- [ ] MGA visibility under load
- [ ] Garbage collection under load

---

## SUCCESS CRITERIA FOR ALPHA

### Functional Requirements
- ✅ All 5 basic index types fully functional
- ✅ MGA compliance (TIP-based visibility)
- ✅ TOAST working with GC
- ✅ 128 UTF-8 character identifiers
- ✅ PostgreSQL wire protocol support
- ✅ Basic SQL query support

### Quality Requirements
- ✅ Zero CRITICAL bugs
- ✅ Zero architectural violations
- ✅ Comprehensive test coverage (>80%)
- ✅ Documentation complete
- ✅ Performance acceptable (within 2x PostgreSQL)

### Non-Functional Requirements
- ✅ Embedded database (no networking for ALPHA)
- ✅ Single-user mode working
- ✅ Multi-user mode with basic concurrency
- ✅ Crash recovery working
- ✅ Basic performance tuning

---

## RISK ASSESSMENT

### HIGH RISKS
1. **MGA compliance fix scope**: May uncover more issues (Mitigation: Early testing)
2. **TOAST index integration complexity**: May affect performance (Mitigation: Benchmarking)
3. **UTF-8 identifier migration**: Existing databases may need conversion (Mitigation: Migration tool)

### MEDIUM RISKS
1. **TOAST GC implementation**: May miss edge cases (Mitigation: Stress testing)
2. **Testing timeline**: May need more time (Mitigation: Parallel testing)
3. **Performance regression**: Fixes may slow queries (Mitigation: Profiling)

### LOW RISKS
1. **Documentation gaps**: Can be filled post-fix (Mitigation: Inline comments)
2. **Tool integration**: External tools may need updates (Mitigation: Version 2)

---

## CONCLUSION

ScratchBird is **70% complete** for ALPHA release. The codebase has excellent foundations (back-versioning, stable TIDs, TIP) but needs **critical architectural corrections** to achieve pure Firebird MGA compliance.

**Critical Path**: 190-275 hours (MGA + TOAST + UTF-8)
**Recommended Path**: 220-315 hours (includes TOAST GC)
**Timeline**: 6-8 weeks

**Next Steps**:
1. Approve Scenario 2 (Standard ALPHA) approach
2. Begin Week 1-2: MGA TransactionManager + B-tree fixes
3. Establish testing infrastructure
4. Weekly progress reviews

**Target ALPHA Release**: 6-8 weeks from start date

---

**Document Date**: November 1, 2025
**Status**: Comprehensive Audit Complete
**Next Action**: Approve roadmap and begin implementation
