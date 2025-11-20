# INDEX SYSTEM REMEDIATION PLAN
**Date:** November 20, 2025
**Based on:** INDEX_SYSTEM_COMPREHENSIVE_AUDIT.md
**Status:** Action Plan for Full Production Readiness

---

## EXECUTIVE SUMMARY

**Current State:** 8 of 11 indexes are 90%+ complete, but only 3 have active DML integration, and 2 have MGA violations.

**Target State:** All 11 indexes fully MGA-compliant, DML-integrated, and bytecode-enabled.

**Estimated Effort:** ~120-160 hours (3-4 weeks with 1 developer)

---

## PRIORITY 1: CRITICAL MGA VIOLATIONS (16-24 hours)

### Issue 1.1: GIN Index MGA Violation
**File:** `src/core/gin_index.cpp:241`
**Problem:** Physical TID removal instead of xmax-based logical deletion
**Impact:** Violates Firebird MGA principles

**Tasks:**
- [ ] Replace physical removal with xmax tombstones in posting lists (8h)
- [ ] Add xmin/xmax to PostingListEntry structure (2h)
- [ ] Implement visibility checks for posting list entries (4h)
- [ ] Update compaction to respect MGA visibility (4h)
- [ ] Add integration tests for GIN logical deletion (2h)

**Affected Code:**
- Lines 196-257: remove() function
- Lines 824-915: Posting list structure
- Lines 1116-1300+: Posting tree operations

---

### Issue 1.2: Bitmap Index Incomplete + MGA Issues
**File:** `src/core/bitmap_index.cpp`
**Problem:** Stubbed insert/remove, no xmin/xmax in bitmap entries
**Impact:** 30% complete, not production-ready

**Tasks:**
- [ ] Design bitmap entry structure with xmin/xmax (2h)
- [ ] Implement insert operation (6h)
- [ ] Implement remove operation (logical deletion) (4h)
- [ ] Add visibility checks to bitmap entries (4h)
- [ ] Update AND/OR operations to handle visibility (4h)
- [ ] Add integration tests (4h)

**Affected Code:**
- Lines 542-630: Search/construction
- Lines 186-301: Bitmap operations
- Missing: insert/remove implementations

---

## PRIORITY 2: DML INTEGRATION (48-64 hours)

### Issue 2.1: Enable DML Hooks for 8 Missing Indexes
**File:** `src/core/storage_engine.cpp:74-85`
**Problem:** Returns `Status::NOT_IMPLEMENTED` for 8 indexes
**Impact:** Indexes not maintained during INSERT/UPDATE/DELETE

**Affected Indexes:**
1. GIN (after fixing MGA violation)
2. HNSW
3. GiST
4. SP-GiST
5. BRIN
6. R-Tree
7. Columnstore
8. Bitmap (after completing implementation)

**Tasks per Index (6-8h each = 48-64h total):**
- [ ] GIN DML Integration (8h)
  - Implement key extraction during INSERT
  - Handle array/composite type indexing
  - Update posting lists on UPDATE
  - Mark entries with xmax on DELETE

- [ ] HNSW DML Integration (8h)
  - Vector insertion with graph updates
  - Layer rebalancing on INSERT
  - Neighbor link updates
  - Logical deletion on DELETE

- [ ] GiST DML Integration (6h)
  - Predicate-based insertion
  - Tree rebalancing
  - Logical deletion with xmax

- [ ] SP-GiST DML Integration (6h)
  - Space-partitioning insertion
  - Quadrant updates
  - Logical deletion

- [ ] BRIN DML Integration (6h)
  - Block range summary updates
  - Min/max recalculation on INSERT/UPDATE
  - Range invalidation on DELETE

- [ ] R-Tree DML Integration (6h)
  - MBR calculation and insertion
  - Spatial index updates
  - Logical deletion

- [ ] Columnstore DML Integration (8h)
  - Columnar append on INSERT
  - Segment updates (append-only)
  - Logical deletion with xmax

- [ ] Bitmap DML Integration (6h)
  - Bitmap updates on INSERT/UPDATE
  - Bitmap invalidation on DELETE

