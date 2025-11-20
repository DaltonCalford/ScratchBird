# INDEX SYSTEM AGENT TASK BREAKDOWN
**Date:** November 20, 2025
**Purpose:** Discrete, assignable tasks for parallel agent execution
**Based on:** INDEX_SYSTEM_REMEDIATION_PLAN.md

---

## TASK ORGANIZATION

This document breaks down the remediation plan into **discrete, independent tasks** that can be assigned to one or more agents for parallel execution.

---

## CRITICAL PATH TASKS (MUST COMPLETE FIRST)

### TASK-CRITICAL-1: Fix GIN Index MGA Violation
**Priority:** P0 (Critical)
**Estimated Time:** 20 hours
**Dependencies:** None
**Agent Type:** Code Implementation Agent

**Objective:** Convert GIN index from physical deletion to MGA-compliant logical deletion

**Detailed Steps:**
1. **Modify PostingListEntry structure** (2h)
   - File: `src/core/gin_index.cpp`
   - Add xmin/xmax fields to PostingListEntry
   - Update serialization/deserialization
   - Verify structure size and alignment

2. **Replace physical removal with xmax marking** (8h)
   - File: `src/core/gin_index.cpp:196-257`
   - Function: `GINIndex::remove()`
   - Replace TID removal logic with xmax = current_xid
   - Preserve posting list entries
   - Update posting list compression

3. **Implement visibility checks for posting lists** (4h)
   - File: `src/core/gin_index.cpp:824-915`
   - Add `isEntryVisible()` checks in posting list traversal
   - Filter invisible entries during search
   - Update posting tree scan logic

4. **Update compaction logic** (4h)
   - File: `src/core/gin_index.cpp` (garbage collection section)
   - Respect xmax visibility during compaction
   - Only remove entries with xmax < OIT
   - Preserve active entries

5. **Add integration tests** (2h)
   - File: `tests/integration/test_gin_index.cpp` (new file)
   - Test insert → delete → visibility
   - Test concurrent transactions
   - Test garbage collection

**Acceptance Criteria:**
- ✅ No physical TID removal in remove() function
- ✅ All entries have xmin/xmax
- ✅ Visibility checks use `isVersionVisible(xmin, xmax, current_xid)`
- ✅ Tests pass with concurrent transactions
- ✅ No MGA violations detected

**Files to Modify:**
- `src/core/gin_index.cpp` (primary)
- `include/scratchbird/core/gin_index.h` (structures)
- `tests/integration/test_gin_index.cpp` (new)

---

### TASK-CRITICAL-2: Complete Bitmap Index Implementation
**Priority:** P0 (Critical)
**Estimated Time:** 24 hours
**Dependencies:** None
**Agent Type:** Code Implementation Agent

**Objective:** Complete bitmap index from 30% to 100% with full MGA compliance

**Detailed Steps:**
1. **Design bitmap entry structure** (2h)
   - File: `include/scratchbird/core/bitmap_index.h`
   - Add xmin/xmax to BitmapEntry
   - Design cardinality tracking
   - Plan disk layout

2. **Implement insert() operation** (6h)
   - File: `src/core/bitmap_index.cpp`
   - Add value → bitmap mapping
   - Create new bitmap pages
   - Set xmin on insertion
   - Handle bitmap overflow

3. **Implement remove() operation** (4h)
   - File: `src/core/bitmap_index.cpp`
   - Logical deletion with xmax
   - Preserve bitmap entries
   - Update cardinality stats

4. **Add visibility checks** (4h)
   - File: `src/core/bitmap_index.cpp:542-630`
   - Check xmin/xmax on bitmap entries (not just heap tuples)
   - Filter invisible bitmaps
   - Update AND/OR operations

5. **Update bitmap operations** (4h)
   - File: `src/core/bitmap_index.cpp:186-301`
   - Handle visibility in AND/OR/NOT
   - Skip invisible bitmap entries
   - Optimize for common cases

6. **Add integration tests** (4h)
   - File: `tests/integration/test_bitmap_index.cpp` (new)
   - Test insert/search/remove
   - Test visibility filtering
   - Test bitmap operations with transactions

