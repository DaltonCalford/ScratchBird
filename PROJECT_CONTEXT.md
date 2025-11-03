# ScratchBird Project Context

**Last Updated**: November 3, 2025
**Version**: Alpha (Engine Phase 1 - In Progress)
**Status**: Educational/Development - 60% Complete

> **PURPOSE**: This file provides essential context for AI assistants working on ScratchBird.
> Read this file at session start and after every context compaction.
> **CRITICAL**: Always check /src/sblr/ and /src/parser/ directories - they contain core functionality!
> **MANDATORY**: Read `/MGA_RULES.md` before ANY transaction or index work.

---

## 1. Current Implementation Status (What's Actually Complete)

### ✅ Core Infrastructure (95% Complete)

**Storage Engine** (100%):
- Buffer pool with LRU page caching
- Heap page management with back-versioning
- TOAST (The Oversized-Attribute Storage Technique) for large objects
- Tablespace system with GPID addressing (64-bit: 16-bit tablespace + 48-bit page)
- Cross-page UPDATE support with stable TIDs

**Transaction Management** (100%):
- **Firebird MGA** (Multi-Generational Architecture) - **TOP PRIORITY** ✅
- TIP-based visibility (Transaction Inventory Pages)
- 4 isolation levels: READ UNCOMMITTED, READ COMMITTED, REPEATABLE READ, SERIALIZABLE
- In-place updates with back-versioning
- Stable TIDs (indexes never updated unless indexed column changes)
- Garbage collection and sweep
- **Zero PostgreSQL MVCC contamination** - mandatory compliance

**Indexes** (4/12 types, 33%):
- ✅ **B-Tree**: Production-quality, prefix compression, TIP-based visibility (~33K lines)
- ✅ **Hash**: Extendible hashing, TIP-based visibility (1,464 lines)
- ✅ **Bitmap**: Roaring compression, TIP post-filtering (1,379 lines)
- ✅ **R-Tree**: Spatial indexing, full TIP integration
- ⚠️ **HNSW**: Stub (510 lines) - vector search
- ⚠️ **BRIN**: Stub (404 lines) - block range indexes
- ⚠️ **GIN**: Partial implementation - inverted indexes
- ❌ **GiST, SP-GiST, Full-Text, Columnstore, LSM-Tree**: Not implemented

**Data Types** (83/86 types, 97%):
- All numeric types (INT8-INT128, UINT8-UINT64, DECIMAL, FLOAT, MONEY)
- All string types (CHAR, VARCHAR, TEXT) with UTF-8 support
- All temporal types (DATE, TIME, TIMESTAMP, INTERVAL)
- Binary types (BINARY, VARBINARY, BLOB, BYTEA)
- Special types (UUID, JSON/JSONB, XML, BOOLEAN)
- Spatial types (POINT, LINESTRING, POLYGON, MULTI* variants)
- Array types, Range types, Network types
- Text search types (TSVECTOR, TSQUERY)
- ⚠️ COMPOSITE, VECTOR, VARIANT - Type exists but operations stubbed

**SQL Execution** (15/35 statements, 43%):
- ✅ SELECT (WHERE, JOIN, GROUP BY, HAVING, ORDER BY, LIMIT, window functions)
- ✅ INSERT, UPDATE, DELETE
- ✅ CREATE TABLE, CREATE INDEX
- ✅ CREATE/ALTER/DROP TABLESPACE, ATTACH/DETACH TABLESPACE
- ✅ BEGIN, COMMIT, ROLLBACK, SAVEPOINT
- ❌ ALTER TABLE, DROP TABLE/INDEX - Not implemented
- ❌ CREATE/DROP VIEW, CREATE/DROP SEQUENCE - Not implemented
- ❌ GRANT/REVOKE - Not implemented
- ❌ MERGE, TRUNCATE, CTEs (WITH clause) - Not implemented
- ❌ Triggers (CREATE exists, execution stubbed), Stored procedures (bytecode stubbed)

