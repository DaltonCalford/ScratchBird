# ScratchBird Database Engine

An educational relational database engine built from scratch featuring Firebird-style MGA (Multi-Generational Architecture), multi-tablespace support, MVCC with all isolation levels, 6 index types, TOAST storage, and comprehensive transaction management.

## Quick Start

```bash
# Build
mkdir build && cd build
cmake .. && make

# Run tests
ctest --output-on-failure
```

## Current Status

**Version:** Alpha 1.0.4
**Status:** Educational/Development (Core Engine Production-Ready, See Audit Reports)
**Last Updated:** October 24, 2025 (Comprehensive Code Audit Complete)

⚠️ **IMPORTANT**: A comprehensive code audit has been completed. See `/docs/audit/README.md` for details.

### Latest Achievements

**Code Audit Complete (Oct 24, 2025)** 📋
- ✅ **Verified**: Core storage engine (~34,000 lines of production code)
- ✅ **Verified**: Sprint 0 (MGA bug fix), Sprint 1 (Autoextend)
- ⚠️ **Found**: Sprint 4 ~33% complete (only TIDResolver verified)
- ❌ **Found**: Sprint 5 not implemented (MigrationWorker missing)
- ❌ **Found**: Query Processing 0% (all 6 files missing)

**Tablespace Implementation (Partial - Oct 20-23, 2025)**
- ✅ **Phase 1-3**: Core infrastructure, TID migration, SQL DDL, Autoextend
- ✅ **Sprint 0**: MGA bug fix verified in code (storage_engine.cpp:880-1034)
- ✅ **Sprint 1**: Autoextend verified in code (page_manager.cpp:1353-1601)
- ⚠️ **Sprint 4**: TIDResolver only (tid_resolver.h/cpp, 557 lines)
- ❌ **Sprint 5**: Not implemented (migration_worker.h/cpp missing)

**Total Verified Code**: ~34,000 lines production C++ code

### What's Implemented ✅ (Code-Verified)

- **Storage Engine:** Buffer pool, page management, heap pages, TOAST (✅ 100% verified)
- **Transaction Management:** Firebird MGA, 4 isolation levels, sweep, GC (✅ 100% verified)
- **MVCC/MGA:** Back versioning, cross-page support, stable TIDs, N2O chains (✅ 100% verified)
- **Concurrency:** Multi-connection, locking, deadlock detection (✅ 100% verified)
- **Indexing:** B-tree, Hash, GIN, Bitmap, HNSW, BRIN (✅ all 6 types exist, ~13,000 lines)
- **Tablespace:** Core infrastructure, GPID/TID, autoextend (✅ verified, ⚠️ ONLINE migration incomplete)
- **Type System:** Core types, UUIDv7, timezones, collations (✅ files exist, count unverified)
- **Query Processing:** Lexer, parser, AST, semantic analyzer, bytecode, executor (❌ 0% - all files missing)
- **Catalog:** System catalog with metadata persistence (⚠️ 75% - features need verification)
- **Code Quality:** RAII, comprehensive logging, const-correct APIs (✅ verified in audit)
- **CI/CD:** TSAN, ASAN, Helgrind, Valgrind, Clang-Tidy (⚠️ configs not verified)

### Known Limitations ⚠️

- **No Query Processing** (❌ 0% - lexer, parser, AST, semantic, bytecode, executor all missing)
- **Incomplete ONLINE Migration** (❌ Sprint 5 not implemented, ⚠️ Sprint 4 partial)
- **No WAL** (no crash recovery - Beta requirement)
- **No network layer** (local database only - Beta requirement)

### Remaining for Alpha-Ready

**Per Code Audit (Oct 24, 2025)**: ~150-200 hours estimated

**Critical Gaps**:
1. Query Processing (100-150 hours) - If required for Alpha
2. Sprint 5 Migration Worker (26-33 hours) - If ONLINE migration required
3. Sprint 4 completion (20-30 hours) - Verify/implement state mgmt & write routing
4. Component verification (20-30 hours) - Verify uncertain components

**See**: `/docs/audit/ALPHA_COMPLETENESS_ASSESSMENT.md` for detailed analysis

## Project Structure

- **`docs/`** - All documentation
  - `specifications/` - TABLESPACE_SPECIFICATION.md, MGA_IMPLEMENTATION.md
  - `planning/` - TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md
  - `guides/` - Developer guides (locking, error handling, concurrency, resources)
  - `STATUS_*.md` - Sprint completion reports
- **`src/`** - Source code
  - `core/` - Storage engine, indexes, transactions, catalog
  - `parser/` - SQL parser
  - `sblr/` - Query executor
- **`tests/`** - Test suites
  - `unit/` - Unit tests
  - `integration/` - Integration tests
- **`include/`** - Public headers

## Development Process

1. **READ FIRST**: [docs/audit/README.md](docs/audit/README.md) - Code audit findings
2. **READ SECOND**: [docs/audit/CRITICAL_DISCREPANCIES_SUMMARY.md](docs/audit/CRITICAL_DISCREPANCIES_SUMMARY.md) - Critical gaps
3. Review [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md) for current state (updated Oct 24)
4. Review [TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md](docs/planning/TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md) for priorities
5. Check [TODO.md](docs/development/TODO.md) for work items
4. Review developer documentation:
   - [Locking Protocol](docs/LOCKING_PROTOCOL.md)
   - [Error Handling Guide](docs/ERROR_HANDLING_GUIDE.md)
   - [Concurrency Patterns](docs/CONCURRENCY_PATTERNS.md)
   - [Resource Management](docs/RESOURCE_MANAGEMENT.md)
5. Follow [Coding Standards](docs/development/CODING_STANDARDS.md)
6. Run sanitizers before commit: `./tools/run_sanitizers.sh --all`
7. Run tests frequently with `ctest --output-on-failure`

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

# Run sanitizers
./tools/run_sanitizers.sh --all
```

## Architecture Highlights

### Firebird MGA (Multi-Generational Architecture)
- **In-place updates** with back versions (not PostgreSQL's append-only)
- **Stable TIDs** - indexes never updated unless indexed column changes
- **N2O version chains** - Newest-to-Oldest traversal
- **Zero heap fragmentation** by design

### Tablespace System
- **GPID Addressing** - 64-bit (16-bit tablespace + 48-bit page)
- **ONLINE Migration** - Zero-downtime table migration with < 5% overhead
- **Dual-Source Visibility** - TIDResolver with Bloom filters
- **Multi-file Support** - Storage tiering, data lifecycle management

### Index Types
1. **B-Tree** - General purpose, with prefix compression
2. **Hash** - Equality lookups
3. **GIN** - Inverted index for arrays, full-text
4. **Bitmap** - Low-cardinality columns, Roaring compression
5. **HNSW** - Vector similarity search (ANN)
6. **BRIN** - Block range indexes for sequential data

## License

See LICENSE file for details.