**Acceptance Criteria:**
- ✅ insert() fully implemented (not stubbed)
- ✅ remove() uses xmax marking
- ✅ All bitmap entries have xmin/xmax
- ✅ Visibility checks on bitmap entries, not just heap
- ✅ Tests pass for DML operations

**Files to Modify:**
- `src/core/bitmap_index.cpp` (primary)
- `include/scratchbird/core/bitmap_index.h` (structures)
- `tests/integration/test_bitmap_index.cpp` (new)

---

## HIGH PRIORITY TASKS (DML INTEGRATION)

### TASK-DML-1: GIN Index DML Integration
**Priority:** P1 (High)
**Estimated Time:** 8 hours
**Dependencies:** TASK-CRITICAL-1 (GIN MGA fix must complete first)
**Agent Type:** Integration Agent

**Objective:** Enable GIN index maintenance during INSERT/UPDATE/DELETE

**Detailed Steps:**
1. **Implement key extraction for INSERT** (3h)
   - File: `src/core/storage_engine.cpp:74-85`
   - Extract keys using registered extractors
   - Handle array types
   - Handle composite types
   - Call gin->insert() for each key

2. **Handle UPDATE operations** (3h)
   - Detect indexed column changes
   - Extract old keys and new keys
   - Remove old keys (set xmax)
   - Insert new keys (set xmin)

3. **Handle DELETE operations** (1h)
   - Extract keys from deleted tuple
   - Mark posting list entries with xmax

4. **Add integration tests** (1h)
   - File: `tests/integration/test_gin_dml.cpp` (new)
   - Test DML maintenance
   - Verify index consistency

**Acceptance Criteria:**
- ✅ No `Status::NOT_IMPLEMENTED` for GIN in storage_engine.cpp
- ✅ GIN updated on INSERT/UPDATE/DELETE
- ✅ Tests verify index consistency after DML

**Files to Modify:**
- `src/core/storage_engine.cpp` (DML hooks)
- `tests/integration/test_gin_dml.cpp` (new)

---

### TASK-DML-2: HNSW Index DML Integration
**Priority:** P1 (High)
**Estimated Time:** 8 hours
**Dependencies:** None
**Agent Type:** Integration Agent

**Objective:** Enable HNSW vector index maintenance during INSERT/UPDATE/DELETE

**Detailed Steps:**
1. **Implement vector insertion** (3h)
   - File: `src/core/storage_engine.cpp:74-85`
   - Extract vector from tuple
   - Call hnsw->insert() with TID
   - Handle layer selection and neighbor linking

2. **Handle UPDATE operations** (3h)
   - Detect vector column changes
   - Mark old node with xmax (logical delete)
   - Insert new vector (new node with xmin)
   - Update graph connections

