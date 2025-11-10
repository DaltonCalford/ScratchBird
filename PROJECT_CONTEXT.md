# ScratchBird Project Context

**Last Updated**: November 9, 2025
**Version**: Alpha - 80% Complete (Catalog Corrections Complete)
**Status**: Educational/Development

> **MANDATORY**: Read `/MGA_RULES.md` before ANY transaction or index work.
> **IMPLEMENTATION REFERENCE**: See `/docs/IMPLEMENTATION_AUDIT.md` for complete code locations and function signatures.

---

## Current Status

### Core Engine (100%)
- **MGA (Multi-Generational Architecture)** - TIP-based visibility, O(1) transaction lookups
- **Buffer Pool & Pages** - LRU caching, heap pages with back-versioning
- **TOAST** - Large object storage with MGA compliance
- **Transactions** - 4 isolation levels, MVCC, deadlock detection
- **Tablespaces** - Multi-file support with GPID addressing

### Catalog System (36 tables = 100% structures, 50% CRUD) ✅
- **18 Schema Hierarchy** - root → sys/app/users/remote/emulation/public
- **Core Tables (10/10)** - Schemas, Tables, Columns, Indexes, Sequences, Views, Constraints, Triggers, Timezones, Collations
- **Dependencies & Comments (2/2)** - Full persistence with disk storage
- **Security (4/4 structures)** - Users, Roles, Groups, RoleMemberships (CRUD pending)
- **Stored Code (5/5 structures)** - Procedures, Parameters, Domains, UDR, Packages
- **Emulation (3/3 structures)** - Types, Servers, Databases (mysql/postgres/mssql/firebird)
- **Infrastructure (4/4)** - Tablespaces, Charsets, Statistics, Permissions
- **UUID System** - UUIDv7 (RFC 9562), system UUID: `00000000-0000-7000-8000-737973746d00`
- **Object Types** - 32 catalog object types defined

### Indexes (11/11 = 100%) 🎉
- B-Tree, Hash, R-Tree, GIN, Bitmap
- GiST, HNSW, SP-GiST, BRIN
- Columnstore, LSM-Tree
- All production-ready with MGA compliance

### Data Types (86/86 = 100%) 🎉
- Numeric: INT8-INT128, UINT8-UINT64, DECIMAL, FLOAT, MONEY
- String: CHAR, VARCHAR, TEXT
- Temporal: DATE, TIME, TIMESTAMP, INTERVAL
- Binary: BLOB, BYTEA, VARBINARY
- Special: UUID, JSON/JSONB, XML, BOOLEAN
- Spatial: POINT, LINESTRING, POLYGON
- Advanced: ARRAY, RANGE, COMPOSITE, VECTOR, VARIANT
- Network: INET, CIDR, MACADDR
- Text Search: TSVECTOR, TSQUERY
- **Domains** with CHECK constraints

### SQL Execution (21/35 = 60%)
- ✅ SELECT (WHERE, JOIN, GROUP BY, HAVING, ORDER BY, LIMIT)
- ✅ INSERT, UPDATE, DELETE
- ✅ CREATE TABLE, CREATE INDEX, CREATE/ALTER/DROP TABLESPACE
- ✅ Transactions: BEGIN, COMMIT, ROLLBACK, SAVEPOINT
- ✅ Window functions
- ✅ **DDL Modifications (100%)**:
  - DROP TABLE [IF EXISTS] [CASCADE | RESTRICT]
  - DROP INDEX [IF EXISTS] [CASCADE | RESTRICT]
  - ALTER TABLE ADD COLUMN
  - ALTER TABLE DROP COLUMN [IF EXISTS] [CASCADE | RESTRICT]
  - ALTER TABLE RENAME COLUMN
  - ALTER TABLE ALTER COLUMN TYPE
- ❌ Views, Sequences, Triggers (execution), Stored procedures
- ❌ GRANT/REVOKE, MERGE, TRUNCATE, CTEs