**Built-in Functions** (60/100, 60%):
- ✅ String: 11 (LENGTH, SUBSTRING, UPPER, LOWER, TRIM, CONCAT, CONVERT, COLLATE, etc.)
- ✅ Aggregate: 6 (COUNT, SUM, AVG, MIN, MAX, ARRAY_AGG)
- ✅ Window: 8 (ROW_NUMBER, RANK, DENSE_RANK, LAG, LEAD, FIRST_VALUE, LAST_VALUE, NTH_VALUE)
- ✅ JSON: 13 (JSON_EXTRACT, JSON_OBJECT, JSON_ARRAY, JSON_SET, operators, etc.)
- ✅ Array: 12 (ARRAY_APPEND, ARRAY_CAT, ARRAY_LENGTH, UNNEST, etc.)
- ✅ Date/Time: 6 (NOW, CURRENT_DATE, DATE_ADD, DATE_SUB, DATE_DIFF, AT TIME ZONE)
- ✅ Conditional: 3 (COALESCE, NULLIF, CASE)
- ✅ Regex: 4 (REGEXP_MATCHES, REGEXP_REPLACE, REGEXP_SPLIT_*)
- ✅ Spatial: 4+ (ST_Point, ST_Distance, ST_Contains, ST_Intersects)
- ❌ Math: **0** - No mathematical functions (no SIN, COS, SQRT, etc.)
- ❌ Statistical, Cryptographic, XML - Not implemented
- ⚠️ LIKE operator - Stub (wildcards not implemented)

**Constraints** (2/10, 20%):
- ✅ NOT NULL
- ✅ Data type validation
- ⚠️ CHECK - Catalog exists, evaluation stubbed
- ⚠️ UNIQUE - Indexes exist, enforcement hooks missing
- ⚠️ DEFAULT - Parser recognizes, execution missing
- ❌ PRIMARY KEY - No special handling beyond unique index
- ❌ FOREIGN KEY - Not implemented
- ❌ Exclusion constraints - Not implemented
- ❌ Generated/computed columns - Not implemented

### 🎯 MGA Compliance - HIGHEST PRIORITY ✅

**Status**: 100% Firebird MGA Compliant (Completed November 2, 2025)

**Critical Achievements**:
- ✅ All 7 implemented index types use TIP-based `isVersionVisible(xmin, current_xid)`
- ✅ Storage layer SNAPSHOT isolation uses TIP lookups (not snapshot arrays)
- ✅ Zero `Snapshot*` parameters in any API
- ✅ Zero `isSnapshotVisible()` calls in codebase
- ✅ O(1) TIP lookups (< 100ns per visibility check)
- ✅ In-place updates with back-versioning (not append-only)
- ✅ Stable TIDs (indexes never change unless indexed column modified)
- ✅ Newest-to-Oldest (N2O) version chains

**MANDATORY READING**: `/MGA_RULES.md` - 15 absolute rules for MGA compliance
- Read BEFORE any transaction or index work
- Violations are architecturally WRONG and must be rewritten
- No exceptions, no mixing with PostgreSQL MVCC

**Key MGA Concepts**:
- **TIP (Transaction Inventory Pages)**: 2-bit bitmap (ACTIVE, COMMITTED, ABORTED, LIMBO)
- **Visibility**: `isVersionVisible(version_xid, reader_xid)` checks TIP, not snapshots
- **Updates**: Modify primary in-place, create back version with old data
- **Version Chains**: Primary → back version → older back version (N2O)
- **Garbage Collection**: Sweep removes back versions older than OIT

### 📊 Latest Achievements

**November 3, 2025:**
- ✅ SQL Identifier UTF-8 Complete (6 phases, 128 characters, 512 bytes, 86 tests)
- ✅ TOAST MGA Compliance Complete (6 phases, 28-byte chunk format, TIP-based visibility)
- ✅ Archived planning documents to `/docs/planning/archive/`
- ✅ Created comprehensive ALPHA Phase 1 Implementation Plan

**November 2, 2025:**
- ✅ Firebird MGA Compliance Complete (7 phases, all indexes TIP-compliant)

---

## 2. Active Work Plan

**Current Plan**: `/docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md`

**Goal**: 100% feature completeness for engine embedding (Phase 1 ALPHA)

**Timeline**: 6-9 months (with 3 developers)
**Remaining Work**: ~1,755-2,425 hours

**Critical Implementation Priorities**:

1. **Index Implementations** (8 types, 740-1,020 hours):
   - GIN (Generalized Inverted Index) - 80-120 hours
   - GiST (Generalized Search Tree) - 100-140 hours
   - SP-GiST (Space-Partitioned GiST) - 80-120 hours
   - BRIN completion - 60-80 hours
   - HNSW completion - 120-160 hours
   - Full-Text Search - 60-80 hours
   - Columnstore - 140-180 hours
   - LSM-Tree - 100-140 hours

2. **Data Type Completions** (110-160 hours):
   - COMPOSITE type operations (30-40 hours)
   - VECTOR element access and operations (20-30 hours)
   - VARIANT type operations (40-60 hours)
   - Domain support (60-80 hours) - **CRITICAL**

