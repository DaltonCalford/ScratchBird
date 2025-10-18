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

**Version:** Alpha 1.2+ (MGA Complete, All Alpha Issues RESOLVED, CI/CD Infrastructure Complete, Documentation Complete)
**Status:** Educational/Development (Production-Ready Core, Comprehensive Testing, Developer Documentation)
**Last Updated:** October 18, 2025 (00:15)
**Latest:** See [MGA Alpha Status](docs/MGA_ALPHA_STATUS.md) | [Alpha Issues Tracker](docs/audit/ALPHA_ISSUES_TRACKER.md) | [CI/CD Guide](docs/CI_CD_GUIDE.md)

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

**Phase 4: All Alpha Issues Resolution (Oct 12-17, 2025)** 🎉
- ✅ **ALL 21 Alpha issues resolved** (100% completion in 5 days!)
- ✅ **CRITICAL ISSUES (5)**: BufferPool atomics, TransactionManager const, Lock ordering, Exception safety (Oct 16)
- ✅ **HIGH ISSUES (8)**: Page table race, Lock manager multimap, Snapshot pin management, XID ordering, Cross-page updates, Page leaks, Index errors (Oct 17)
- ✅ **MEDIUM ISSUES (7)**: BufferPool stats, TOAST overflow/offset, GIN magic number, PageManager destructor, Vector operations (Oct 17)
- ✅ **LOW ISSUES (1)**: Manual database header allocation (Oct 17)
- ✅ Production-ready concurrency, memory management, error handling
- ✅ 23/23 Phase 1 audit issues resolved (9 fixed + 14 false positives)
- ✅ 19/41 Phase 2 major issues resolved (Oct 16-17: Issues 2.16-2.19 complete)

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

**ALL ALPHA ISSUES RESOLVED (Oct 17, 2025)** 🎉🎉🎉
- ✅ **21/21 ISSUES RESOLVED** - 100% completion in 5 days!
- ✅ **CRITICAL (5)**: BufferPool atomics, TransactionManager const, Lock ordering, Exception safety
- ✅ **HIGH (8)**: Page table race, Lock manager race, BTree documentation, Snapshot pins, XID ordering, Cross-page updates, Page leaks, Index errors
- ✅ **MEDIUM (7)**: Stats accuracy, TOAST validation, GIN cleanup, Destructor flush, Vector safety
- ✅ **LOW (1)**: Modern RAII for database header
- ✅ **Production-Ready Core** - All concurrency, memory, and error handling issues resolved

**CI/CD Infrastructure COMPLETE (Oct 17, 2025)** 🎉
- ✅ **GitHub Actions Workflow** - 7 parallel jobs (TSAN, ASAN, Helgrind, Clang-Tidy, Leak Detection, Pin/Unpin Balance, Summary)
- ✅ **Sanitizer Tests** - TSAN, ASAN, Helgrind, Valgrind integrated
- ✅ **Static Analysis** - Clang-Tidy with comprehensive checks
- ✅ **Local Testing Script** - tools/run_sanitizers.sh for pre-commit validation
- ✅ **Suppression Files** - False positive filtering for all sanitizers
- ✅ **Documentation** - CI_CD_GUIDE.md (650+ lines)

**MGA Implementation COMPLETE (Oct 16, 2025)** 🎉
- ✅ **MGA Phases 1-4 COMPLETE & VALIDATED** - Full Firebird-style back versioning with cross-page support
- ✅ **MGA Alpha Validation COMPLETE** - 3/3 tests passing (100%), all core functionality verified
- ✅ **MGA Performance Benchmarks COMPLETE** - Sub-microsecond version chain traversal validated
- ✅ **MGA Phase 5 Design Complete** - Index integration architecture documented, awaits executor
- ✅ **Page Allocation Infrastructure** - Database::allocate_page_id, BufferPool::allocatePage/markDirty
- ✅ **Cross-Page Back Versions** - No more PAGE_FULL errors, full TOAST support
- ✅ **MGA Test Suite** - 500+ lines, 6 comprehensive tests covering all Alpha scenarios
- ✅ **Test Environment Cleanup** - Deprecated 10+ outdated tests, standalone validation tool created