3. **Handle DELETE operations** (1h)
   - Mark HNSW node with xmax
   - Preserve graph structure (don't unlink)

4. **Add integration tests** (1h)
   - File: `tests/integration/test_hnsw_dml.cpp` (new)
   - Test DML on vector columns
   - Verify K-NN search consistency

**Acceptance Criteria:**
- ✅ No `Status::NOT_IMPLEMENTED` for HNSW
- ✅ HNSW updated on INSERT/UPDATE/DELETE
- ✅ Graph remains navigable after DML

**Files to Modify:**
- `src/core/storage_engine.cpp` (DML hooks)
- `tests/integration/test_hnsw_dml.cpp` (new)

---

### TASK-DML-3: GiST Index DML Integration
**Priority:** P1 (High)
**Estimated Time:** 6 hours
**Dependencies:** None
**Agent Type:** Integration Agent

**Objective:** Enable GiST predicate index maintenance during INSERT/UPDATE/DELETE

**Detailed Steps:**
1. **Implement predicate-based insertion** (2h)
   - File: `src/core/storage_engine.cpp:74-85`
   - Extract predicate key from tuple
   - Call gist->insert() with TID
   - Handle tree rebalancing

2. **Handle UPDATE operations** (2h)
   - Detect indexed column changes
   - Mark old entry with xmax
   - Insert new entry with xmin

3. **Handle DELETE operations** (1h)
   - Mark GiST entry with xmax

4. **Add integration tests** (1h)
   - File: `tests/integration/test_gist_dml.cpp` (new)

**Acceptance Criteria:**
- ✅ No `Status::NOT_IMPLEMENTED` for GiST
- ✅ GiST updated on INSERT/UPDATE/DELETE

**Files to Modify:**
- `src/core/storage_engine.cpp`
- `tests/integration/test_gist_dml.cpp` (new)

---

### TASK-DML-4: SP-GiST Index DML Integration
**Priority:** P1 (High)
**Estimated Time:** 6 hours
**Dependencies:** None
**Agent Type:** Integration Agent

**Objective:** Enable SP-GiST space-partitioning index maintenance during DML

**Detailed Steps:**
1. **Implement space-partitioning insertion** (2h)
   - File: `src/core/storage_engine.cpp:74-85`
   - Extract spatial key
   - Call spgist->insert() with quadrant logic

2. **Handle UPDATE operations** (2h)
   - Detect spatial column changes
   - Mark old entry with xmax
   - Insert new entry in correct quadrant

3. **Handle DELETE operations** (1h)
   - Mark SP-GiST entry with xmax

4. **Add integration tests** (1h)
   - File: `tests/integration/test_spgist_dml.cpp` (new)

**Acceptance Criteria:**
- ✅ No `Status::NOT_IMPLEMENTED` for SP-GiST
- ✅ SP-GiST updated on INSERT/UPDATE/DELETE

**Files to Modify:**
- `src/core/storage_engine.cpp`
- `tests/integration/test_spgist_dml.cpp` (new)

---

### TASK-DML-5: BRIN Index DML Integration
**Priority:** P1 (High)
**Estimated Time:** 6 hours
**Dependencies:** None
**Agent Type:** Integration Agent

**Objective:** Enable BRIN block-range index maintenance during DML

**Detailed Steps:**
1. **Implement range summary updates on INSERT** (2h)
   - File: `src/core/storage_engine.cpp:74-85`
   - Determine block range for TID
   - Update min/max statistics
   - Create new range if needed

2. **Handle UPDATE operations** (2h)
   - Recalculate min/max for affected range
   - Handle value changes that affect range bounds

3. **Handle DELETE operations** (1h)
   - Mark range for re-summarization if needed

4. **Add integration tests** (1h)
   - File: `tests/integration/test_brin_dml.cpp` (new)

**Acceptance Criteria:**
- ✅ No `Status::NOT_IMPLEMENTED` for BRIN
- ✅ Block ranges updated on INSERT/UPDATE/DELETE

**Files to Modify:**
- `src/core/storage_engine.cpp`
- `tests/integration/test_brin_dml.cpp` (new)

---

### TASK-DML-6: R-Tree Index DML Integration
**Priority:** P1 (High)
**Estimated Time:** 6 hours
**Dependencies:** TASK-AUDIT-1 (R-Tree audit)
**Agent Type:** Integration Agent

**Objective:** Enable R-Tree spatial index maintenance during DML

**Detailed Steps:**
1. **Implement MBR-based insertion** (2h)
   - File: `src/core/storage_engine.cpp:74-85`
   - Calculate MBR for spatial value
   - Call rtree->insert() with TID

2. **Handle UPDATE operations** (2h)
   - Detect spatial column changes
   - Update R-Tree with new MBR

3. **Handle DELETE operations** (1h)
   - Mark R-Tree entry with xmax (delegated to rtree.cpp)

4. **Add integration tests** (1h)
   - File: `tests/integration/test_rtree_dml.cpp` (new)

**Acceptance Criteria:**
- ✅ No `Status::NOT_IMPLEMENTED` for R-Tree
- ✅ R-Tree updated on INSERT/UPDATE/DELETE

**Files to Modify:**
- `src/core/storage_engine.cpp`
- `tests/integration/test_rtree_dml.cpp` (new)

---

### TASK-DML-7: Columnstore Index DML Integration
**Priority:** P1 (High)
**Estimated Time:** 8 hours
**Dependencies:** None
**Agent Type:** Integration Agent

**Objective:** Enable Columnstore maintenance during DML (append-only)

**Detailed Steps:**
1. **Implement columnar append on INSERT** (3h)
   - File: `src/core/storage_engine.cpp:74-85`
   - Extract column values
   - Call columnstore->insert() (buffered)
   - Auto-flush when buffer full

2. **Handle UPDATE operations** (3h)
   - Append new row with xmin = current_xid
   - Mark old row with xmax (already in heap)
   - No in-place updates (columnar is append-only)

3. **Handle DELETE operations** (1h)
   - Mark in heap (columnstore scans use heap visibility)

4. **Add integration tests** (1h)
   - File: `tests/integration/test_columnstore_dml.cpp` (new)

**Acceptance Criteria:**
- ✅ No `Status::NOT_IMPLEMENTED` for Columnstore
- ✅ Columnstore segments updated on INSERT/UPDATE/DELETE

**Files to Modify:**
- `src/core/storage_engine.cpp`
- `tests/integration/test_columnstore_dml.cpp` (new)

---

### TASK-DML-8: Bitmap Index DML Integration
**Priority:** P1 (High)
**Estimated Time:** 6 hours
**Dependencies:** TASK-CRITICAL-2 (Bitmap completion)
**Agent Type:** Integration Agent

**Objective:** Enable Bitmap index maintenance during DML

**Detailed Steps:**
1. **Implement bitmap updates on INSERT** (2h)
   - File: `src/core/storage_engine.cpp:74-85`
   - Extract indexed value
   - Call bitmap->insert()

2. **Handle UPDATE operations** (2h)
   - Detect value changes
   - Update bitmaps for old and new values

3. **Handle DELETE operations** (1h)
   - Mark bitmap entry with xmax

4. **Add integration tests** (1h)
   - File: `tests/integration/test_bitmap_dml.cpp` (new)

**Acceptance Criteria:**
- ✅ No `Status::NOT_IMPLEMENTED` for Bitmap
- ✅ Bitmaps updated on INSERT/UPDATE/DELETE

**Files to Modify:**
- `src/core/storage_engine.cpp`
- `tests/integration/test_bitmap_dml.cpp` (new)

---

## MEDIUM PRIORITY TASKS (BYTECODE SUPPORT)

### TASK-BYTECODE-1: Define Index Bytecode Opcodes
**Priority:** P2 (Medium)
**Estimated Time:** 8 hours
**Dependencies:** None
**Agent Type:** Bytecode Design Agent

**Objective:** Design and implement bytecode opcodes for index operations

**Detailed Steps:**
1. **Review existing CREATE/DROP INDEX opcodes** (1h)
   - File: `src/sblr/opcodes.h`
   - Verify CREATE_INDEX (if exists)
   - Verify DROP_INDEX (if exists)

2. **Define new index operation opcodes** (2h)
   - INDEX_SEARCH (search for key)
   - INDEX_SCAN_RANGE (range scan)
   - INDEX_INSERT (internal DML)
   - INDEX_UPDATE (internal DML)
   - INDEX_DELETE (internal DML)

3. **Design opcode parameters** (2h)
   - Index type enum (1 byte)
   - Table ID (4 bytes)
   - Index ID (4 bytes)
   - Key serialization format
   - TID range encoding
   - Predicate encoding

4. **Update opcodes.h** (1h)
   - Add opcode definitions
   - Document parameters
   - Add encoding examples

5. **Add validation tests** (2h)
   - File: `tests/unit/test_bytecode_opcodes.cpp`
   - Test opcode encoding/decoding
   - Test parameter validation

**Acceptance Criteria:**
- ✅ All index opcodes defined
- ✅ Parameter encoding documented
- ✅ Validation tests pass

**Files to Modify:**
- `src/sblr/opcodes.h`
- `tests/unit/test_bytecode_opcodes.cpp`

---

### TASK-BYTECODE-2: Implement Bytecode Generation
**Priority:** P2 (Medium)
**Estimated Time:** 12 hours
**Dependencies:** TASK-BYTECODE-1
**Agent Type:** Bytecode Implementation Agent

**Objective:** Generate bytecode for CREATE/DROP INDEX and index hints in SELECT

**Detailed Steps:**
1. **Generate CREATE INDEX bytecode** (3h)
   - File: `src/sblr/bytecode_generator.cpp`
   - Handle CreateIndexStmt AST node
   - Encode index type
   - Encode column list
   - Encode options (UNIQUE, WHERE clause)

2. **Generate DROP INDEX bytecode** (1h)
   - Handle DropIndexStmt AST node
   - Encode IF EXISTS
   - Encode CASCADE/RESTRICT

3. **Generate index hints for SELECT** (4h)
   - Analyze WHERE clause
   - Select best index type
   - Generate INDEX_SEARCH or INDEX_SCAN_RANGE
   - Implement cost-based selection

4. **Generate index maintenance for DML** (3h)
   - INSERT → INDEX_INSERT for all indexes
   - UPDATE → INDEX_UPDATE for all indexes
   - DELETE → INDEX_DELETE for all indexes

5. **Add bytecode generation tests** (1h)
   - File: `tests/integration/test_bytecode_generation.cpp`

**Acceptance Criteria:**
- ✅ CREATE/DROP INDEX generate bytecode
- ✅ SELECT uses index hints
- ✅ DML generates index maintenance opcodes
- ✅ Tests verify bytecode correctness

**Files to Modify:**
- `src/sblr/bytecode_generator.cpp`
- `tests/integration/test_bytecode_generation.cpp`

---

### TASK-BYTECODE-3: Implement Bytecode Execution
**Priority:** P2 (Medium)
**Estimated Time:** 16 hours
**Dependencies:** TASK-BYTECODE-2
**Agent Type:** Executor Implementation Agent

**Objective:** Execute index bytecode operations in SBLR executor

**Detailed Steps:**
1. **Implement executeCreateIndex** (4h)
   - File: `src/sblr/executor.cpp`
   - Decode bytecode parameters
   - Call index factory
   - Register in catalog
   - Build initial index

2. **Implement executeDropIndex** (2h)
   - Decode parameters
   - Unregister from catalog
   - Delete index files

3. **Implement executeIndexSearch** (4h)
   - Dispatch to index type
   - Extract key from bytecode
   - Collect TIDs
   - Return result set

4. **Implement executeIndexScan** (4h)
   - Handle range scans
   - Manage iterators
   - Apply visibility filtering
   - Return sorted results

5. **Add executor tests** (2h)
   - File: `tests/integration/test_bytecode_executor.cpp`

**Acceptance Criteria:**
- ✅ All index opcodes execute correctly
- ✅ Catalog updates persist
- ✅ Index files created/deleted
- ✅ Tests verify execution

**Files to Modify:**
- `src/sblr/executor.cpp`
- `tests/integration/test_bytecode_executor.cpp`

---

### TASK-BYTECODE-4: Query Planner Integration
**Priority:** P2 (Medium)
**Estimated Time:** 12 hours
**Dependencies:** TASK-BYTECODE-3
**Agent Type:** Planner Integration Agent

**Objective:** Integrate indexes into query planner for optimization

**Detailed Steps:**
1. **Implement index selection** (6h)
   - File: `src/sblr/query_planner.cpp` (or similar)
   - Cost estimation for index types
   - Index suitability analysis
   - Multi-index support

2. **Implement index scan plan nodes** (3h)
   - IndexScan plan type
   - BitmapIndexScan plan type
   - Generate execution plan

3. **Implement index-only scans** (2h)
   - Detect covering indexes
   - Use visibility map

4. **Add planner tests** (1h)
   - File: `tests/integration/test_query_planner.cpp`

**Acceptance Criteria:**
- ✅ Query planner selects indexes
- ✅ Cost-based optimization works
- ✅ Index-only scans detected
- ✅ Tests verify plan correctness

**Files to Modify:**
- `src/sblr/query_planner.cpp` (if exists)
- `tests/integration/test_query_planner.cpp`

---

## LOW PRIORITY TASKS (DOCUMENTATION & AUDITS)

### TASK-AUDIT-1: Audit R-Tree Implementation
**Priority:** P3 (Low)
**Estimated Time:** 13 hours
**Dependencies:** None
**Agent Type:** Code Audit Agent

**Objective:** Verify MGA compliance of underlying rtree.cpp implementation

**Detailed Steps:**
1. **Read rtree.cpp implementation** (4h)
   - File: `src/core/rtree.cpp`
   - Understand data structures
   - Trace insert/search/delete paths

2. **Check MGA compliance** (4h)
   - Verify xmin/xmax usage
   - Check for TIP-based visibility
   - Look for PostgreSQL patterns (snapshots, etc.)
   - Verify logical deletion

3. **Document findings** (2h)
   - File: `docs/audit/RTREE_MGA_AUDIT.md`
   - List violations (if any)
   - Provide recommendations

4. **Fix violations (if found)** (3h estimated)
   - Implement fixes based on audit
   - Update rtree_index.cpp wrapper if needed

**Acceptance Criteria:**
- ✅ Full audit document created
- ✅ All MGA violations identified
- ✅ Fixes implemented (if needed)

**Files to Review:**
- `src/core/rtree.cpp` (primary)
- `src/core/rtree_index.cpp` (wrapper)

**Files to Create:**
- `docs/audit/RTREE_MGA_AUDIT.md`

---

### TASK-DOC-1: Create Index Architecture Documentation
**Priority:** P3 (Low)
**Estimated Time:** 4 hours
**Dependencies:** All DML tasks complete
**Agent Type:** Documentation Agent

**Objective:** Create comprehensive index architecture documentation

**Detailed Steps:**
1. **Write INDEX_ARCHITECTURE.md** (4h)
   - File: `docs/specifications/INDEX_ARCHITECTURE.md`
   - Overview of all 11 index types
   - When to use each type
   - MGA compliance patterns
   - DML integration patterns
   - Bytecode integration

**Acceptance Criteria:**
- ✅ Document covers all 11 indexes
- ✅ Usage guidelines clear
- ✅ MGA patterns documented

**Files to Create:**
- `docs/specifications/INDEX_ARCHITECTURE.md`

---

### TASK-DOC-2: Create Index Implementation Guide
**Priority:** P3 (Low)
**Estimated Time:** 4 hours
**Dependencies:** All tasks complete
**Agent Type:** Documentation Agent

**Objective:** Create developer guide for adding new index types

**Detailed Steps:**
1. **Write INDEX_IMPLEMENTATION_GUIDE.md** (4h)
   - File: `docs/specifications/INDEX_IMPLEMENTATION_GUIDE.md`
   - How to add new index types
   - DML integration template
   - Bytecode integration template
   - MGA compliance checklist

**Acceptance Criteria:**
- ✅ Step-by-step guide created
- ✅ Templates provided
- ✅ Examples included

**Files to Create:**
- `docs/specifications/INDEX_IMPLEMENTATION_GUIDE.md`

---

### TASK-DOC-3: Update Project Context
**Priority:** P3 (Low)
**Estimated Time:** 1 hour
**Dependencies:** All tasks complete
**Agent Type:** Documentation Agent

**Objective:** Update PROJECT_CONTEXT.md with final index status

**Detailed Steps:**
1. **Update PROJECT_CONTEXT.md** (1h)
   - File: `PROJECT_CONTEXT.md`
   - Update index status to 100%
   - Update DML integration status
   - Update bytecode status
   - Update completion date

**Acceptance Criteria:**
- ✅ All index statuses updated
- ✅ Completion percentages accurate

**Files to Modify:**
- `PROJECT_CONTEXT.md`

---

### TASK-PERF-1: Create Performance Testing Framework
**Priority:** P3 (Low)
**Estimated Time:** 8 hours
**Dependencies:** All DML tasks complete
**Agent Type:** Performance Testing Agent

**Objective:** Create framework for benchmarking all index types

**Detailed Steps:**
1. **Create benchmark framework** (4h)
   - File: `tests/performance/index_benchmark.cpp` (new)
   - Insert performance
   - Search performance
   - Range scan performance
   - DML overhead measurement

2. **Run benchmarks** (2h)
   - Benchmark all 11 index types
   - Collect metrics
   - Generate reports

3. **Document results** (2h)
   - File: `docs/performance/INDEX_PERFORMANCE.md` (new)
   - Performance characteristics
   - Recommendations

**Acceptance Criteria:**
- ✅ Benchmark framework created
- ✅ All 11 indexes benchmarked
- ✅ Results documented

**Files to Create:**
- `tests/performance/index_benchmark.cpp`
- `docs/performance/INDEX_PERFORMANCE.md`

---

## TASK DEPENDENCY GRAPH

```
CRITICAL-1 (GIN MGA Fix)
    └── DML-1 (GIN DML Integration)

CRITICAL-2 (Bitmap Completion)
    └── DML-8 (Bitmap DML Integration)

DML-2 (HNSW) ─┐
DML-3 (GiST) ──┤
DML-4 (SP-GiST)├── (All independent, can run in parallel)
DML-5 (BRIN) ──┤
DML-7 (Columnstore) ─┘

AUDIT-1 (R-Tree Audit)
    └── DML-6 (R-Tree DML Integration)

BYTECODE-1 (Opcodes)
    └── BYTECODE-2 (Generation)
        └── BYTECODE-3 (Execution)
            └── BYTECODE-4 (Planner)

All DML tasks ──┐
BYTECODE-4 ─────├── DOC-1, DOC-2, DOC-3
               └── PERF-1
```

---

## PARALLELIZATION STRATEGY

### Phase 1: Critical Fixes (Week 1)
**Parallel Agents:**
- Agent A: TASK-CRITICAL-1 (GIN MGA Fix)
- Agent B: TASK-CRITICAL-2 (Bitmap Completion)
- Agent C: TASK-AUDIT-1 (R-Tree Audit)

### Phase 2: DML Integration Part 1 (Week 2)
**Parallel Agents:**
- Agent A: TASK-DML-1 (GIN) + TASK-DML-2 (HNSW)
- Agent B: TASK-DML-3 (GiST) + TASK-DML-4 (SP-GiST)
- Agent C: TASK-DML-5 (BRIN) + TASK-DML-6 (R-Tree)

### Phase 3: DML Integration Part 2 + Bytecode (Week 3)
**Parallel Agents:**
- Agent A: TASK-DML-7 (Columnstore) + TASK-DML-8 (Bitmap)
- Agent B: TASK-BYTECODE-1 + TASK-BYTECODE-2

### Phase 4: Bytecode + Docs (Week 4)
**Parallel Agents:**
- Agent A: TASK-BYTECODE-3 + TASK-BYTECODE-4
- Agent B: TASK-DOC-1 + TASK-DOC-2 + TASK-DOC-3
- Agent C: TASK-PERF-1

---

## COMPLETION TRACKING

### Priority 0 (Critical): 0/2 complete
- [ ] TASK-CRITICAL-1: GIN MGA Fix
- [ ] TASK-CRITICAL-2: Bitmap Completion

### Priority 1 (High): 0/8 complete
- [ ] TASK-DML-1: GIN DML
- [ ] TASK-DML-2: HNSW DML
- [ ] TASK-DML-3: GiST DML
- [ ] TASK-DML-4: SP-GiST DML
- [ ] TASK-DML-5: BRIN DML
- [ ] TASK-DML-6: R-Tree DML
- [ ] TASK-DML-7: Columnstore DML
- [ ] TASK-DML-8: Bitmap DML

### Priority 2 (Medium): 0/4 complete
- [ ] TASK-BYTECODE-1: Opcodes
- [ ] TASK-BYTECODE-2: Generation
- [ ] TASK-BYTECODE-3: Execution
- [ ] TASK-BYTECODE-4: Planner

### Priority 3 (Low): 0/5 complete
- [ ] TASK-AUDIT-1: R-Tree Audit
- [ ] TASK-DOC-1: Architecture Doc
- [ ] TASK-DOC-2: Implementation Guide
- [ ] TASK-DOC-3: Update Context
- [ ] TASK-PERF-1: Performance Testing

**Total Progress: 0/19 tasks complete (0%)**

---

## NOTES FOR AGENT ASSIGNMENT

1. **Task Independence:** Tasks within same priority can run in parallel unless dependencies noted
2. **Skill Requirements:**
   - Critical/DML tasks: Requires C++ expertise + MGA understanding
   - Bytecode tasks: Requires compiler/VM expertise
   - Audit tasks: Requires code review skills + MGA knowledge
   - Documentation tasks: Requires technical writing skills
   - Performance tasks: Requires profiling/benchmarking expertise

3. **Risk Mitigation:**
   - Always run tests after each task
   - Verify MGA compliance using MGA_RULES.md
   - Check for PostgreSQL MVCC contamination
   - Use feature flags for risky changes

---

**Document Created:** November 20, 2025
**Last Updated:** November 20, 2025
**Status:** Ready for agent assignment
