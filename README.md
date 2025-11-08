# ScratchBird Database Engine

A relational database engine featuring **Firebird MGA (Multi-Generational Architecture)**, 11 index types, TOAST storage, and full transaction management.

## Status: Alpha - 78% Complete

**Last Updated:** November 7, 2025

## Quick Start

```bash
# Build
mkdir build && cd build
cmake .. && make -j$(nproc)

# Test
ctest --output-on-failure
```

## What's Working ✅

### Core Engine (100%)
- **MGA (Multi-Generational Architecture)** - TIP-based visibility, O(1) transaction state lookups
- **Buffer Pool & Pages** - LRU caching, heap pages with back-versioning
- **TOAST** - Large object storage with MGA compliance
- **Transactions** - 4 isolation levels, MVCC, deadlock detection
- **Tablespaces** - Multi-file support with GPID addressing

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
- ✅ Window functions: ROW_NUMBER, RANK, LAG, LEAD, etc.
- ✅ **DDL Modifications (100%)**:
  - DROP TABLE [IF EXISTS] [CASCADE | RESTRICT]
  - DROP INDEX [IF EXISTS] [CASCADE | RESTRICT]
  - ALTER TABLE ADD COLUMN
  - ALTER TABLE DROP COLUMN [IF EXISTS] [CASCADE | RESTRICT]
  - ALTER TABLE RENAME COLUMN old TO new
  - ALTER TABLE ALTER COLUMN name TYPE type

### Built-in Functions (60/100 = 60%)
- ✅ String: 11 functions (LENGTH, SUBSTRING, UPPER, LOWER, TRIM, etc.)
- ✅ Aggregate: 6 (COUNT, SUM, AVG, MIN, MAX, ARRAY_AGG)
- ✅ Window: 8 functions
- ✅ JSON: 13 functions
- ✅ Array: 12 functions
- ✅ Date/Time: 6 functions
- ✅ Conditional: 3 (COALESCE, NULLIF, CASE)

## What's Missing ❌

### DDL Operations
- Views, Sequences, Triggers
- CREATE DOMAIN, CREATE TYPE

### Security & Constraints
- GRANT/REVOKE permissions
- FOREIGN KEY enforcement
- UNIQUE constraint enforcement
- DEFAULT value enforcement

### Functions
- Mathematical functions (40 missing: SIN, COS, SQRT, etc.)
- Statistical functions
- Cryptographic functions
- XML functions

### Advanced SQL
- Common Table Expressions (CTEs)
- Recursive queries
- PSQL/stored procedure execution

**Remaining:** ~1,150-1,650 hours

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

## Testing

```bash
# All tests
cd build && ctest --output-on-failure

# Specific test
ctest -R "test_name"

# Verbose
ctest -V
```

## Development Workflow

1. **Read first:**
   - [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md) - Current state
   - [MGA_RULES.md](MGA_RULES.md) - **MANDATORY** architecture rules
   - [ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md](docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md) - Work plan

2. **Before transaction/index work:** Read `/MGA_RULES.md` (violations are wrong)

3. **Code standards:** [CODING_STANDARDS.md](docs/development/CODING_STANDARDS.md)

4. **Test frequently:** Run tests after changes

## Documentation

- **MGA_RULES.md** - Firebird MGA architecture (mandatory reading)
- **PROJECT_CONTEXT.md** - Project overview and status
- **docs/planning/** - Implementation roadmaps
- **docs/specifications/** - Technical specifications
- **docs/status/** - Completion reports

## License

See LICENSE file.