### Built-in Functions (60/100 = 60%)
- ✅ String (11), Aggregate (6), Window (8)
- ✅ JSON (13), Array (12), Date/Time (6)
- ✅ Conditional (3), Regex (4), Spatial (4+)
- ❌ Math (40 missing: SIN, COS, SQRT, etc.)
- ❌ Statistical, Cryptographic, XML

### Constraints (2/10 = 20%)
- ✅ NOT NULL, Data type validation
- ❌ CHECK, UNIQUE, DEFAULT, PRIMARY KEY, FOREIGN KEY enforcement

**Remaining**: ~1,150-1,650 hours

---

## MGA Architecture (Firebird Style)

**Critical:** All transaction visibility uses **TIP (Transaction Inventory Pages)**, not PostgreSQL snapshots.

### Key Principles
- **TIP-based visibility**: `isVersionVisible(xmin, current_xid)` only
- **In-place updates**: Primary record modified, old data in back versions
- **Stable TIDs**: Indexes never change unless indexed column changes
- **No snapshots**: Zero PostgreSQL MVCC contamination
- **O(1) lookups**: Transaction state in 2 bits per TIP entry

### Rules (See MGA_RULES.md)
```cpp
// CORRECT - Firebird MGA
if (isVersionVisible(tuple->xmin, current_xid)) { ... }

// WRONG - PostgreSQL MVCC (forbidden)
if (isSnapshotVisible(tuple, snapshot)) { ... }  // NEVER USE
```

**Before ANY transaction/index work:** Read `/MGA_RULES.md`

---

## Project Structure

```
ScratchBird/
├── src/
│   ├── core/          # Storage engine, indexes, transactions, catalog
│   ├── parser/        # SQL parser
│   └── sblr/          # Query executor
├── include/           # Public headers
├── tests/
│   ├── unit/          # Unit tests
│   └── integration/   # Integration tests
└── docs/
    ├── planning/      # Implementation plans
    ├── specifications/# Architecture specs
    └── status/        # Completion reports
```

---

## Architecture

### 3-Layer Embedded Design

```
┌──────────────────────────────────────────┐
│ Layer 3: Client Applications            │
│  - sb_isql (CLI) or custom apps         │
└──────────────────────────────────────────┘
                    ↓
┌──────────────────────────────────────────┐
│ Layer 2: Parser Engines                 │
│  - libsb_parser_scratchbird.so          │
│  - SQL → AST → SBLR bytecode            │
└──────────────────────────────────────────┘
                    ↓
┌──────────────────────────────────────────┐
│ Layer 1: Database Engine                │
│  - SBLR bytecode interpreter            │
│  - Storage, transactions, indexes        │
└──────────────────────────────────────────┘
```

### SBLR (ScratchBird Binary Language Runner)
**Purpose**: Dialect-agnostic bytecode for database operations (based on Firebird BLR)
**Location**: `/src/sblr/` and `/include/scratchbird/sblr/`

**Key Files**:
- `opcodes.h` - Opcode definitions
- `executor.cpp` - Bytecode interpreter (3,108 lines)
- `bytecode_generator.cpp` - AST → SBLR compiler (1,162 lines)

---

## MGA vs PostgreSQL MVCC

**CRITICAL**: ScratchBird uses **Firebird MGA**, NOT PostgreSQL MVCC

### Detection Rules

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

### Key Differences

| Aspect | Firebird MGA | PostgreSQL MVCC |
|--------|--------------|-----------------|
| Visibility | TIP bitmap (O(1)) | Snapshot array (O(N)) |
| Updates | In-place + back versions | Append-only |
| Version Chain | Newest → Oldest (N2O) | Oldest → Newest (O2N) |
| Index Updates | Only if column changed | Every UPDATE |
| TID Stability | Stable forever | Changes on UPDATE |

---

## Critical File Locations

