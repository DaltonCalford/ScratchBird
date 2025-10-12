# ScratchBird Database Engine

An educational relational database engine built from scratch featuring Firebird-style transaction management, multi-connection support, MVCC with all isolation levels, B-tree/hash indexing, TOAST storage, and comprehensive garbage collection.

## Quick Start

```bash
# Build
mkdir build && cd build
cmake .. && make

# Run tests
ctest --output-on-failure
```

## Current Status

**Version:** Alpha 1.2 (Phase 2, 3 & 4 Complete)
**Status:** Educational/Development (Not Production Ready)
**Last Updated:** October 12, 2025
**Latest:** See [Current Status](docs/status/CURRENT_STATUS.md) | [Code Audit](docs/audit/after_transaction_work.md) | [TODO](docs/development/TODO.md)

### ✅ Major Milestones (October 2025)

**Phase 2: ConnectionContext & Always-In-Transaction (Oct 7, 2025)**
- ✅ Multi-connection support with thread-local storage
- ✅ Always-in-transaction execution model
- ✅ Locking enabled throughout codebase (15+ TODO markers resolved)
- ✅ Parser support for transaction statements (START TRANSACTION, COMMIT, ROLLBACK)

**Phase 3: Firebird Transaction Model (Oct 7-11, 2025)**
- ✅ Transaction markers (OIT, OAT, OST, NEXT)
- ✅ All four isolation levels (READ UNCOMMITTED, READ COMMITTED, REPEATABLE READ, SERIALIZABLE)
- ✅ Sweep mechanism for garbage collection
- ✅ Long transaction monitoring with action policies
- ✅ READ ONLY transaction optimizations
- ✅ Monitoring queries (MON_ACTIVE_TRANSACTIONS, etc.)

**Phase 4: Critical Issues Resolution (Oct 12, 2025)**
- ✅ CRIT-001: Deadlock detection fully implemented with cycle detection & victim selection
- ✅ CRIT-002: Cross-page tuple updates with version chain management
- ✅ CRIT-003: Lock manager converted to RAII (smart pointers, memory safe)
- ✅ CRIT-004: Transaction abort in deadlock detector (full rollback)
- ✅ CRIT-005: Long transaction monitor stub implementations completed
- ✅ HIGH-002: Buffer pool eviction safety with comprehensive bounds checking
- ✅ HIGH-003: Magic numbers moved to configuration (config.h)
- ✅ HIGH-004: Logging framework integrated (Logger with categories & levels)
- ✅ MED-001: Code formatting standardized with clang-format (83 files)
- ✅ MED-002: Const correctness (Phase 1, 2 & 3) - 19 methods improved across 6 classes
- ✅ MED-003: Removed commented debug code (fprintf/printf cleanup)
- ✅ MED-004: Type cast safety audit (4 const_cast, 345 reinterpret_cast audited)
- ✅ MED-005: Page size mismatch detection with logging & statistics tracking

### What's Implemented ✅
- **Storage Engine:** Page management, buffer pool with 2-pass eviction, heap pages, TOAST, compression, cross-page updates (95%)
- **Transaction Management:** Full Firebird-style MGA with all isolation levels, sweep, GC, monitoring (98%)
- **Concurrency:** Multi-connection support, locking operational, deadlock detection, READ ONLY optimizations (95%)
- **Indexing:** B-tree (2,256 lines) and hash indexes (2,254 lines) with scans, index updates on relocation (85%)
- **Type System:** 30+ data types, UUIDv7, timezones, 100+ collations (95%)
- **Query Processing:** Lexer, parser (with transaction statements), AST, semantic analyzer, bytecode, executor (72%)
- **Catalog:** System catalog with metadata persistence (75%)
- **Code Quality:** RAII throughout, comprehensive logging, configuration system, const-correct APIs (90%)

### Known Limitations ⚠️
- **No WAL** (no crash recovery - Beta requirement)
- **Limited SQL** (no JOINs, subqueries, many advanced features - see TODO.md)
- **No network layer** (local database only - Beta requirement)
- **Incomplete type system** (missing INT128, UINT types, JSONB, XML, VECTOR - see ALPHA-001)
- **Missing advanced indexes** (GIN, GIST, BRIN, Bitmap - see ALPHA-003)

### Recent Achievements 🎉
- ✅ **All Critical Issues Resolved** - CRIT-001 through CRIT-005 completed (Oct 12, 2025)
- ✅ **All High Priority Issues Resolved** - HIGH-001 through HIGH-005 completed (Oct 12, 2025)
- ✅ **All Medium Priority Issues Resolved** - MED-001 through MED-005 completed (Oct 12, 2025)
- ✅ **Memory Safety** - Lock manager fully RAII-ified with smart pointers
- ✅ **Production-Ready Logging** - Logger framework with categories and levels integrated
- ✅ **Code Quality Improvements** - 83 files formatted with clang-format, magic numbers eliminated
- ✅ **API Safety** - Const correctness improvements across 19 methods in 6 core classes
- ✅ **Corruption Detection** - Page size mismatches now logged and tracked with statistics
- See [TODO.md](docs/development/TODO.md) for remaining work and Alpha 1.2 requirements

## Project Structure

- **`docs/`** - All documentation
  - `status/` - Implementation status and completion reports
  - `planning/` - Implementation plans and roadmaps
  - `development/` - Development notes and analysis
  - `design/` - Architecture and design documents
  - `specifications/` - Technical specifications
- **`src/`** - Source code
  - `core/` - Storage engine, indexes, transactions
  - `parser/` - SQL parser
  - `sblr/` - Query executor
- **`tests/`** - Test suites
  - `unit/` - Unit tests
  - `integration/` - Integration tests
- **`include/`** - Public headers

## Development Process

1. Review [Current Status](docs/status/CURRENT_STATUS.md) (updated Oct 12, 2025)
2. Check [TODO.md](docs/development/TODO.md) for prioritized work items (Alpha 1.2 goals)
3. Review completion docs: [Phase 2](docs/planning/implemented/PHASE_2_COMPLETE.md) | [Phase 3](docs/planning/implemented/PHASE_3_COMPLETE.md)
4. Review audits: [Code Audit](docs/audit/after_transaction_work.md) | [Doc Audit](docs/audit/after_transaction_documentation_work.md)
5. Follow [Coding Standards](docs/development/CODING_STANDARDS.md)
6. Run tests frequently with `ctest --output-on-failure`
7. See [Build Instructions](docs/development/BUILD_INSTRUCTIONS.md) for details

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Testing

```bash
# Run all tests
ctest --output-on-failure

# Run specific tests
ctest -R "Alpha101"
```

## License

See LICENSE file for details.
