# ScratchBird Database Engine

A relational database engine built from scratch featuring **Firebird-style MGA (Multi-Generational Architecture)**, comprehensive index types, TOAST storage, and full transaction management.

## Quick Start

```bash
# Build
mkdir build && cd build
cmake .. && make

# Run tests
ctest --output-on-failure
```

## Current Status

**Version:** Alpha (Engine Phase 1 - In Progress)
**Last Updated:** November 4, 2025
**Status:** Educational/Development - **62% Complete**

### ✅ Completed Infrastructure (95% Complete)

**Core Storage & Transactions** (100%):
- Firebird MGA (Multi-Generational Architecture) - TIP-based visibility
- Buffer pool, page management, heap pages
- TOAST (The Oversized-Attribute Storage Technique) for large objects
- Transaction management with 4 isolation levels
- MVCC with back-versioning and stable TIDs
- Garbage collection and sweep

**Indexes** (6/12 types complete, 50%):
- ✅ B-Tree - Production ready with prefix compression
- ✅ Hash - Extendible hashing
- ✅ **Bitmap - Complete** (Roaring compression, NOT operations, multi-page dictionary) ✨ Nov 4
- ✅ R-Tree - Spatial indexing
- ✅ GIN - Complete (Generalized Inverted Index with wildcard/fuzzy search)
- ✅ **HNSW - Complete** (Vector similarity search, multi-layer graphs, k-NN) ✨ Nov 4
- ⚠️ BRIN - Stub (block range indexes)
- ❌ GiST, SP-GiST, Full-Text, Columnstore, LSM-Tree - Not implemented

**Data Types** (83/86 types, 97%):
- All numeric types (INT8-INT128, UINT8-UINT64, DECIMAL, FLOAT, MONEY)
- All string types (CHAR, VARCHAR, TEXT)
- All temporal types (DATE, TIME, TIMESTAMP, INTERVAL)
- Binary types (BINARY, VARBINARY, BLOB, BYTEA)
- Special types (UUID, JSON/JSONB, XML, BOOLEAN)
- Spatial types (POINT, LINESTRING, POLYGON, etc.)
- Array types, Range types, Network types (INET, CIDR, MACADDR)
- Text search types (TSVECTOR, TSQUERY)
- ⚠️ COMPOSITE, VECTOR, VARIANT - Partial implementation

**SQL Execution** (15/35 statements, 43%):
- ✅ SELECT (with WHERE, JOIN, GROUP BY, HAVING, ORDER BY, LIMIT)
- ✅ INSERT, UPDATE, DELETE
- ✅ CREATE TABLE, CREATE INDEX, CREATE/ALTER/DROP TABLESPACE
- ✅ BEGIN, COMMIT, ROLLBACK, SAVEPOINT
- ✅ Window functions (ROW_NUMBER, RANK, LAG, LEAD, etc.)
- ✅ JSON functions, Array functions, Spatial functions
- ❌ ALTER TABLE, DROP TABLE/INDEX - Not implemented
- ❌ GRANT/REVOKE - Not implemented
- ❌ Views, Sequences, Triggers (execution), CTEs - Not implemented

**Built-in Functions** (60/100, 60%):
- ✅ String: 11 functions (LENGTH, SUBSTRING, UPPER, LOWER, TRIM, etc.)
- ✅ Aggregate: 6 functions (COUNT, SUM, AVG, MIN, MAX, ARRAY_AGG)
- ✅ Window: 8 functions
- ✅ JSON: 13 functions
- ✅ Array: 12 functions
- ✅ Date/Time: 6 functions
- ✅ Conditional: 3 functions (COALESCE, NULLIF, CASE)
- ❌ Math: 0 functions (no SIN, COS, SQRT, etc.)
- ❌ Statistical, Cryptographic, XML functions - Not implemented

### 🎯 MGA Compliance - TOP PRIORITY ✅

**Status:** 100% Firebird MGA Compliant (Completed November 2, 2025)

- ✅ Pure TIP-based visibility (Transaction Inventory Pages)
- ✅ Zero PostgreSQL MVCC contamination
- ✅ In-place updates with back-versioning
- ✅ Stable TIDs (indexes never updated unless indexed column changes)
- ✅ O(1) transaction state lookups (< 100ns)
- ✅ All index types use `isVersionVisible(xmin, current_xid)`
- ✅ No snapshot arrays, no `isSnapshotVisible()` calls

**See:** `/MGA_RULES.md` for mandatory MGA architecture rules

### 📋 Current Work

**Active Plan:** `/docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md`

**Goal:** 100% feature completeness for engine embedding (Phase 1)
**Timeline:** 6-9 months (with 3 developers)
**Remaining Work:** ~1,755-2,425 hours

**Critical Priorities:**
1. Complete all 8 remaining index types (GIN, GiST, SP-GiST, BRIN, HNSW, Full-Text, Columnstore, LSM-Tree)
2. DDL modifications (ALTER TABLE, DROP statements)
3. Security system (GRANT/REVOKE)
4. All built-in functions (especially 40 missing mathematical functions)
5. Complete constraints (CHECK, FOREIGN KEY, DEFAULT, UNIQUE enforcement)
6. Complete PSQL/stored procedures (bytecode execution)
7. Views, sequences, triggers, CTEs
8. Complete data type operations (COMPOSITE, VECTOR, VARIANT, Domains)

