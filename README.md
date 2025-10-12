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

**Version:** Alpha 1.2 (Phase 2 & 3 Complete)
**Status:** Educational/Development (Not Production Ready)
**Last Updated:** October 11, 2025
**Latest:** See [Current Status](docs/status/CURRENT_STATUS.md) | [Code Audit](docs/audit/after_transaction_work.md) | [Documentation Audit](docs/audit/after_transaction_documentation_work.md)

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

### What's Implemented ✅
- **Storage Engine:** Page management, buffer pool with 2-pass eviction, heap pages, TOAST, compression (92%)
- **Transaction Management:** Full Firebird-style MGA with all isolation levels, sweep, GC, monitoring (98%)
- **Concurrency:** Multi-connection support, locking operational, READ ONLY optimizations (78%)
- **Indexing:** B-tree (2,256 lines) and hash indexes (2,254 lines) with scans (80%)
- **Type System:** 30+ data types, UUIDv7, timezones, 100+ collations (95%)
- **Query Processing:** Lexer, parser (with transaction statements), AST, semantic analyzer, bytecode, executor (72%)
- **Catalog:** System catalog with metadata persistence (75%)

### Known Limitations ⚠️
- **Deadlock detection incomplete** (CRIT-001 - buildWaitGraph is stub)
- **Cross-page UPDATE not implemented** (CRIT-002 - returns NOT_IMPLEMENTED)
- **Lock manager memory safety** (CRIT-003 - manual new/delete)
- **No WAL** (no crash recovery - future work)
- **Limited SQL** (no JOINs, subqueries, many features)

### Active Issues 🔧
- CRIT-001: Implement deadlock detection (1-2 weeks)
- CRIT-002: Implement cross-page UPDATE (3-5 days)
- CRIT-003: Fix lock manager memory safety (1 week)
- CRIT-004: Fix TIP page logic (3-5 days)
- CRIT-005: Standardize error handling (1 week)
- See [code audit](docs/audit/after_transaction_work.md) for full analysis

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

1. Review [Current Status](docs/status/CURRENT_STATUS.md) (updated Oct 11, 2025)
2. Check [Code Audit](docs/audit/after_transaction_work.md) for critical issues
3. Check [TODO.md](docs/development/TODO.md) for prioritized work items
4. Review completion docs: [Phase 2](docs/planning/implemented/PHASE_2_COMPLETE.md) | [Phase 3](docs/planning/implemented/PHASE_3_COMPLETE.md)
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