**Common Code Changes:**
- Remove `Status::NOT_IMPLEMENTED` returns
- Add proper index type dispatch
- Integrate with transaction manager for xmin/xmax

---

## PRIORITY 3: BYTECODE SUPPORT (40-48 hours)

### Issue 3.1: No Bytecode/VM Integration
**Files:** `src/sblr/opcodes.h`, `src/sblr/bytecode_generator.cpp`, `src/sblr/executor.cpp`
**Problem:** Index operations not accessible via SBLR bytecode
**Impact:** Cannot use indexes from SQL execution

**Tasks:**

#### Phase 3.1: Bytecode Opcodes (8h)
- [ ] Define index operation opcodes (2h)
  - CREATE INDEX (already exists, verify completeness)
  - DROP INDEX (already exists, verify completeness)
  - INDEX_SEARCH
  - INDEX_SCAN
  - INDEX_INSERT (internal)
  - INDEX_UPDATE (internal)
  - INDEX_DELETE (internal)

- [ ] Add opcode parameters (2h)
  - Index type enum encoding
  - Key/value serialization
  - TID range encoding
  - Predicate encoding

- [ ] Update opcodes.h documentation (1h)
- [ ] Add opcode validation (1h)
- [ ] Integration tests for opcode encoding (2h)

#### Phase 3.2: Bytecode Generation (12h)
- [ ] Generate CREATE INDEX bytecode (2h)
  - Index type selection
  - Column list encoding
  - Index options (UNIQUE, predicate, etc.)

- [ ] Generate DROP INDEX bytecode (1h)
  - IF EXISTS handling
  - CASCADE/RESTRICT support

- [ ] Generate index hints for SELECT (4h)
  - Index selection logic
  - Cost-based optimization
  - USING INDEX clause

- [ ] Generate index maintenance for DML (4h)
  - INSERT → INDEX_INSERT calls
  - UPDATE → INDEX_UPDATE calls
  - DELETE → INDEX_DELETE calls

- [ ] Add bytecode generation tests (1h)

#### Phase 3.3: Bytecode Execution (16h)
- [ ] Implement executeCreateIndex (4h)
  - Index factory integration
  - Catalog registration
  - Initial index build

- [ ] Implement executeDropIndex (2h)
  - Catalog deregistration
  - Index file cleanup

- [ ] Implement executeIndexSearch (4h)
  - Index type dispatch
  - Key extraction
  - TID collection

- [ ] Implement executeIndexScan (4h)
  - Range scan support
  - Iterator management
  - Visibility filtering

- [ ] Add executor integration tests (2h)

#### Phase 3.4: Query Planner Integration (12h)
- [ ] Index selection in query planner (6h)
  - Cost estimation
  - Index type suitability
  - Multi-index support

- [ ] Index scan plan nodes (3h)
  - IndexScan plan type
  - BitmapIndexScan plan type

- [ ] Index-only scans (2h)
  - Covering index detection
  - Visibility map integration

- [ ] Add planner tests (1h)

---

## PRIORITY 4: ADDITIONAL IMPROVEMENTS (16-24 hours)

### Issue 4.1: R-Tree Wrapper Audit
**File:** `src/core/rtree_index.cpp` (wrapper) + `src/core/rtree.cpp` (implementation)
**Problem:** Wrapper delegates to rtree.cpp, which wasn't fully audited
**Impact:** Unknown MGA compliance of underlying implementation

**Tasks:**
- [ ] Audit rtree.cpp for MGA compliance (4h)
- [ ] Verify xmin/xmax usage throughout (2h)
- [ ] Check for PostgreSQL MVCC patterns (2h)
- [ ] Document findings (1h)
- [ ] Fix any violations found (4h estimated)

### Issue 4.2: Index Documentation
**Files:** `docs/specifications/`, `docs/status/`
**Problem:** No comprehensive index documentation
**Impact:** Hard for developers to understand index architecture

**Tasks:**
- [ ] Create INDEX_ARCHITECTURE.md (4h)
  - Overview of all 11 index types
  - When to use each type
  - MGA compliance patterns