**After Phase 1:** Parser separation into embeddable library + standalone application

### 📊 Latest Achievements

**November 4, 2025 - Two Indexes Completed (50% Index Completion Rate):**

**HNSW Vector Index - 100% Complete (Afternoon):**
- ✅ Link management (add_link, remove_link)
- ✅ Connection pruning (distance-based heuristic)
- ✅ Page reorganization for variable-sized nodes
- ✅ Full statistics (deleted_nodes, avg_connections, avg_path_length)
- ✅ All 13 API methods implemented (was 9/13)
- ✅ Production-ready for vector similarity search (k-NN)

**Bitmap Index - 100% Complete (Morning):**
- ✅ NOT operations (bitwiseNot, containerNot, findNot)
- ✅ Single tuple removal (BitmapIndex::remove)
- ✅ Multi-page dictionary (unlimited unique values)
- ✅ Actual compression ratio calculation
- ✅ Mixed container type handling
- ✅ All 21 API methods implemented (was 16/21)

**November 3, 2025 - SQL Identifier UTF-8 Complete:**
- ✅ 128-character UTF-8 identifiers (SQL:2016 §5.2)
- ✅ 512-byte storage (supports all UTF-8 characters)
- ✅ Proper character boundary integrity
- ✅ 86 comprehensive test cases

**November 2-3, 2025 - TOAST MGA Compliance Complete:**
- ✅ 28-byte TOAST chunk format with explicit xmin/xmax
- ✅ TIP-based visibility for TOAST
- ✅ All 7 index types detoast before indexing
- ✅ TOAST garbage collection

**November 2, 2025 - Firebird MGA Compliance Complete:**
- ✅ All indexes use TIP-based visibility
- ✅ Storage layer SNAPSHOT isolation uses TIP
- ✅ Zero PostgreSQL MVCC contamination

## Architecture Highlights

### Firebird MGA (Multi-Generational Architecture)

**Pure TIP-based visibility** - O(1) transaction state lookups
- Transaction Inventory Pages store 2 bits per transaction
- States: ACTIVE, COMMITTED, ABORTED, LIMBO
- No snapshot arrays (PostgreSQL MVCC eliminated)

**In-place updates with back versions:**
- Primary record modified in-place
- Old data stored as back versions
- Newest-to-Oldest (N2O) version chains
- Zero heap fragmentation by design

**Stable TIDs:**
- Indexes store permanent TIDs
- Index entries never change (unless indexed column modified)
- No index bloat from updates to non-indexed columns

### Storage Engine

- **Buffer Pool:** LRU page caching
- **Heap Pages:** Record storage with back-version chains
- **TOAST:** Out-of-line storage for large attributes
- **Tablespaces:** Multi-file support with GPID addressing

### Transaction Management

- **4 Isolation Levels:** READ UNCOMMITTED, READ COMMITTED, REPEATABLE READ, SERIALIZABLE
- **MVCC:** Multi-Version Concurrency Control via back-versioning
- **Locking:** Deadlock detection, multi-granularity locks
- **Garbage Collection:** Sweep removes old back versions

## Project Structure

- **`docs/`** - All documentation
  - `planning/` - Implementation plans and roadmaps
  - `specifications/` - Architecture specifications
  - `status/` - Completion reports
  - `audit/` - Code audits and gap analyses
- **`src/`** - Source code
  - `core/` - Storage engine, indexes, transactions, catalog
  - `parser/` - SQL parser
  - `sblr/` - Query executor (ScratchBird Binary Language Runner)
- **`tests/`** - Test suites
  - `unit/` - Unit tests
  - `integration/` - Integration tests
- **`include/`** - Public headers

## Development Process

1. **READ FIRST:**
   - [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md) - Current project state
   - [MGA_RULES.md](MGA_RULES.md) - **MANDATORY** Firebird MGA architecture rules
   - [ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md](docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md) - Current work plan

2. **Before ANY transaction/index work:** Read `/MGA_RULES.md` - violations are architecturally WRONG

3. **Review documentation:**
   - [ALPHA_ENGINE_READINESS_SUMMARY.md](docs/ALPHA_ENGINE_READINESS_SUMMARY.md) - Detailed feature analysis
   - Developer guides in `/docs/guides/`

4. **Follow standards:** [CODING_STANDARDS.md](docs/development/CODING_STANDARDS.md)

5. **Run tests frequently:** `ctest --output-on-failure`

## Building

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### Build Types

```bash
# Debug (default)
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release
cmake -DCMAKE_BUILD_TYPE=Release ..

# TSAN (thread safety)
cmake -DCMAKE_BUILD_TYPE=TSan ..

# ASAN (memory errors)
cmake -DCMAKE_BUILD_TYPE=ASan ..
```

## Testing

```bash
# Run all tests
cd build && ctest --output-on-failure

# Run specific tests
ctest -R "test_name"
```

## License

See LICENSE file for details.