**Developer Documentation COMPLETE (Oct 17, 2025)** 🎉
- ✅ **LOCKING_PROTOCOL.md** - Lock ordering rules, deadlock prevention (74 KB)
- ✅ **ERROR_HANDLING_GUIDE.md** - Exception patterns, Status codes, ErrorContext (750+ lines)
- ✅ **CONCURRENCY_PATTERNS.md** - Thread-safety guidelines, RAII patterns (600+ lines)
- ✅ **RESOURCE_MANAGEMENT.md** - Pin/unpin, lock/unlock, transaction lifecycle patterns (550+ lines)
- ✅ **Total**: 2,600+ lines of comprehensive developer documentation

**Test Infrastructure COMPLETE (Oct 17, 2025)** 🎉
- ✅ **Unit Tests**: Statistics accuracy (475 lines), Page manager destructor (464 lines)
- ✅ **TSAN Tests**: BufferPool race (310 lines), Transaction cache (116 lines), Lock ordering (273 lines)
- ✅ **Helgrind Tests**: Comprehensive race detection (335 lines)
- ✅ **Stress Tests**: Multi-threaded stress testing (410 lines, peak 769k ops/s)
- ✅ **Integration Tests**: Exception injection (479 lines), Buffer pool exhaustion (410 lines)
- ✅ **Total**: 6,000+ lines of comprehensive testing code

**Previous Milestones**
- ✅ **Phase 1 Critical Fixes** - 23/23 audit issues (9 fixed + 14 false positives) (Oct 14, 2025)
- ✅ **Phase 2 Major Fixes** - 19/41 issues resolved (Issues 2.1-2.19) (Oct 14-17, 2025)
- ✅ **Memory Safety** - Lock manager fully RAII-ified with smart pointers
- ✅ **Production-Ready Logging** - Logger framework with categories and levels
- ✅ **Code Quality** - 83 files formatted with clang-format

**Key Documentation**
- [Alpha Issues Tracker](docs/audit/ALPHA_ISSUES_TRACKER.md) - All 21 issues resolved ✅
- [MGA Alpha Status](docs/MGA_ALPHA_STATUS.md) - Complete MGA implementation details
- [CI/CD Guide](docs/CI_CD_GUIDE.md) - Comprehensive testing and quality assurance
- [Audit README](docs/audit/README.md) - Issue tracking and sprint planning

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

1. Review [Alpha Issues Tracker](docs/audit/ALPHA_ISSUES_TRACKER.md) - **All 21 Alpha issues RESOLVED ✅**
2. Review [MGA Alpha Status](docs/MGA_ALPHA_STATUS.md) (updated Oct 16, 2025) - **MGA implementation complete**
3. Review [CI/CD Guide](docs/CI_CD_GUIDE.md) (new Oct 17, 2025) - **Quality assurance infrastructure**
4. Check [TODO.md](docs/development/TODO.md) for prioritized work items
5. Review developer documentation:
   - [Locking Protocol](docs/LOCKING_PROTOCOL.md) - Lock ordering and deadlock prevention
   - [Error Handling Guide](docs/ERROR_HANDLING_GUIDE.md) - Exception patterns and Status codes
   - [Concurrency Patterns](docs/CONCURRENCY_PATTERNS.md) - Thread-safety guidelines
   - [Resource Management](docs/RESOURCE_MANAGEMENT.md) - RAII patterns for resources
6. Follow [Coding Standards](docs/development/CODING_STANDARDS.md)
7. Run sanitizers before commit: `./tools/run_sanitizers.sh --all`
8. Run tests frequently with `ctest --output-on-failure`

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
