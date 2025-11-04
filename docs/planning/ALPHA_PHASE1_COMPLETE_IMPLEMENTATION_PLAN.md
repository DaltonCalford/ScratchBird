# ScratchBird ALPHA Phase 1 - Complete Implementation Plan

**Created**: November 3, 2025
**Scope**: Full engine feature completeness before Phase 2 (parser separation)
**Goal**: 100% implementation of all specified features
**Status**: ACTIVE PLAN

---

## EXECUTIVE SUMMARY

### Objective

Implement **ALL** remaining features specified in the grammar/documentation to achieve complete engine functionality. Phase 2 (parser separation) cannot begin until Phase 1 is 100% complete.

### Current Completion: 60% (Revised November 4, 2025)

**Remaining Work**: ~1,735-2,505 hours (43-62 weeks at 40 hours/week, or 11-16 months)
**Recent Progress**:
- ✅ GIN fully complete (November 2-3, 2025)
- ✅ **Bitmap fully complete (November 4, 2025)** ✨
**Audit Date**: November 4, 2025 - Comprehensive source code analysis revealed actual completion status

### Critical Requirements (Non-Negotiable)

**All** of the following must be 100% implemented (✅ = required, ❌ = not yet implemented):
- ⚠️ **45% of 11 index types complete** (B-Tree, Hash, R-Tree, GIN, Bitmap) - **6 remaining** (GiST 70%, SP-GiST 75%, BRIN 50%, HNSW 80%, Columnstore 0%, LSM-Tree 0%)
  - **NOTE**: FTS is NOT a separate index - it's GIN + TSVECTOR/TSQUERY types (counts as type system, not 12th index)
- ❌ Complete data type support (Domain, VECTOR, VARIANT fully operational)
- ❌ All built-in functions (math, statistical, crypto, XML, advanced string)
- ❌ LIKE operator with wildcard support
- ❌ Complete DDL (ALTER/DROP operations)
- ❌ Security system (GRANT/REVOKE)
- ❌ Views and materialized views
- ❌ Sequences
- ❌ Advanced DML (MERGE, TRUNCATE, RETURNING, CTEs)
- ❌ All constraints (CHECK, UNIQUE, DEFAULT, PRIMARY KEY, FOREIGN KEY, Exclusion, Generated columns)
- ❌ Complete PSQL/SBLR (triggers, stored procedures, full control flow)

---

## ⚠️ CRITICAL BLOCKERS - NOT YET IMPLEMENTED

**These features are COMPLETELY MISSING or STUBBED and are blocking ALPHA Phase 1 completion:**

### 🔴 CRITICAL PRIORITY (Must have for any database)

1. **DDL Modifications (0% complete)** - 80-100 hours
   - ❌ ALTER TABLE - Cannot modify schemas
   - ❌ DROP TABLE - Cannot remove tables
   - ❌ DROP INDEX - Cannot remove indexes
   - **Impact**: Schema evolution impossible

2. **Security System (0% complete)** - 80-100 hours
   - ❌ GRANT/REVOKE - No access control
   - ❌ Role management - No user permissions
   - **Impact**: All users have full access to all data

3. **Mathematical Functions (0/40 implemented)** - 30-40 hours
   - ❌ No SIN, COS, TAN, SQRT, EXP, LOG, POWER, etc.
   - **Impact**: Cannot perform basic math in queries
   - **Comparison**: PostgreSQL (40+), MySQL (30+), MSSQL (40+)

### 🟠 HIGH PRIORITY (Expected by most users)

4. **Foreign Key Constraints (0% enforced)** - 100-140 hours
   - ❌ No referential integrity enforcement
   - **Impact**: Data integrity cannot be guaranteed

5. **Views & Sequences (0% complete)** - 60-80 hours
   - ❌ No CREATE VIEW, MATERIALIZED VIEW
   - ❌ No CREATE SEQUENCE (auto-increment impossible)

6. **PSQL/SBLR Execution (10% complete)** - 140-180 hours
   - ❌ Triggers don't fire (CREATE works, execution doesn't)
   - ❌ Stored procedures don't execute
   - ❌ Bytecode generation incomplete

### 🟡 MEDIUM PRIORITY (Advanced features)