- [ ] Create INDEX_IMPLEMENTATION_GUIDE.md (4h)
  - How to add new index types
  - DML integration patterns
  - Bytecode integration template

- [ ] Update PROJECT_CONTEXT.md with final status (1h)

### Issue 4.3: Performance Testing
**Files:** `tests/performance/`
**Problem:** No performance benchmarks for indexes
**Impact:** Unknown performance characteristics

**Tasks:**
- [ ] Create index performance test framework (8h)
- [ ] Benchmark all 11 index types (4h)
- [ ] Document performance characteristics (2h)

---

## IMPLEMENTATION SEQUENCE

### Week 1: Critical MGA Fixes
**Days 1-2:** GIN Index MGA compliance (16h)
**Days 3-4:** Bitmap Index completion (24h)
**Day 5:** Testing and verification (8h)

### Week 2: DML Integration Part 1
**Days 1-2:** GIN, HNSW, GiST DML hooks (22h)
**Days 3-4:** SP-GiST, BRIN, R-Tree DML hooks (18h)
**Day 5:** Testing (8h)

### Week 3: DML Integration Part 2 + Bytecode Phase 1
**Days 1-2:** Columnstore, Bitmap DML hooks (14h)
**Day 3:** Bytecode opcodes (8h)
**Days 4-5:** Bytecode generation (12h)
**Weekend:** Testing (4h)

### Week 4: Bytecode Phase 2 + Documentation
**Days 1-2:** Bytecode execution (16h)
**Day 3:** Query planner integration (12h)
**Days 4-5:** R-Tree audit, documentation, final testing (16h)

---

## SUCCESS CRITERIA

### MGA Compliance (100%)
- ✅ All 11 indexes use xmin/xmax tracking
- ✅ All visibility checks use TIP-based `isVersionVisible()`
- ✅ All deletions are logical (xmax marking)
- ✅ No physical tuple removal
- ✅ No PostgreSQL MVCC patterns

### DML Integration (100%)
- ✅ All 11 indexes maintained during INSERT
- ✅ All 11 indexes maintained during UPDATE
- ✅ All 11 indexes maintained during DELETE
- ✅ No `Status::NOT_IMPLEMENTED` returns
- ✅ Integration tests for all DML operations

### Bytecode Support (100%)
- ✅ CREATE INDEX bytecode generation + execution
- ✅ DROP INDEX bytecode generation + execution
- ✅ Index selection in query planner
- ✅ Index maintenance opcodes for DML
- ✅ Integration tests for bytecode

### Code Quality
- ✅ All code follows MGA_RULES.md
- ✅ Comprehensive error handling
- ✅ No memory leaks
- ✅ Full test coverage

---

## RISK MITIGATION

### Risk 1: Breaking Existing Code
**Mitigation:**
- Run full test suite after each change
- Use feature flags for new DML hooks
- Keep old code paths until new ones verified

### Risk 2: Performance Regression
**Mitigation:**
- Benchmark before and after changes
- Profile critical paths
- Optimize hot paths

### Risk 3: MGA Violations
**Mitigation:**
- Code review against MGA_RULES.md
- Automated checks for forbidden patterns
- Integration tests with TIP validation

---

## TRACKING

**Start Date:** November 20, 2025
**Target Completion:** December 18, 2025 (4 weeks)
**Progress Updates:** Weekly

**Completion Tracking:**
- Priority 1: 0/2 issues complete (0%)
- Priority 2: 0/8 indexes complete (0%)
- Priority 3: 0/4 phases complete (0%)
- Priority 4: 0/3 items complete (0%)

**Overall Progress:** 0% → Target: 100%

---

## NOTES

1. **GIN and Bitmap are blockers** for claiming "production-ready" status
2. **DML integration is critical** for data consistency
3. **Bytecode support unlocks SQL access** to advanced indexes
4. **R-Tree audit may reveal additional work**

---

**Plan Created:** November 20, 2025
**Review Status:** Pending developer approval
**Estimated Total Effort:** 120-160 hours