3. **Built-in Functions** (115-165 hours):
   - Mathematical functions (30-40 hours) - **CRITICAL** (0 functions currently)
   - Statistical functions (25-35 hours)
   - Cryptographic functions (15-20 hours)
   - XML functions (40-50 hours)
   - Advanced string functions (15-25 hours)
   - LIKE operator wildcard support (10-15 hours) - **CRITICAL**

4. **SQL Statement Completions** (420-580 hours):
   - DDL modifications (ALTER TABLE, DROP statements) - 80-100 hours **CRITICAL**
   - Security (GRANT/REVOKE) - 80-100 hours **CRITICAL**
   - Views and materialized views - 60-80 hours
   - Sequences - 30-40 hours
   - Advanced DML (MERGE, TRUNCATE, RETURNING, CTEs) - 80-110 hours

5. **Constraint Implementations** (230-320 hours):
   - CHECK constraints - 25-35 hours
   - UNIQUE enforcement - 30-40 hours
   - DEFAULT values - 15-20 hours
   - PRIMARY KEY - 20-30 hours
   - FOREIGN KEY - 100-140 hours **CRITICAL**
   - Exclusion constraints - 50-70 hours
   - Generated/computed columns - 40-50 hours

6. **PSQL/Stored Procedures** (140-180 hours):
   - Complete bytecode generation - 80-100 hours **CRITICAL**
   - Cursors - 30-40 hours
   - Exception handling - 30-40 hours
   - Triggers execution - 60-80 hours

**After Phase 1 Complete**: Parser separation into embeddable library + standalone SQL application

---

## 3. Architecture & Design Principles

### 3-Layer Embedded Architecture

```
┌─────────────────────────────────────────────────────────┐
│ Layer 3: Client Applications                            │
│  - sb_isql (CLI tool) or custom applications            │
└─────────────────────────────────────────────────────────┘
                         ↓ uses
┌─────────────────────────────────────────────────────────┐
│ Layer 2: Parser Engines (Dialect-Specific Libraries)    │
│  - libsb_parser_scratchbird.so (ScratchBird SQL)        │
│  - Future: Firebird, PostgreSQL, MySQL parsers          │
│  • SQL → AST → SBLR bytecode (one-way translation)      │
└─────────────────────────────────────────────────────────┘
                         ↓ SBLR bytecode
┌─────────────────────────────────────────────────────────┐
│ Layer 1: Database Engine (libscratchbird.so)            │
│  - SBLR bytecode interpreter/executor                   │
│  - Storage, transactions, indexes, catalog              │
│  - Universal backend via SBLR                           │
└─────────────────────────────────────────────────────────┘
```

### SBLR (ScratchBird Binary Language Runner)

**Purpose**: Dialect-agnostic bytecode for database operations (based on Firebird BLR)
**Location**: `/src/sblr/` and `/include/scratchbird/sblr/`
**Design**: Stack-based interpreter with opcode instructions

**Key Files**:
- `opcodes.h` - Opcode definitions
- `executor.cpp` - Bytecode interpreter (3,108 lines)
- `bytecode_generator.cpp` - AST → SBLR compiler (1,162 lines)

### Firebird MGA vs PostgreSQL MVCC

**CRITICAL**: ScratchBird uses **Firebird MGA**, NOT PostgreSQL MVCC

**Detection Rules** (from `/MGA_RULES.md`):

**❌ WRONG (PostgreSQL MVCC)**:
- `Snapshot` structures
- `isSnapshotVisible()` calls
- Forward pointers (old → new)
- Append-only updates
- Index TID updates on every UPDATE

**✅ CORRECT (Firebird MGA)**:
- TIP (Transaction Inventory Pages)
- `getTransactionState(xid)` calls
- `isVersionVisible(version_xid, reader_xid)`
- Back pointers (new → old)
- In-place updates
- Stable TIDs

**Key Differences**:

| Aspect | Firebird MGA | PostgreSQL MVCC |
|--------|--------------|-----------------|
| Visibility | TIP bitmap (O(1)) | Snapshot array (O(N)) |
| Updates | In-place + back versions | Append-only |
| Version Chain | Newest → Oldest (N2O) | Oldest → Newest (O2N) |
| Index Updates | Only if column changed | Every UPDATE |
| TID Stability | Stable forever | Changes on UPDATE |

---

## 4. Critical File Locations