### Core Implementation
```
src/core/buffer_pool.cpp            - Buffer management
src/core/heap_page.cpp               - Record storage with back-versioning
src/core/toast.cpp                   - Large object storage
src/core/transaction_manager.cpp    - TIP-based transaction management
src/core/btree.cpp                   - B-Tree index (~33K lines)
src/core/hash_index.cpp              - Hash index
src/core/gin_index.cpp               - GIN index
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
/docs/IMPLEMENTATION_AUDIT.md                            - **AI-OPTIMIZED** Complete implementation reference
/docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md  - Active work plan
/docs/status/CATALOG_CORRECTIONS_COMPLETE_2025-11-09.md - Catalog system completion report
```

---

## Development Guidelines

### For AI Assistants

**MANDATORY READING**:
1. Read `/MGA_RULES.md` at session start
2. Re-read `/MGA_RULES.md` after context compaction
3. Read `/MGA_RULES.md` BEFORE any transaction or index work
4. **NEW**: Read `/docs/IMPLEMENTATION_AUDIT.md` for function signatures and exact implementation locations

**IMPLEMENTATION REFERENCE**:
- `/docs/IMPLEMENTATION_AUDIT.md` contains ALL function signatures, struct layouts, and exact file:line locations
- Use this to find existing implementations without searching
- Context-optimized format (no prose, just facts)
- Updated: November 9, 2025

**DO**:
- ✅ Use Firebird MGA model (TIP-based visibility)
- ✅ Maintain stable TIDs (no changes on UPDATE)
- ✅ In-place updates with back versions
- ✅ Follow error handling patterns (Status enum, ErrorContext)
- ✅ Use RAII for all resources
- ✅ Check `/docs/IMPLEMENTATION_AUDIT.md` for existing function signatures before implementing

**DON'T**:
- ❌ Use PostgreSQL MVCC patterns (snapshots, `isSnapshotVisible()`)
- ❌ Implement `Snapshot` structures
- ❌ Use forward-versioning (old → new pointers)
- ❌ Update index TIDs unless indexed column changes
- ❌ Skip reading `/MGA_RULES.md` before transaction work
- ❌ Guess function signatures when `/docs/IMPLEMENTATION_AUDIT.md` has them

**CRITICAL**: Violating `/MGA_RULES.md` means the code is architecturally WRONG and must be rewritten.

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

## Building

```bash
# Debug build (default)
mkdir build && cd build
cmake .. && make -j$(nproc)

# Release build
cmake -DCMAKE_BUILD_TYPE=Release ..

# Thread sanitizer
cmake -DCMAKE_BUILD_TYPE=TSan ..

# Address sanitizer
cmake -DCMAKE_BUILD_TYPE=ASan ..
```

---

## Testing

```bash
# All tests
cd build && ctest --output-on-failure

# Specific test
ctest -R "test_name"

# Verbose
ctest -V
```

---

## Status Summary

**Version**: Alpha (Engine Phase 1)
**Completion**: 80% (Catalog Corrections Complete)
**MGA Compliance**: 100% ✅
**Catalog System**: 36/36 tables (100% structures, 50% CRUD) ✅
**Active Plan**: `/docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md`
**Implementation Audit**: `/docs/IMPLEMENTATION_AUDIT.md` (AI-optimized reference)
**Timeline**: 5-7 months to completion (with 3 developers)

**Recently Completed** (Nov 9, 2025):
- ✅ 36 catalog table structures defined
- ✅ 18-schema hierarchy implemented
- ✅ UUID-based object references (UUIDv7)
- ✅ Dependencies system with persistence
- ✅ Comments system with persistence
- ✅ Fresh database bootstrap for all 36 tables

**Top Priorities**:
1. Complete catalog CRUD operations (security, stored code, emulation tables)
2. Implement all 40 missing mathematical functions (SIN, COS, SQRT, etc.)
3. Implement security system (GRANT/REVOKE) using new catalog tables
4. Complete constraint enforcement (CHECK, FOREIGN KEY, DEFAULT, UNIQUE)
5. Complete PSQL bytecode execution (procedures, triggers, cursors)
6. Add CTEs and recursive queries

**After Phase 1**: Parser separation → embeddable library + standalone SQL application

---

**Last Updated**: November 9, 2025
**Status**: Phase 1 ALPHA - 80% Complete (Catalog System Ready)
