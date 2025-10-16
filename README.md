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

**Version:** Alpha 1.2+ (MGA Phases 1-4 COMPLETE & VALIDATED, Phase 5 Design Complete)
**Status:** Educational/Development (Core MGA Validated)
**Last Updated:** October 16, 2025 (22:00)
**Latest:** See [MGA Alpha Status](docs/MGA_ALPHA_STATUS.md) | [Current Status](docs/status/CURRENT_STATUS.md) | [TODO](docs/development/TODO.md)

### ✅ Major Milestones (October 2025)

**MGA Back Versioning Implementation COMPLETE & VALIDATED (Oct 16, 2025)** 🎉
- ✅ **Phase 1**: Data structure changes - renamed next_version_tid → back_version_tid, added helper methods
- ✅ **Phase 2**: updateTuple() rewrite - 3-phase Firebird MGA algorithm with back version creation
- ✅ **Phase 3**: findVisibleVersion() rewrite - dual-access mode, N2O traversal, cycle detection
- ✅ **Phase 4**: Cross-page back versions - page allocation infrastructure (Database::allocate_page_id, BufferPool::allocatePage/markDirty), cross-page creation/traversal, snapshot pin management
- ✅ **Phase 5 Design**: Index integration architecture documented - awaits executor layer implementation
- ✅ **Phase 6**: Test suite development - 500+ lines, 6 comprehensive tests
- ✅ Item pointer stability - SAME item_id returned on UPDATE (core MGA benefit)
- ✅ N2O version chains - Newest-to-Oldest traversal
- ✅ Cross-page support - no more PAGE_FULL errors, works with TOAST
- ✅ Test environment cleanup - deprecated 10+ outdated tests, created standalone validation tool
- ✅ **Alpha validation - COMPLETE** - 3/3 tests passing (100%)
- ✅ **Performance benchmarks - COMPLETE** - sub-microsecond version chain traversal

**Phase 2: ConnectionContext & Always-In-Transaction (Oct 7, 2025)**
- ✅ Multi-connection support with thread-local storage
- ✅ Always-in-transaction execution model
- ✅ Locking enabled throughout codebase
- ✅ Parser support for transaction statements

**Phase 3: Firebird Transaction Model (Oct 7-11, 2025)**
- ✅ Transaction markers (OIT, OAT, OST, NEXT)
- ✅ All four isolation levels
- ✅ Sweep mechanism for garbage collection
- ✅ Long transaction monitoring
- ✅ READ ONLY optimizations

**Phase 4: Critical Issues Resolution (Oct 12-14, 2025)**
- ✅ **23/23 CRITICAL issues resolved** (9 fixed + 14 false positives)
- ✅ 15/41 MAJOR issues resolved (26 remaining)
- ✅ Comprehensive audit report with 126 issues documented

### What's Implemented ✅
- **Storage Engine:** Page management, buffer pool, heap pages, TOAST, compression, page allocation infrastructure (100%)
- **Transaction Management:** Full Firebird-style MGA with all isolation levels, sweep, GC, monitoring (98%)
- **MVCC/MGA:** ✅ **Back versioning (Phases 1-4)**, cross-page support, stable item pointers, N2O chains, snapshot pin management (100% implementation, Phase 5 design complete)
- **Concurrency:** Multi-connection support, locking operational, deadlock detection, READ ONLY optimizations (95%)
- **Indexing:** B-tree (2,256 lines), Hash (2,254 lines), GIN (Phase 6 complete), Bitmap (Roaring compression) (90%)
- **Type System:** 30+ data types, UUIDv7, timezones, 100+ collations (95%)
- **Query Processing:** Lexer, parser (with transaction statements), AST, semantic analyzer, bytecode, executor (72%)
- **Catalog:** System catalog with metadata persistence (75%)
- **Code Quality:** RAII throughout, comprehensive logging, configuration system, const-correct APIs (90%)

### Known Limitations ⚠️
- **No WAL** (no crash recovery - Beta requirement)
- **Limited SQL** (no JOINs, subqueries, many advanced features - see TODO.md)
- **No network layer** (local database only - Beta requirement)
- **MGA Index Integration:** Phase 5 design complete, implementation awaits executor layer development
- **Incomplete type system** (missing INT128, UINT types, JSONB, XML, VECTOR - see ALPHA-001)

### Recent Achievements 🎉
- ✅ **MGA Phases 1-4 COMPLETE & VALIDATED** - Full Firebird-style back versioning with cross-page support (Oct 16, 2025) 🎉
- ✅ **MGA Alpha Validation COMPLETE** - 3/3 tests passing (100%), all core functionality verified (Oct 16, 2025) 🎉
- ✅ **MGA Performance Benchmarks COMPLETE** - Sub-microsecond version chain traversal validated (Oct 16, 2025) 🎉
- ✅ **MGA Phase 5 Design Complete** - Index integration architecture documented, awaits executor (Oct 16, 2025)
- ✅ **Page Allocation Infrastructure** - Database::allocate_page_id, BufferPool::allocatePage/markDirty (Oct 16, 2025)
- ✅ **Cross-Page Back Versions** - No more PAGE_FULL errors, full TOAST support (Oct 16, 2025)
- ✅ **MGA Test Suite** - 500+ lines, 6 comprehensive tests covering all Alpha scenarios (Oct 16, 2025)
- ✅ **Test Environment Cleanup** - Deprecated 10+ outdated tests, standalone validation tool created (Oct 16, 2025)
- ✅ **All Critical Issues Resolved** - 23/23 critical issues from comprehensive audit (Oct 14, 2025)
- ✅ **PHASE 2 Major Fixes** - 15/41 major issues resolved (Oct 14, 2025)
- ✅ **Memory Safety** - Lock manager fully RAII-ified with smart pointers
- ✅ **Production-Ready Logging** - Logger framework with categories and levels integrated
- ✅ **Code Quality Improvements** - 83 files formatted with clang-format, magic numbers eliminated
- See [MGA_ALPHA_STATUS.md](docs/MGA_ALPHA_STATUS.md) for complete MGA implementation details
- See [TODO.md](docs/development/TODO.md) for remaining work and Alpha 1.3 roadmap

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

1. Review [MGA Alpha Status](docs/MGA_ALPHA_STATUS.md) (updated Oct 16, 2025) - **Latest milestone**
2. Review [Current Status](docs/status/CURRENT_STATUS.md) (updated Oct 12, 2025)
3. Check [TODO.md](docs/development/TODO.md) for prioritized work items (Alpha 1.3 goals)
4. Review [MGA Implementation Plan](docs/planning/MGA_PROPER_INDEX_IMPLEMENTATION_PLAN.md) for MGA details
5. Review [Comprehensive Audit](docs/audit/COMPREHENSIVE_AUDIT_REPORT.md) for known issues
6. Follow [Coding Standards](docs/development/CODING_STANDARDS.md)
7. Run tests frequently with `ctest --output-on-failure`
8. See [Build Instructions](docs/development/BUILD_INSTRUCTIONS.md) for details

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