### Core Implementation
```
src/core/buffer_pool.cpp            - Buffer management
src/core/heap_page.cpp               - Record storage with back-versioning
src/core/toast.cpp                   - Large object storage (823 lines)
src/core/transaction_manager.cpp    - TIP-based transaction management
src/core/btree.cpp                   - B-Tree index (~33K lines)
src/core/hash_index.cpp              - Hash index (1,464 lines)
src/core/bitmap_index.cpp            - Bitmap index (1,379 lines)
src/core/rtree.cpp                   - R-Tree spatial index
src/core/catalog_manager.cpp        - System catalog
```

### Parser & Executor
```
src/parser/parser.cpp                - SQL parser
src/parser/semantic_analyzer.cpp    - Semantic analysis
src/sblr/bytecode_generator.cpp     - AST → SBLR compiler
src/sblr/executor.cpp                - SBLR interpreter
src/sblr/expression_evaluator.cpp   - Expression evaluation
```

### Documentation
```
/MGA_RULES.md                                            - **MANDATORY** MGA architecture rules
/PROJECT_CONTEXT.md                                      - This file
/docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md  - Active work plan
/docs/ALPHA_ENGINE_READINESS_SUMMARY.md                  - Detailed feature analysis
/docs/specifications/MGA_IMPLEMENTATION.md               - MGA architecture spec
/docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md  - Transaction model
```

---

## 5. Development Guidelines

### For AI Assistants

**MANDATORY READING**:
1. Read `/MGA_RULES.md` at session start
2. Re-read `/MGA_RULES.md` after context compaction
3. Read `/MGA_RULES.md` BEFORE any transaction or index work

**DO**:
- ✅ Use Firebird MGA model (TIP-based visibility)
- ✅ Maintain stable TIDs (no changes on UPDATE)
- ✅ In-place updates with back versions
- ✅ Check active work plan before implementing features
- ✅ Follow error handling patterns (Status enum, ErrorContext)
- ✅ Use RAII for all resources

**DON'T**:
- ❌ Use PostgreSQL MVCC patterns (snapshots, `isSnapshotVisible()`)
- ❌ Implement `Snapshot` structures
- ❌ Use forward-versioning (old → new pointers)
- ❌ Update index TIDs unless indexed column changes
- ❌ Create new tuple locations on UPDATE
- ❌ Skip reading `/MGA_RULES.md` before transaction work

**CRITICAL**: Violating `/MGA_RULES.md` means the code is architecturally WRONG and must be rewritten. No exceptions.

### Error Handling
```cpp
Status operation(ErrorContext* ctx) {
    if (error) {
        SET_ERROR_CONTEXT(ctx, Status::ERROR_CODE, "Error message");
        return Status::ERROR_CODE;
    }
    return Status::OK;
}
```

### Memory Management
- RAII everywhere (smart pointers, lock guards)
- `std::unique_ptr` for ownership
- `std::shared_ptr` for shared ownership
- No manual `new`/`delete`

### Logging
```cpp
LOG_DEBUG(Category::STORAGE, "Message %s", var);
LOG_INFO(Category::TRANSACTION, "Message %d", count);
LOG_WARN(Category::CATALOG, "Message");
LOG_ERROR(Category::BUFFER, "Message");
```

---

## 6. Quick Commands

### Build
```bash
mkdir build && cd build
cmake .. && make -j$(nproc)
```

### Run Tests
```bash
cd build && ctest --output-on-failure
```

### Build Types
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..      # Default
cmake -DCMAKE_BUILD_TYPE=Release ..    # Optimized
cmake -DCMAKE_BUILD_TYPE=TSan ..       # Thread safety
cmake -DCMAKE_BUILD_TYPE=ASan ..       # Memory errors
```

---

## 7. Project Status Summary

**Version**: Alpha (Engine Phase 1)
**Completion**: 60%
**MGA Compliance**: 100% ✅
**Active Plan**: `/docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md`
**Timeline**: 6-9 months to Phase 1 completion (with 3 developers)

**Top Priorities**:
1. Complete all 8 remaining index types
2. Implement all 40 missing mathematical functions
3. Add DDL modification operations (ALTER/DROP TABLE)
4. Implement security system (GRANT/REVOKE)
5. Complete constraint enforcement (CHECK, FOREIGN KEY, DEFAULT, UNIQUE)
6. Complete PSQL bytecode execution (procedures, triggers, cursors)
7. Add views, sequences, CTEs
8. Complete data type operations (COMPOSITE, VECTOR, VARIANT, Domains)

**After Phase 1**: Parser separation → embeddable library + standalone SQL application

---

**Last Updated**: November 3, 2025
**Status**: Phase 1 ALPHA - 60% Complete
**Next Milestone**: Begin Phase 1A (Critical Blockers) per implementation plan