7. **6 Remaining Index Types (45% complete)** - 640-880 hours
   - ✅ B-Tree - Complete (2,834 lines, full CRUD, vacuum, compression)
   - ✅ Hash - Complete (1,464 lines, extendible hashing)
   - ✅ R-Tree - Complete (1,168 lines, spatial queries, k-NN)
   - ✅ GIN - Complete (4,155 lines, posting trees, wildcard, fuzzy)
   - ✅ **Bitmap - Complete (1,590 lines, NOT ops, multi-page dictionary, compression metrics)** ✨ **COMPLETED Nov 4, 2025**
   - ⚠️ GiST - **70% complete** (40-60 hours remaining: remove(), picksplit(), garbage collection)
   - ⚠️ SP-GiST - **75% complete** (30-40 hours remaining: picksplit(), remove(), garbage collection)
   - ⚠️ BRIN - **50% complete** (60-100 hours remaining: vacuum, multi-page, revmap, statistics)
   - ⚠️ HNSW - **80% complete** (30-40 hours remaining: link management, pruning, statistics)
   - ❌ Columnstore - **0% complete** (140-180 hours: ALL compression algorithms, predicate pushdown, batch processing)
   - ❌ LSM-Tree - **0% complete** (100-140 hours: memtable, SSTable, compaction, WAL, Bloom filter)
   - ❌ ~~FTS~~ - **NOT A SEPARATE INDEX** (it's GIN + TSVECTOR/TSQUERY types - already counted in type system)

8. **Advanced SQL (0% complete)** - 80-110 hours
   - ❌ CTEs (WITH clause)
   - ❌ MERGE statement
   - ❌ RETURNING clause

**TOTAL REMAINING**: 1,735-2,505 hours (7-10.5 months with 3 developers)

**Index Implementations Added**: 400-560 hours (GiST, SP-GiST, BRIN, HNSW completion - Bitmap now done)

**See sections below for detailed breakdown of each item.**

---

## PART 1: INDEX IMPLEMENTATIONS

### Current Status: 4/11 Complete (36%) - REVISED AFTER AUDIT

**⚠️ AUDIT FINDINGS (November 4, 2025)**: Comprehensive source code analysis revealed significant completion gaps.

**Fully Complete Indexes (4/11)**:
- ✅ B-Tree (2,834 lines)
- ✅ Hash (1,464 lines)
- ✅ R-Tree (1,168 lines)
- ✅ GIN (4,155 lines)

**Partially Complete Indexes (4/11)** - **400-560 hours remaining**:
- ✅ Bitmap (100% complete) ✨ **COMPLETED Nov 4, 2025**
- ⚠️ GiST (70% complete, 40-60 hours)
- ⚠️ SP-GiST (75% complete, 30-40 hours)
- ⚠️ BRIN (50% complete, 60-100 hours)
- ⚠️ HNSW (80% complete, 30-40 hours)

**Stub/Not Started (2/11)** - **240-320 hours remaining**:
- ❌ Columnstore (0%, stub only, 140-180 hours)
- ❌ LSM-Tree (0%, not started, 100-140 hours)

**Index Count Correction**: FTS is NOT index #12 - it's GIN + TSVECTOR/TSQUERY types (type system feature)

**Remaining Work**: 7 index types (660-910 hours)

**Audit Report**: See `/docs/analysis/INDEX_IMPLEMENTATION_AUDIT_2025-11-04.md` for full details

---

### 1. GIN (Generalized Inverted Index) - ✅ COMPLETE

**Status**: Complete (3,946 lines)
**Completion Date**: November 3, 2025
**Effort Saved**: 80-120 hours

**Implemented Features**:
1. **Core GIN Structure** ✅
   - Posting tree implementation with B-Tree structure
   - Entry tree for keys with variable-length key support
   - TID list compression (varbyte encoding)
   - Page management (meta, pending list, posting list/tree, entry tree)

2. **GIN Operations** ✅
   - Insert with posting list creation and automatic tree conversion
   - Search with posting list merge (AND/OR operations)
   - Pending list for fast insertions
   - Garbage collection support (partial - pending list only)

3. **Advanced Query Operations** ✅
   - Wildcard query support with full tree traversal
   - Fuzzy matching with Levenshtein distance
   - Selectivity-based query optimization
   - PostgreSQL GIN operator compatibility (@>, <@, &&, ?, ?|, ?&, @@)

4. **Performance Features** ✅
   - SIMD-optimized intersection and union operations
   - Cardinality estimation for query planning
   - TIP-based visibility filtering (Firebird MGA compliant)

**Deliverables Achieved**:
- ✅ Array indexing support
- ✅ JSONB indexing support
- ✅ High-performance multi-key indexing
- ✅ Wildcard and fuzzy text search
- ⚠️ Full-text search (requires TSVECTOR/TSQUERY types - deferred)

**Files Modified**:
- `src/core/gin_index.cpp` - Complete implementation (Phases 1-6)
- `include/scratchbird/core/gin_index.h` - Complete API

**Notes**:
- Full-text search requires completion of TSVECTOR/TSQUERY type operations (see PART 2)
- Posting list/tree garbage collection partially implemented (pending list complete)

---

### 2. Bitmap Index - ✅ 100% COMPLETE ✨ **COMPLETED November 4, 2025**

**Status**: ✅ COMPLETE - All API methods implemented, all TODOs resolved
**File**: `src/core/bitmap_index.cpp` (1,590 lines, up from 1,378)
**Completion Date**: November 4, 2025
**Time Taken**: ~4 hours (vs estimated 20-30 hours)

**What's Complete (100%)**:
- ✅ Roaring bitmap storage and operations
- ✅ Insert with bitmap creation
- ✅ Full AND/OR/NOT operations (bitwiseNot, containerNot, findNot)
- ✅ Single tuple removal (BitmapIndex::remove)
- ✅ Multi-page dictionary with linked-list chaining
- ✅ Actual compression ratio calculation
- ✅ Mixed container type handling (ARRAY ∩ BITSET)
- ✅ MGA compliance (TIP-based visibility)
- ✅ All 21/21 API methods implemented

**Previously Missing (NOW COMPLETE)**:
- ✅ **Multi-page dictionary** (Line 282) - IMPLEMENTED with linked-list chaining
- ✅ **Compression ratio calculation** (Line 659) - IMPLEMENTED based on cardinality
- ✅ **Mixed type handling** (Line 1102) - IMPLEMENTED converts to bitset
- ✅ **NOT operations** - IMPLEMENTED containerNot, bitwiseNot, findNot
- ✅ **Single tuple removal** - IMPLEMENTED BitmapIndex::remove()

**Documentation**:
- `/docs/status/BITMAP_INDEX_COMPLETION_REPORT_2025-11-04.md` - Completion report
- `/BITMAP_INDEX_IMPLEMENTATION_SUMMARY.md` - Implementation summary
- `/docs/specifications/BITMAP_INDEX_COMPLETION_SPEC.md`
- `/docs/planning/BITMAP_INDEX_COMPLETION_PLAN.md`

---

### 3. GiST - ⚠️ 70% COMPLETE (40-60 hours remaining)

**Status**: PARTIAL - Framework works, delete operations incomplete
**File**: `src/core/gist_index.cpp` (633 lines)
**Audit Date**: November 4, 2025

**What Works (70%)**:
- ✅ GiST framework with operator class interface
- ✅ Insert with recursive descent
- ✅ Search with subtree pruning (consistent() method)
- ✅ k-NN search with priority queue
- ✅ box_ops operator class (geometric boxes)
- ✅ MGA compliance (xmin/xmax visibility)

**Missing (30% = 40-60 hours)**:
- ⏸️ **remove() method** (Line 331) - Only logical delete, needs tree traversal - **15-20 hours**
- ⏸️ **picksplit() implementation** (Lines 459-460) - Stub, needs quadratic split algorithm - **15-20 hours**
- ⏸️ **Garbage collection** (Line 512) - Stub, needs physical entry removal - **10-15 hours**

**Specifications**:
- `/docs/specifications/GIST_INDEX_COMPLETION_SPEC.md`
- `/docs/planning/GIST_INDEX_COMPLETION_PLAN.md`

---

### 4. SP-GiST - ⚠️ 75% COMPLETE (30-40 hours remaining)

**Status**: PARTIAL - Framework works, delete operations incomplete
**File**: `src/core/spgist_index.cpp` (526 lines)
**Audit Date**: November 4, 2025

**What Works (75%)**:
- ✅ SP-GiST framework (inner/leaf distinction)
- ✅ Insert with recursive descent
- ✅ Search with partition pruning
- ✅ quad_ops (quad-tree for 2D points)
- ✅ text_ops (radix tree for prefix search)
- ✅ MGA compliance

**Missing (25% = 30-40 hours)**:
- ⏸️ **picksplit() for leaf overflow** (Line 384) - Stub, needs leaf split logic - **15-20 hours**
- ⏸️ **remove() method** (Line 400) - Only logical delete, needs tree traversal - **10-15 hours**
- ⏸️ **Garbage collection** (Line 410) - Stub, needs physical entry removal - **5-10 hours**

**Specifications**:
- `/docs/specifications/SPGIST_INDEX_COMPLETION_SPEC.md` (to be created)
- `/docs/planning/SPGIST_INDEX_COMPLETION_PLAN.md` (to be created)

---

### 5. BRIN - ⚠️ 50% COMPLETE (60-100 hours remaining)

**Status**: PARTIAL - Basic operations work, missing critical features
**File**: `src/core/brin_index.cpp` (532 lines)
**Audit Date**: November 4, 2025

**What Works (50%)**:
- ✅ BRIN page structure
- ✅ Insert with summary updates
- ✅ Scan with range filtering
- ✅ Min/max summary tracking
- ✅ MGA compliance structure

**Missing (50% = 60-100 hours)**:
- ⏸️ **Vacuum/compaction** (Line 411) - Stub, needs range removal and compaction - **30-40 hours**
- ⏸️ **Multi-page support** (Phase 1 limitation) - Currently single-page only - **20-30 hours**
- ⏸️ **Revmap (reverse map)** (Phase 2 feature) - Fast block lookup - **20-30 hours**
- ⏸️ **Statistics** (Line 495) - Returns placeholders, needs actual calculation - **5-10 hours**

**Specifications**:
- `/docs/specifications/BRIN_INDEX_COMPLETION_SPEC.md` (to be created)
- `/docs/planning/BRIN_INDEX_COMPLETION_PLAN.md` (to be created)

---

### 6. HNSW - ⚠️ 80% COMPLETE (30-40 hours remaining)

**Status**: PARTIAL - k-NN search works, link management incomplete
**File**: `src/core/hnsw_index.cpp` (1,147 lines)
**Audit Date**: November 4, 2025

**What Works (80%)**:
- ✅ Multi-layer graph construction
- ✅ Layer selection with exponential decay
- ✅ k-NN search with beam search
- ✅ Greedy best-first search
- ✅ Distance functions (Euclidean, Cosine, Inner Product)
- ✅ Insert with neighbor connections (simplified)
- ✅ MGA compliance

**Missing (20% = 30-40 hours)**:
- ⏸️ **Link management** (Lines 791-801) - add_link/remove_link stubs - **15-20 hours**
- ⏸️ **Connection pruning** (Lines 854-855) - Heuristic pruning stub - **10-15 hours**
- ⏸️ **Statistics** (Lines 488-492) - Placeholder values - **5-10 hours**

**Specifications**:
- `/docs/specifications/HNSW_INDEX_COMPLETION_SPEC.md` (to be created)
- `/docs/planning/HNSW_INDEX_COMPLETION_PLAN.md` (to be created)

---

### 7. Full-Text Search (FTS) - ✅ TYPES COMPLETE

**Status**: ✅ TSVECTOR/TSQUERY types complete (3,597 lines across 12 files)
**Note**: **FTS is GIN + types, not a separate index**

**Completed Features**:
- ✅ TSVECTOR type (document representation)
- ✅ TSQUERY type (query representation)
- ✅ to_tsvector(), to_tsquery() functions
- ✅ ts_match(), ts_rank() operations
- ✅ GIN tsvector_ops operator class
- ✅ Porter stemmer, stop word filtering

**Clarification**: FTS is **NOT index #12** - it uses GIN index with tsvector_ops operator class. The types and operators are complete.

---

### 8. Columnstore Index - ⚠️ STUB ONLY (NOT COMPLETE)

**Status**: ❌ NOT COMPLETE - Stub/Infrastructure Only (November 3, 2025)
**Current Implementation**: 776 lines (378 header + 398 implementation) - STUBS ONLY
**Required Implementation Effort**: 140-180 hours for FULL implementation

**⚠️ CRITICAL: This is STUB/INFRASTRUCTURE only - NOT a complete implementation**

**What Exists (Infrastructure Only):**
1. ✅ **Page Format Design**
   - Complete SBColumnstorePage structure (on-disk layout finalized)
   - Compression type enumeration (RLE, dictionary, bitpack, delta)
   - Segment metadata (row count, null count, min/max values)
   - MGA compliance fields (xmin, xmax, lsn)
   - TID range tracking for segments

2. ✅ **API Design**
   - Factory methods: create(), open()
   - CRUD operations: insert(), scan()
   - Compression: compressRLE(), decompressRLE()
   - Predicate pushdown: applyPredicate()
   - Statistics: getStats()
   - All methods stubbed and compile-ready

3. ✅ **MGA Compliance Structure**
   - Segment-level xmin/xmax tracking
   - Visibility checking: isValueVisible()
   - TIP-based visibility via TransactionManager
   - Garbage collection hooks
   - Stable TID references

4. ✅ **Compression Framework**
   - CompressionType enum: NONE, RLE, DICTIONARY, BITPACK, DELTA
   - ColumnSegment in-memory representation
   - Null bitmap support
   - Min/max value tracking

5. ✅ **Predicate Operations**
   - ColumnPredicate structure
   - 8 operators: =, !=, <, <=, >, >=, IS NULL, IS NOT NULL
   - Predicate pushdown architecture

**Files Created (Infrastructure)**:
- `include/scratchbird/core/columnstore.h` (378 lines) - Structures only
- `src/core/columnstore.cpp` (398 lines) - Method stubs that return OK but do nothing

**❌ NOT IMPLEMENTED (Required for Completion - 140-180 hours)**:
- ⏸️ RLE compression algorithm implementation (20-30 hours)
- ⏸️ Dictionary encoding (30-40 hours)
- ⏸️ Bit-packing compression (20-30 hours)
- ⏸️ Predicate evaluation logic (30-40 hours)
- ⏸️ Batch-mode processing / vectorization (20-30 hours)
- ⏸️ Segment management (split/merge/compact) (30-40 hours)
- ⏸️ Delta stores for updates (20-30 hours)
- ⏸️ Hybrid row-column integration (30-40 hours)

**Why Only Stub Exists:**
- 140-180 hour implementation is beyond single-session scope
- Infrastructure created to define interfaces
- **DOES NOT COUNT toward completion percentage**
- Full implementation required before Phase 1 can be considered complete

**Status**: ❌ Infrastructure only - full implementation required
**Blocking**: Yes - prevents Phase 1 completion
**Priority**: HIGH - must implement fully

**Next Steps**:
1. Create detailed technical specification (research columnstore algorithms)
2. Create comprehensive implementation plan
3. Implement full compression algorithms (RLE, dictionary, bitpack)
4. Implement predicate pushdown and filtering
5. Implement batch processing
6. Implement segment management
7. Test with actual data and verify performance

**See**: `/docs/specifications/COLUMNSTORE_SPEC.md` (to be created)
**See**: `/docs/planning/COLUMNSTORE_IMPLEMENTATION_PLAN.md` (to be created)

---

### 9. LSM-Tree Index - NEW IMPLEMENTATION

**Status**: Not implemented
**Effort**: 100-140 hours
**Priority**: MEDIUM (write-heavy workloads)

**Tasks**:
1. **LSM Structure** (40-60 hours)
   - Memtable (in-memory sorted structure)
   - SSTable format (immutable sorted files)
   - Level structure (L0, L1, ..., Ln)
   - Bloom filters per SSTable
   - Manifest file (metadata)

2. **LSM Operations** (40-60 hours)
   - Insert to memtable
   - Flush to L0 SSTable
   - Compaction (size-tiered or leveled)
   - Search with bloom filter + binary search
   - Delete with tombstones

3. **LSM Optimization** (20-30 hours)
   - Write-ahead log (WAL) for memtable recovery
   - Block cache for hot data
   - Compaction throttling
   - Key range partitioning

**Deliverables**:
- High write throughput (10x better than B-Tree for writes)
- Efficient range scans
- Space amplification control
- Write-heavy workload optimization

**Files to Create**:
- `src/core/lsm_index.cpp` (new file)
- `src/core/lsm_sstable.cpp` (SSTable format)
- `src/core/lsm_compaction.cpp` (compaction strategies)
- `include/scratchbird/core/lsm_index.h`

---

### Index Implementation Summary

| Index Type | Status | Effort (hours) | Priority | Critical Issues |
|------------|--------|----------------|----------|-----------------|
| 1. B-Tree | ✅ Complete | 0 | DONE | None |
| 2. Hash | ✅ Complete | 0 | DONE | None |
| 3. R-Tree | ✅ Complete | 0 | DONE | None |
| 4. GIN | ✅ Complete | 0 | DONE | None |
| 5. **Bitmap** | ✅ **Complete** ✨ | **0** | **DONE** | **None** (Nov 4, 2025) |
| 6. GiST | ⚠️ 70% | 40-60 | CRITICAL | remove(), picksplit(), GC |
| 7. SP-GiST | ⚠️ 75% | 30-40 | HIGH | picksplit(), remove(), GC |
| 8. BRIN | ⚠️ 50% | 60-100 | HIGH | Vacuum, multi-page, revmap |
| 9. HNSW | ⚠️ 80% | 30-40 | HIGH | Link management, pruning |
| 10. Columnstore | ❌ 0% | 140-180 | MEDIUM | Everything |
| 11. LSM-Tree | ❌ 0% | 100-140 | MEDIUM | Everything |
| ~~12. FTS~~ | ~~N/A~~ | ~~0~~ | ~~N/A~~ | **Not an index (GIN+types)** |
| **TOTAL** | **5/11 Complete (45%)** | **400-560** | - | **6 remaining** |

**Breakdown**:
- **5/11 Complete (45%)**: B-Tree, Hash, R-Tree, GIN, **Bitmap** ✨
- **4/11 Partial (400-560 hours)**: GiST, SP-GiST, BRIN, HNSW
- **2/11 Not Started (240-320 hours)**: Columnstore, LSM-Tree
- **Total Remaining**: 660-910 hours

**Timeline**: 17-23 weeks (4-6 months at 40 hours/week)

---

## PART 2: DATA TYPE COMPLETIONS

### Current Status: 83/86 Complete (97%)

**Remaining Work**: 3 types + Domain support (110-160 hours)

---

### 1. COMPOSITE Type Operations

**Status**: Stub at `domain_manager.cpp:502`
**Effort**: 30-40 hours
**Priority**: CRITICAL

**Tasks**:
1. **Type Definition** (10-15 hours)
   - CREATE TYPE ... AS (field1 type1, field2 type2, ...)
   - Nested composite types
   - Type catalog storage

2. **Operations** (15-20 hours)
   - Field access: value.field_name
   - Construction: ROW(val1, val2, ...)
   - Comparison operators
   - Assignment and casting

3. **Function Integration** (5-10 hours)
   - RETURN composite type from functions
   - Composite type parameters
   - Table-returning functions with composite

**Files to Modify**:
- `src/core/domain_manager.cpp:502` (implement NOT_IMPLEMENTED section)
- `src/parser/parser.cpp` (CREATE TYPE AS parsing)
- `src/sblr/expression_evaluator.cpp` (field access)

---

### 2. VECTOR Element Access & Operations

**Status**: Type exists, operations stubbed at lines 830-862
**Effort**: 20-30 hours
**Priority**: CRITICAL (HNSW depends on this)

**Tasks**:
1. **Element Access** (10-15 hours)
   - Subscript operator: vector[index]
   - Slice operator: vector[start:end]
   - ARRAY_LENGTH(), ARRAY_DIMS() for vectors

2. **Vector Operations** (10-15 hours)
   - Distance functions: L2 (<->), cosine (<=>), inner product (<#>)
   - Vector arithmetic: +, -, scalar multiplication
   - Normalization: vector_norm()
   - Dot product: vector_dot()

**Files to Modify**:
- `src/core/domain_manager.cpp:830-862` (implement vector operations)
- `src/sblr/expression_evaluator.cpp` (vector operators)

---

### 3. VARIANT Type Operations

**Status**: Stub at lines 1011-1051
**Effort**: 40-60 hours
**Priority**: HIGH

**Tasks**:
1. **Type Union Structure** (15-20 hours)
   - Tagged union storage (type tag + value)
   - Type discriminator functions
   - Safe type coercion

2. **Operations** (15-25 hours)
   - Construction from any type
   - IS type_name operators
   - CAST to specific type
   - Automatic type promotion

3. **Function Polymorphism** (10-15 hours)
   - Functions accepting VARIANT
   - Return VARIANT from functions
   - Type checking at runtime

**Files to Modify**:
- `src/core/domain_manager.cpp:1011-1051` (implement variant operations)

---

### 4. Domain Support (CRITICAL)

**Status**: Basic domains exist, constraints not enforced
**Effort**: 60-80 hours
**Priority**: CRITICAL

**Tasks**:
1. **Domain Catalog** (20-30 hours)
   - CREATE DOMAIN name AS base_type [CHECK (...)]
   - ALTER DOMAIN (add/drop constraints)
   - DROP DOMAIN
   - Domain dependency tracking

2. **Domain Constraints** (30-40 hours)
   - CHECK constraint evaluation (see constraint section)
   - NOT NULL enforcement
   - DEFAULT value handling
   - Constraint violation reporting

3. **Domain Types** (10-15 hours)
   - Record domains (composite with constraints)
   - Enum domains (ordered enumeration)
   - Range domains (value ranges)
   - Set domains (unordered collections)

**Files to Create**:
- `src/core/domain.cpp` (new file for domain logic)
- `include/scratchbird/core/domain.h`

**Files to Modify**:
- `src/core/catalog_manager.cpp` (domain catalog operations)
- `src/core/domain_manager.cpp:1364` (CHECK constraint evaluation)

---

## PART 3: BUILT-IN FUNCTIONS

### Current Status: ~60/100 (60%)

**Remaining Work**: ~40 functions (115-165 hours)

---

### 1. Mathematical Functions - CRITICAL

**Status**: ZERO math functions
**Effort**: 30-40 hours
**Priority**: CRITICAL (embarrassing to ship without)

**Tasks**:
1. **Basic Math** (10-15 hours)
   - ABS, SIGN (may exist as opcodes, verify)
   - ROUND, CEIL, FLOOR, TRUNC
   - MOD (may exist as %)
   - SQRT, CBRT, POWER
   - Opcode assignments: 0x200-0x20F

2. **Trigonometric** (10-15 hours)
   - SIN, COS, TAN
   - ASIN, ACOS, ATAN, ATAN2
   - DEGREES, RADIANS
   - PI() constant
   - Opcode assignments: 0x210-0x21F

3. **Exponential/Logarithmic** (10-15 hours)
   - EXP
   - LN, LOG, LOG10, LOG (base, value)
   - Opcode assignments: 0x220-0x22F

**Files to Modify**:
- `src/sblr/opcodes.h` (add math opcodes 0x200-0x22F)
- `src/sblr/bytecode_generator.cpp` (generate math opcodes)
- `src/sblr/executor.cpp` (execute math opcodes)
- `src/sblr/expression_evaluator.cpp` (evaluate math expressions)

---

### 2. Statistical Functions

**Status**: Not implemented
**Effort**: 25-35 hours
**Priority**: HIGH

**Tasks**:
1. **Basic Statistics** (15-20 hours)
   - STDDEV, STDDEV_POP, STDDEV_SAMP
   - VARIANCE, VAR_POP, VAR_SAMP
   - Aggregation framework extension
   - Opcode assignments: 0x230-0x237

2. **Correlation/Regression** (10-15 hours)
   - CORR (correlation coefficient)
   - COVAR_POP, COVAR_SAMP (covariance)
   - REGR_SLOPE, REGR_INTERCEPT, REGR_R2
   - REGR_COUNT, REGR_SXX, REGR_SYY, REGR_SXY
   - Opcode assignments: 0x238-0x23F

**Files to Modify**:
- `src/sblr/opcodes.h` (add statistical opcodes 0x230-0x23F)
- `src/sblr/executor.cpp` (aggregate execution framework)

---

### 3. Cryptographic Functions

**Status**: Not implemented
**Effort**: 15-20 hours
**Priority**: MEDIUM

**Tasks**:
1. **Hash Functions** (15-20 hours)
   - MD5 (128-bit)
   - SHA1 (160-bit)
   - SHA256 (256-bit)
   - SHA512 (512-bit)
   - Opcode assignments: 0x240-0x243
   - Use external library: OpenSSL or libsodium

**Files to Modify**:
- `src/sblr/opcodes.h` (add crypto opcodes 0x240-0x24F)
- `src/sblr/executor.cpp` (crypto execution)
- `CMakeLists.txt` (link OpenSSL)

---

### 4. XML Functions

**Status**: XML type exists, no processing functions
**Effort**: 40-50 hours
**Priority**: MEDIUM

**Tasks**:
1. **XML Parsing** (20-25 hours)
   - XMLPARSE (content | document)
   - XMLSERIALIZE
   - XMLVALIDATE
   - Use external library: libxml2

2. **XML Query** (20-25 hours)
   - XMLTABLE (full implementation, stub exists at parser line 375)
   - XMLEXISTS (XPath boolean)
   - XMLQUERY (XPath result)
   - XPATH functions

**Files to Modify**:
- `src/sblr/opcodes.h` (add XML opcodes 0x250-0x25F)
- `src/sblr/executor.cpp` (XML execution)
- `CMakeLists.txt` (link libxml2)

---

### 5. Advanced String Functions

**Status**: Basic string functions exist (11), missing advanced
**Effort**: 15-25 hours
**Priority**: HIGH

**Tasks**:
1. **String Manipulation** (15-25 hours)
   - POSITION (find substring position)
   - OVERLAY (replace substring)
   - TRANSLATE (character mapping)
   - REPEAT (repeat string N times)
   - REVERSE (reverse string)
   - SPLIT_PART (split on delimiter, return part N)
   - LPAD, RPAD (padding)
   - LTRIM, RTRIM, BTRIM (trimming variants)
   - Opcode assignments: 0x260-0x26F

**Files to Modify**:
- `src/sblr/opcodes.h` (add string opcodes 0x260-0x26F)
- `src/sblr/executor.cpp` (string execution)

---

### 6. LIKE Operator - CRITICAL FIX

**Status**: Stub at `expression_evaluator.cpp:217`
**Effort**: 10-15 hours
**Priority**: CRITICAL

**Tasks**:
1. **LIKE Implementation** (10-15 hours)
   - % wildcard (match zero or more characters)
   - _ wildcard (match exactly one character)
   - ESCAPE clause support
   - Case-sensitive LIKE
   - Case-insensitive ILIKE
   - Optimization: prefix optimization for 'abc%' pattern

**Files to Modify**:
- `src/sblr/expression_evaluator.cpp:217` (implement LIKE wildcard logic)
- Consider SP-GiST radix tree for LIKE 'prefix%' optimization

---

## PART 4: SQL STATEMENT COMPLETIONS

### Current Status: 15/35 Complete (43%)

**Remaining Work**: 20 statements (420-580 hours)

---

### 1. DDL Modifications - CRITICAL BLOCKER

**Effort**: 80-100 hours
**Priority**: CRITICAL

#### ALTER TABLE (50-60 hours)

**Tasks**:
1. **Column Operations** (25-30 hours)
   - ALTER TABLE ADD COLUMN
   - ALTER TABLE DROP COLUMN [CASCADE | RESTRICT]
   - ALTER TABLE ALTER COLUMN type
   - ALTER TABLE ALTER COLUMN SET/DROP DEFAULT
   - ALTER TABLE ALTER COLUMN SET/DROP NOT NULL
   - ALTER TABLE RENAME COLUMN

2. **Constraint Operations** (25-30 hours)
   - ALTER TABLE ADD CONSTRAINT
   - ALTER TABLE DROP CONSTRAINT [CASCADE | RESTRICT]
   - ALTER TABLE ALTER CONSTRAINT (deferrable, etc.)
   - Constraint validation for existing data

**Implementation**:
- Catalog updates for schema changes
- Data migration for type changes
- Constraint validation
- Dependency tracking (CASCADE/RESTRICT)

#### DROP Statements (30-40 hours)

**Tasks**:
1. **Basic DROP** (15-20 hours)
   - DROP TABLE [IF EXISTS] [CASCADE | RESTRICT]
   - DROP INDEX [IF EXISTS] [CASCADE | RESTRICT]
   - DROP VIEW [IF EXISTS] [CASCADE | RESTRICT]
   - DROP SEQUENCE [IF EXISTS] [CASCADE | RESTRICT]

2. **Dependency Management** (15-20 hours)
   - Dependency graph traversal
   - CASCADE deletion (dependent objects)
   - RESTRICT validation (block if dependencies)
   - Catalog cleanup

**Files to Modify**:
- `src/core/catalog_manager.cpp` (ADD: alterTable(), dropTable(), dropIndex(), dropView(), dropSequence())
- `src/parser/parser.cpp` (parse ALTER TABLE, DROP statements)
- `src/sblr/bytecode_generator.cpp` (generate opcodes)
- `src/sblr/executor.cpp` (execute DDL modifications)

---

### 2. Views - HIGH PRIORITY

**Effort**: 60-80 hours
**Priority**: HIGH

**Tasks**:
1. **Basic Views** (30-40 hours)
   - CREATE VIEW, CREATE OR REPLACE VIEW
   - DROP VIEW [CASCADE | RESTRICT]
   - View catalog storage
   - View expansion in query planner
   - View dependency tracking

2. **Updatable Views** (15-20 hours)
   - Simple updatable views (single table, no aggregates)
   - INSERT/UPDATE/DELETE through views
   - CHECK OPTION enforcement

3. **Materialized Views** (15-20 hours)
   - CREATE MATERIALIZED VIEW
   - REFRESH MATERIALIZED VIEW [CONCURRENTLY]
   - DROP MATERIALIZED VIEW
   - Materialized view storage (like table)
   - Incremental refresh (advanced, can defer)

**Files to Modify**:
- `src/core/catalog_manager.cpp` (ADD: createView(), dropView(), refreshMaterializedView())
- `src/optimizer/query_planner.cpp` (view expansion)
- `src/parser/parser.cpp` (parse view statements)

---

### 3. Sequences - HIGH PRIORITY

**Effort**: 30-40 hours
**Priority**: HIGH (IDENTITY columns depend on this)

**Tasks**:
1. **Sequence Catalog** (15-20 hours)
   - CREATE SEQUENCE [IF NOT EXISTS]
   - ALTER SEQUENCE (restart, increment, min/max, cycle)
   - DROP SEQUENCE [IF EXISTS] [CASCADE | RESTRICT]
   - Sequence catalog storage
   - Sequence dependency tracking (IDENTITY columns)

2. **Sequence Functions** (15-20 hours)
   - NEXT VALUE FOR sequence_name (NEXTVAL)
   - CURRENT VALUE FOR sequence_name (CURRVAL)
   - SETVAL, LASTVAL functions
   - Transactional sequence behavior
   - Sequence caching for performance

**Files to Modify**:
- `src/core/catalog_manager.cpp` (ADD: createSequence(), alterSequence(), dropSequence())
- `src/core/sequence.cpp` (NEW FILE: sequence value generation)
- `include/scratchbird/core/sequence.h` (NEW FILE)
- `src/sblr/executor.cpp` (NEXTVAL, CURRVAL execution)

---

### 4. Security System - CRITICAL BLOCKER

**Effort**: 80-100 hours
**Priority**: CRITICAL

**Tasks**:
1. **Permission Framework** (40-50 hours)
   - Permission catalog (sys_privileges table)
   - Role catalog (sys_roles table)
   - Role membership catalog (sys_role_members table)
   - Permission checking hooks in executor
   - Object ownership tracking

2. **GRANT/REVOKE** (40-50 hours)
   - GRANT privileges ON object TO grantee [WITH GRANT OPTION]
   - REVOKE privileges ON object FROM grantee [CASCADE | RESTRICT]
   - GRANT role TO user [WITH ADMIN OPTION]
   - REVOKE role FROM user
   - Privilege types: SELECT, INSERT, UPDATE, DELETE, TRUNCATE, REFERENCES, TRIGGER, CREATE, CONNECT, TEMPORARY, EXECUTE, USAGE, ALL
   - Object types: TABLE, DATABASE, SCHEMA, FUNCTION, PROCEDURE, SEQUENCE, DOMAIN, TYPE
   - Bulk grants: ALL TABLES/SEQUENCES/FUNCTIONS IN SCHEMA

**Files to Create**:
- `src/core/security.cpp` (NEW FILE: permission checking)
- `include/scratchbird/core/security.h` (NEW FILE)

**Files to Modify**:
- `src/core/catalog_manager.cpp` (ADD: grant(), revoke(), security catalog management)
- `src/sblr/executor.cpp` (permission checks before all operations)
- `src/parser/parser.cpp` (parse GRANT/REVOKE)

---

### 5. Advanced DML - HIGH PRIORITY

**Effort**: 80-110 hours
**Priority**: HIGH

#### MERGE Statement (40-50 hours)

**Tasks**:
- MERGE INTO target USING source ON condition
- WHEN MATCHED THEN UPDATE/DELETE
- WHEN NOT MATCHED [BY TARGET] THEN INSERT
- WHEN NOT MATCHED BY SOURCE THEN UPDATE/DELETE
- Complex match conditions
- Multiple WHEN clauses

**Implementation**:
- Join target and source
- Classify rows (matched, not matched by target, not matched by source)
- Execute appropriate action per classification
- Transactional semantics

#### TRUNCATE (10-15 hours)

**Tasks**:
- TRUNCATE TABLE table [CASCADE | RESTRICT]
- TRUNCATE TABLE table1, table2, ... (multiple tables)
- RESTART IDENTITY | CONTINUE IDENTITY
- Fast table emptying (don't scan all rows)
- Trigger TRUNCATE triggers (if implemented)

#### RETURNING Clause (15-20 hours)

**Tasks**:
- INSERT ... RETURNING *
- UPDATE ... RETURNING column_list
- DELETE ... RETURNING *
- MERGE ... RETURNING (advanced)
- Return modified rows to client

#### CTEs (WITH Clause) (15-25 hours)

**Tasks**:
- WITH cte_name AS (select) SELECT ...
- WITH RECURSIVE (recursive CTEs)
- Multiple CTEs: WITH cte1 AS (...), cte2 AS (...) SELECT ...
- [NOT] MATERIALIZED hint
- SEARCH clause (DEPTH FIRST / BREADTH FIRST)
- CYCLE detection

**Implementation**:
- CTE evaluation before main query
- Recursive CTE with iteration until fixpoint
- CTE result caching (materialization)

**Files to Modify**:
- `src/parser/parser.cpp` (parse MERGE, TRUNCATE, RETURNING, WITH)
- `src/sblr/bytecode_generator.cpp` (generate bytecodes)
- `src/sblr/executor.cpp` (execute MERGE, TRUNCATE, WITH)
- `src/optimizer/query_planner.cpp` (CTE optimization)

---

## PART 5: CONSTRAINT IMPLEMENTATIONS

### Current Status: 2/10 Complete (20%)

**Remaining Work**: 8 constraints (230-320 hours)

---

### 1. CHECK Constraints

**Status**: Stub at `domain_manager.cpp:1364`
**Effort**: 25-35 hours
**Priority**: HIGH

**Tasks**:
1. **Table CHECK Constraints** (15-20 hours)
   - ALTER TABLE ADD CONSTRAINT CHECK (expression)
   - CHECK constraint validation on INSERT/UPDATE
   - Named vs unnamed constraints
   - Constraint catalog storage

2. **Domain CHECK Constraints** (10-15 hours)
   - CREATE DOMAIN ... CHECK (expression)
   - Domain constraint validation on value assignment
   - VALUE keyword in domain CHECK

**Files to Modify**:
- `src/core/domain_manager.cpp:1364` (implement CHECK evaluation)
- `src/core/constraint.cpp` (NEW FILE: constraint evaluation logic)
- `src/sblr/executor.cpp` (check constraint validation in INSERT/UPDATE)

---

### 2. UNIQUE Constraints

**Status**: Unique indexes exist, no enforcement hooks
**Effort**: 30-40 hours
**Priority**: HIGH

**Tasks**:
1. **UNIQUE Constraint Infrastructure** (15-20 hours)
   - ALTER TABLE ADD CONSTRAINT UNIQUE (columns)
   - Unique constraint catalog
   - Link constraints to unique indexes
   - Constraint violation reporting

2. **Enforcement** (15-20 hours)
   - Check uniqueness on INSERT
   - Check uniqueness on UPDATE
   - Null handling (multiple NULLs allowed unless NOT NULL)
   - Violation error messages with constraint name

**Files to Modify**:
- `src/core/catalog_manager.cpp` (unique constraint catalog)
- `src/sblr/executor.cpp` (uniqueness checks in INSERT/UPDATE)
- `src/core/btree.cpp` (integrate uniqueness check)

---

### 3. DEFAULT Values

**Status**: Parser recognizes, no execution
**Effort**: 15-20 hours
**Priority**: HIGH

**Tasks**:
1. **DEFAULT Storage** (5-10 hours)
   - Store DEFAULT expressions in catalog
   - Support constants and function calls (e.g., DEFAULT NOW())

2. **DEFAULT Application** (10-15 hours)
   - Apply DEFAULT on INSERT when column omitted
   - Evaluate DEFAULT expression per row
   - Handle DEFAULT with sequences (NEXTVAL)

**Files to Modify**:
- `src/core/catalog_manager.cpp` (store DEFAULT in column catalog)
- `src/sblr/executor.cpp` (apply DEFAULT in INSERT)

---

### 4. PRIMARY KEY Constraints

**Status**: No special handling beyond unique index
**Effort**: 20-30 hours
**Priority**: HIGH

**Tasks**:
1. **PRIMARY KEY Infrastructure** (10-15 hours)
   - ALTER TABLE ADD CONSTRAINT PRIMARY KEY (columns)
   - One primary key per table enforcement
   - Automatic unique index creation
   - NOT NULL enforcement on PK columns

2. **PRIMARY KEY Semantics** (10-15 hours)
   - Prevent NULL in PK columns
   - Drop PK constraint
   - PK dependency tracking (foreign keys reference PK)

**Files to Modify**:
- `src/core/catalog_manager.cpp` (PK constraint catalog)
- `src/sblr/executor.cpp` (PK validation)

---

### 5. FOREIGN KEY Constraints - CRITICAL

**Status**: Not implemented
**Effort**: 100-140 hours
**Priority**: CRITICAL (data integrity)

**Tasks**:
1. **FK Catalog** (20-30 hours)
   - ALTER TABLE ADD CONSTRAINT FOREIGN KEY (columns) REFERENCES table (columns)
   - FK constraint catalog (source table, target table, columns, actions)
   - MATCH FULL | MATCH PARTIAL | MATCH SIMPLE
   - Referential actions: CASCADE, SET NULL, SET DEFAULT, RESTRICT, NO ACTION

2. **FK Validation** (40-60 hours)
   - INSERT validation: referenced row must exist
   - UPDATE validation: new FK value must exist in referenced table
   - DELETE validation on referenced table: check FK actions
   - UPDATE validation on referenced table: check FK actions
   - NULL handling (NULL FK values allowed unless NOT NULL)

3. **Referential Actions** (40-50 hours)
   - ON DELETE CASCADE: delete referencing rows
   - ON DELETE SET NULL: set FK to NULL
   - ON DELETE SET DEFAULT: set FK to DEFAULT
   - ON DELETE RESTRICT: block delete if references exist
   - ON DELETE NO ACTION: deferred check (at commit)
   - Same actions for ON UPDATE

4. **Deferred Constraints** (0 hours - defer to later)
   - DEFERRABLE, NOT DEFERRABLE
   - INITIALLY DEFERRED, INITIALLY IMMEDIATE
   - SET CONSTRAINTS

**Files to Create**:
- `src/core/foreign_key.cpp` (NEW FILE: FK validation logic)
- `include/scratchbird/core/foreign_key.h` (NEW FILE)

**Files to Modify**:
- `src/core/catalog_manager.cpp` (FK catalog operations)
- `src/sblr/executor.cpp` (FK checks in INSERT/UPDATE/DELETE)

---

### 6. Exclusion Constraints

**Status**: Not implemented
**Effort**: 50-70 hours
**Priority**: MEDIUM

**Tasks**:
1. **Exclusion Constraint Framework** (30-40 hours)
   - ALTER TABLE ADD CONSTRAINT EXCLUDE USING index_method (element WITH operator, ...)
   - Constraint catalog with operator specifications
   - Support GiST, SP-GiST index methods

2. **Exclusion Validation** (20-30 hours)
   - Check exclusion on INSERT
   - Check exclusion on UPDATE
   - Use index to find conflicting rows
   - Temporal exclusion (date ranges cannot overlap)

**Files to Modify**:
- `src/core/catalog_manager.cpp` (exclusion constraint catalog)
- `src/sblr/executor.cpp` (exclusion checks)
- `src/core/gist_index.cpp` (integrate exclusion checks)

---

### 7. Generated/Computed Columns

**Status**: Not implemented
**Effort**: 40-50 hours
**Priority**: MEDIUM

**Tasks**:
1. **Generated Column Storage** (20-25 hours)
   - GENERATED ALWAYS AS (expression) STORED
   - GENERATED ALWAYS AS (expression) VIRTUAL
   - Column catalog with generation expression
   - STORED: compute and store value
   - VIRTUAL: compute on access (not stored)

2. **Generated Column Computation** (20-25 hours)
   - Compute generated columns on INSERT
   - Recompute on UPDATE (for STORED)
   - Prevent manual assignment to generated columns
   - Dependency tracking (generated column depends on other columns)

**Files to Modify**:
- `src/core/catalog_manager.cpp` (store generation expression)
- `src/sblr/executor.cpp` (compute generated columns)

---

## PART 6: PSQL/SBLR PROCEDURAL LANGUAGE

### Current Status: AST Nodes Only (~20%)

**Remaining Work**: Complete bytecode generation and execution (140-180 hours)

---

### 1. Stored Procedures & Functions - CRITICAL

**Effort**: 80-100 hours
**Priority**: CRITICAL

**Tasks**:
1. **Procedure/Function Catalog** (20-25 hours)
   - CREATE [OR REPLACE] PROCEDURE/FUNCTION
   - ALTER PROCEDURE/FUNCTION
   - DROP PROCEDURE/FUNCTION
   - Function catalog with source, bytecode, parameters, return type
   - Function overloading support

2. **Bytecode Generation Completion** (40-50 hours)
   - Assignment execution (currently stubbed at `bytecode_generator.cpp:3126`)
   - ELSIF support (TODO at `bytecode_generator.cpp:3165`)
   - FOR loops (cursor iteration, range iteration)
   - CASE statement (procedural, not just expression)
   - Exception handling (TRY...EXCEPT bytecode)

3. **Execution Framework** (20-25 hours)
   - Call stack management
   - Local variable storage
   - Parameter passing (IN, OUT, INOUT)
   - Return value handling
   - Cursor state management

**Files to Modify**:
- `src/core/catalog_manager.cpp` (procedure/function catalog)
- `src/sblr/bytecode_generator.cpp:3126` (complete assignment)
- `src/sblr/bytecode_generator.cpp:3165` (implement ELSIF)
- `src/sblr/executor.cpp` (procedure/function execution)

---

### 2. Cursors - HIGH PRIORITY

**Effort**: 30-40 hours
**Priority**: HIGH

**Tasks**:
1. **Cursor Infrastructure** (15-20 hours)
   - DECLARE cursor [SCROLL] FOR select
   - Cursor catalog (local to procedure)
   - Cursor state (position, open/closed, result set)

2. **Cursor Operations** (15-20 hours)
   - OPEN cursor [(parameters)]
   - FETCH [orientation] FROM cursor INTO variables
   - Fetch orientations: NEXT, PRIOR, FIRST, LAST, ABSOLUTE n, RELATIVE n
   - CLOSE cursor
   - FOR variable IN cursor LOOP (implicit open/fetch/close)

**Files to Modify**:
- `src/sblr/bytecode_generator.cpp` (cursor bytecode)
- `src/sblr/executor.cpp` (cursor execution)

---

### 3. Exception Handling - HIGH PRIORITY

**Effort**: 30-40 hours
**Priority**: HIGH

**Tasks**:
1. **Exception Framework** (15-20 hours)
   - Exception type catalog
   - Exception state (SQLSTATE, message, detail, context)
   - Exception propagation

2. **Exception Operations** (15-20 hours)
   - RAISE EXCEPTION/NOTICE/WARNING/INFO/DEBUG
   - TRY...EXCEPT execution
   - WHEN exception_condition THEN handler
   - Exception conditions: exception_name, SQLSTATE, OTHERS
   - RESIGNAL (re-throw)

**Files to Modify**:
- `src/sblr/bytecode_generator.cpp` (exception bytecode)
- `src/sblr/executor.cpp` (exception execution, stack unwinding)

---

### 4. Triggers - HIGH PRIORITY

**Effort**: 60-80 hours
**Priority**: HIGH

**Tasks**:
1. **Trigger Catalog** (20-30 hours)
   - CREATE [OR REPLACE] TRIGGER
   - ALTER TRIGGER (enable/disable)
   - DROP TRIGGER
   - Trigger catalog (timing, event, level, function, condition)

2. **Trigger Execution** (40-50 hours)
   - BEFORE/AFTER/INSTEAD OF firing
   - FOR EACH ROW/STATEMENT
   - Trigger ordering (POSITION, alphabetical)
   - OLD/NEW row variables (ROW triggers)
   - OLD_TABLE/NEW_TABLE (STATEMENT triggers, transition tables)
   - WHEN condition evaluation
   - Trigger recursion control

**Files to Modify**:
- `src/core/catalog_manager.cpp` (trigger catalog)
- `src/sblr/executor.cpp` (trigger firing in INSERT/UPDATE/DELETE)
- `src/sblr/bytecode_generator.cpp` (trigger function bytecode)

---

## PART 7: EFFORT SUMMARY

### Total Remaining Work

| Category | Tasks | Effort (hours) | Priority |
|----------|-------|----------------|----------|
| **Index Implementations** | 7 types (5 partial + 2 new) | 660-910 | CRITICAL/HIGH |
| **Data Types** | 3 types + Domains | 110-160 | CRITICAL |
| **Built-in Functions** | ~40 functions | 115-165 | CRITICAL/HIGH |
| **SQL Statements** | 20 statements | 420-580 | CRITICAL/HIGH |
| **Constraints** | 8 constraints | 230-320 | CRITICAL/HIGH |
| **PSQL/SBLR** | Procedures, triggers | 140-180 | CRITICAL |
| **TOTAL** | **78 tasks** | **1,755-2,535** | - |

**Note**: After November 4, 2025 audit, index effort increased from original estimate. Partial index completions (420-590 hours) + new indexes (240-320 hours) = 660-910 total hours for indexes.

### Revised Timeline

**Total Effort**: **1,755-2,535 hours** (includes 660-910 hours for index completions)

**Timeline Options**:

1. **Single Developer** (40 hours/week):
   - Minimum: 1,755 hours ÷ 40 = **44 weeks (11 months)**
   - Maximum: 2,535 hours ÷ 40 = **63 weeks (16 months)**

2. **Two Developers** (80 hours/week combined):
   - Minimum: 1,755 hours ÷ 80 = **22 weeks (5.5 months)**
   - Maximum: 2,535 hours ÷ 80 = **32 weeks (8 months)**

3. **Three Developers** (120 hours/week combined):
   - Minimum: 1,755 hours ÷ 120 = **15 weeks (3.75 months)**
   - Maximum: 2,535 hours ÷ 120 = **21 weeks (5.25 months)**

### Recommended: Phased Approach with 2-3 Developers

**Target**: 6 months to Phase 1 completion

---

## PART 8: IMPLEMENTATION PHASES

### Phase 1A: Critical Blockers (Parallel) - 6-8 weeks

**Team**: 2-3 developers working in parallel

**Track 1: DDL & Security** (160-180 hours)
- ALTER TABLE, DROP statements
- GRANT/REVOKE
- Views, Sequences

**Track 2: Constraints & Types** (200-260 hours)
- CHECK, UNIQUE, DEFAULT, PRIMARY KEY
- FOREIGN KEY (most complex)
- COMPOSITE, VECTOR, VARIANT types
- Domain support

**Track 3: Functions** (80-110 hours)
- All mathematical functions
- Statistical functions
- LIKE operator fix
- Advanced string functions

**Total Parallel**: 440-550 hours (with 3 devs = 6-8 weeks)

---

### Phase 1B: Advanced SQL (Parallel) - 4-6 weeks

**Track 1: Advanced DML** (80-110 hours)
- MERGE, TRUNCATE
- RETURNING clause
- CTEs (WITH clause)

**Track 2: PSQL Foundation** (110-140 hours)
- Stored procedures/functions bytecode
- Cursors
- Exception handling

**Total Parallel**: 190-250 hours (with 2 devs = 5-7 weeks)

---

### Phase 1C: Indexes Part 1 (Parallel) - 8-10 weeks

**Track 1: Complete Partial Indexes** (180-250 hours)
- ✅ Bitmap completion (DONE - November 4, 2025)
- GiST completion (40-60 hours)
- SP-GiST completion (30-40 hours)
- BRIN completion (60-100 hours)
- HNSW completion (30-40 hours)

**Track 2: New Indexes** (240-320 hours)
- Columnstore (140-180 hours)
- LSM-Tree (100-140 hours)

**Total Parallel**: 420-570 hours (with 2-3 devs = 7-10 weeks)

---

### Phase 1D: PSQL & Advanced Features (Parallel) - 4-6 weeks

**Track 1: PSQL Completion** (60-80 hours)
- Triggers (60-80 hours)
- Final bytecode testing

**Track 2: Advanced Constraints** (50-70 hours)
- Exclusion constraints
- Generated columns

**Track 3: Additional Testing** (40-60 hours)
- Index integration tests
- Performance benchmarks
- Stress testing

**Total Parallel**: 150-210 hours (with 3 devs = 2-3 weeks)

---

### Phase 1E: Testing & Polish - 2-4 weeks

**All Developers**: Integration testing, bug fixes, optimization

**Effort**: 120-160 hours (with 3 devs = 2-3 weeks)

---

## PART 9: TOTAL TIMELINE

### With 3 Developers (Recommended)

| Phase | Duration | Weeks | Cumulative |
|-------|----------|-------|------------|
| Phase 1A: Critical Blockers | 440-550 hrs | 6-8 | 6-8 |
| Phase 1B: Advanced SQL | 190-250 hrs | 4-6 | 10-14 |
| Phase 1C: Indexes Part 1 | 460-640 hrs | 8-11 | 18-25 |
| Phase 1D: Indexes Part 2 | 350-470 hrs | 5-7 | 23-32 |
| Phase 1E: Testing & Polish | 120-160 hrs | 2-3 | 25-35 |
| **TOTAL PHASE 1** | **1,755-2,535 hrs** | **15-21 weeks** | **3.75-5.25 months** |

**Revised After Audit (November 4, 2025)**:
- Total: **1,755-2,535 hours**
- 3 developers: **15-21 weeks (3.75-5.25 months)**
- Index implementations: **660-910 hours** (up from original estimate)

### With 2 Developers

**Total**: 22-32 weeks (5.5-8 months)

### With 1 Developer

**Total**: 44-63 weeks (11-16 months)

---

## PART 10: SUCCESS CRITERIA

### Phase 1 Completion Checklist

Before Phase 2 (parser separation) can begin, ALL of the following must be ✅:

#### Indexes (11/11 = 100%)
- [x] B-Tree (complete)
- [x] Hash (complete)
- [x] R-Tree (complete)
- [x] **GIN (complete - Nov 3, 2025)** ✅
- [ ] Bitmap (complete multi-page dictionary)
- [ ] GiST (complete remove/picksplit/GC)
- [ ] SP-GiST (complete picksplit/remove/GC)
- [ ] BRIN (complete vacuum/multi-page/revmap)
- [ ] HNSW (complete link management/pruning)
- [ ] Columnstore (complete implementation)
- [ ] LSM-Tree (complete implementation)
- ~~Full-Text Search~~ - **NOT an index** (it's GIN + TSVECTOR/TSQUERY types)

#### Data Types (86/86 = 100%)
- [x] 83 types complete
- [ ] COMPOSITE operations (complete stub)
- [ ] VECTOR operations (complete stub)
- [ ] VARIANT operations (complete stub)
- [ ] Domain support (complete implementation)

#### Built-in Functions (100/100 = 100%)
- [x] 60 functions complete
- [ ] 15 mathematical functions
- [ ] 9 statistical functions
- [ ] 4 cryptographic functions
- [ ] 8+ XML functions
- [ ] 8 advanced string functions
- [ ] LIKE operator (fix wildcard support)

#### SQL Statements (35/35 = 100%)
- [x] 15 statements complete
- [ ] ALTER TABLE (all variants)
- [ ] DROP TABLE/INDEX/VIEW/SEQUENCE
- [ ] CREATE/DROP VIEW
- [ ] CREATE/ALTER/DROP SEQUENCE
- [ ] GRANT/REVOKE
- [ ] MERGE, TRUNCATE
- [ ] RETURNING clause
- [ ] CTEs (WITH clause)

#### Constraints (10/10 = 100%)
- [x] NOT NULL (complete)
- [x] Type validation (complete)
- [ ] CHECK constraints
- [ ] UNIQUE enforcement
- [ ] DEFAULT values
- [ ] PRIMARY KEY
- [ ] FOREIGN KEY
- [ ] Exclusion constraints
- [ ] Generated columns
- [ ] Domain constraints

#### PSQL/SBLR (100%)
- [ ] Stored procedures (bytecode + execution)
- [ ] Functions (bytecode + execution)
- [ ] All control flow (IF, ELSIF, LOOP, WHILE, FOR, CASE)
- [ ] Assignment (complete stub)
- [ ] Cursors (DECLARE, OPEN, FETCH, CLOSE)
- [ ] Exception handling (RAISE, TRY...EXCEPT)
- [ ] Triggers (CREATE, execution, OLD/NEW variables)

### Phase 1 Complete = Engine Ready for Embedding

**Status**: 100% feature complete per specifications
**Next**: Phase 2 - Parser separation into embedded library + standalone application

---

## PART 11: RESOURCE ALLOCATION RECOMMENDATIONS

### Team Composition (Recommended)

**3 Developers for 4-6 months**:

**Developer 1: Index Specialist**
- Focus: Complete 7 remaining index types
- GiST, SP-GiST, BRIN, HNSW completion (400-560 hours) - Bitmap DONE Nov 4, 2025
- Columnstore, LSM-Tree implementation (240-320 hours)
- **Effort**: ~660-910 hours

**Developer 2: SQL Engine Specialist**
- Focus: DDL, DML, constraints, security
- ALTER/DROP, Views, Sequences, GRANT/REVOKE, MERGE, CTEs, all constraints
- **Effort**: ~650-900 hours

**Developer 3: PSQL/Type Specialist**
- Focus: Procedural language, data types, functions
- Stored procedures, triggers, cursors, exceptions, types, all functions
- **Effort**: ~365-505 hours

### Skills Required

**Developer 1** (Index Specialist):
- Strong algorithms and data structures background
- Experience with B-trees, hash tables, graphs (HNSW)
- Spatial indexing knowledge (R-trees, quad-trees)
- Column-oriented storage (Columnstore)
- LSM-trees and compaction strategies

**Developer 2** (SQL Engine Specialist):
- Deep SQL standards knowledge
- Database catalog design
- Query optimization experience
- Constraint and referential integrity implementation
- Security/permission systems

**Developer 3** (PSQL/Type Specialist):
- Compiler/interpreter experience (bytecode generation)
- Type system design
- Mathematical function library integration
- Procedural language runtime design
- Exception handling frameworks

---

## PART 12: RISKS & MITIGATION

### Risk 1: Timeline Overrun

**Risk**: Implementation takes longer than estimated (50% contingency already included)

**Mitigation**:
- Weekly progress reviews
- Adjust scope if necessary (defer low-priority features)
- Add 4th developer if severely behind schedule

### Risk 2: Complexity Underestimation

**Risk**: FOREIGN KEY, HNSW, Columnstore may be more complex than estimated

**Mitigation**:
- Spike (proof-of-concept) for complex features in week 1
- Re-estimate after spike
- Consider external libraries (e.g., FAISS for HNSW)

### Risk 3: Dependency Blocking

**Risk**: Some features depend on others (Full-Text needs GIN)

**Mitigation**:
- Clear dependency graph
- Prioritize foundation features first
- Parallel tracks where possible

### Risk 4: Testing Insufficient

**Risk**: Integration testing reveals major bugs late in project

**Mitigation**:
- Test-driven development (TDD) for critical features
- Continuous integration testing
- Weekly integration test runs
- Dedicated Phase 1E (2-4 weeks) for testing

---

## CONCLUSION

### Summary

**Phase 1: Complete Engine Implementation**
- **Scope**: 100% of specified features
- **Effort**: 1,755-2,535 hours (revised after audit)
- **Timeline**: 4-6 months (with 3 developers)
- **Deliverable**: Feature-complete embeddable SQL engine

**Index Implementations** (Major Component):
- 4/11 complete (36%): B-Tree, Hash, R-Tree, GIN
- 4/11 partial (400-560 hours): GiST, SP-GiST, BRIN, HNSW (Bitmap completed Nov 4)
- 2/11 not started (240-320 hours): Columnstore, LSM-Tree
- Total: 660-910 hours remaining

**Phase 2: Parser Separation** (after Phase 1)
- **Scope**: Extract parser, create standalone application
- **Effort**: 120-180 hours
- **Timeline**: 3-4 weeks

**Total ALPHA**: 5-7 months

### Critical Path

1. **Weeks 1-8**: Critical blockers (DDL, security, constraints, types, math functions)
2. **Weeks 9-14**: Advanced SQL (PSQL, advanced DML)
3. **Weeks 15-24**: Index completions (Bitmap, GiST, SP-GiST, BRIN, HNSW) + new indexes (Columnstore, LSM-Tree)
4. **Weeks 25-27**: PSQL completion + advanced constraints
5. **Weeks 28-30**: Testing & polish

**Phase 1 Complete → Phase 2 Begin**

### Next Steps

1. **Assemble team** (2-3 developers with required skills)
2. **Week 1**: Setup, spike complex features, finalize estimates
3. **Week 2+**: Begin Phase 1A (Critical Blockers)
4. **Weekly**: Progress reviews, blockers resolution
5. **Month 4-6**: Phase 1 completion
6. **Month 7**: Phase 2 (parser separation)

**ALPHA ENGINE READY**: Month 7

---

**Document Version**: 1.1
**Created**: November 3, 2025
**Updated**: November 4, 2025 (Post-Audit Revision)
**Status**: ACTIVE PLAN
**Target**: Phase 1 Complete in 4-6 months (3 developers)
**Key Changes**: Accurate index completion status (4/11 complete, not 10/12)
**Next Milestone**: Team assembly and Phase 1A kickoff
