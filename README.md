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
**Last Updated:** October 25, 2025 (Corrected Comprehensive Code Audit Complete)

⚠️ **IMPORTANT**: A comprehensive code audit has been completed with corrections. See `/docs/audit/` for detailed reports.

### Latest Achievements

**Corrected Code Audit Complete (Oct 25, 2025)** 📋
- ✅ **Type System**: 90-95% complete (~3,407 lines) - 29 data types implemented
- ✅ **Index Types**: 95-100% complete (~11,376 lines) - 6 index types fully functional
- ⚠️ **Functions/Operators**: 25-30% complete (~4,458 lines SBLR) - 20 functions + 11 operators
- ✅ **Schema Structure**: 100% complete (~6,432 lines) - Full recursive schema support
- ⚠️ **Parser**: 64% complete (~6,083 lines) - 15 statement types (missing UPDATE/DELETE)
- ❌ **Query Optimizer**: 0% complete (0 lines) - Specification exists, no implementation
- ✅ **Storage Engine**: 100% verified (~34,000 lines) - Full MGA/MVCC implementation

**Previous Audit Error Corrected**:
- ❌ **Oct 24 Audit Claimed**: "Query Processing 0% (all 6 files missing)"
- ✅ **Reality**: Parser subsystem ~6,083 lines + SBLR subsystem ~4,458 lines = ~10,541 lines exist
- The Oct 24 audit only searched `/src/core/` and missed `/src/parser/` and `/src/sblr/` entirely

**Total Verified Code**: ~65,000+ lines production C++ code (corrected from ~34,000)

### What's Implemented ✅ (Code-Verified - Oct 25, 2025)

- **Storage Engine:** Buffer pool, page management, heap pages, TOAST (✅ 100% verified)
- **Transaction Management:** Firebird MGA, 4 isolation levels, sweep, GC (✅ 100% verified)
- **MVCC/MGA:** Back versioning, cross-page support, stable TIDs, N2O chains (✅ 100% verified)
- **Concurrency:** Multi-connection, locking, deadlock detection (✅ 100% verified)
- **Indexing:** B-tree, Hash, GIN, Bitmap, HNSW, BRIN (✅ all 6 types exist, ~11,376 lines verified)
- **Tablespace:** Core infrastructure, GPID/TID, autoextend (✅ verified, ⚠️ ONLINE migration incomplete)
- **Type System:** 29 data types, UUIDv7, timezones, collations (✅ 90-95% complete, ~3,407 lines)
- **Parser:** Lexer, AST, semantic analyzer (⚠️ 64% - ~6,083 lines, missing UPDATE/DELETE)
- **Query Executor:** SBLR bytecode generator + executor (⚠️ 25-30% functions, ~4,458 lines)
- **Query Optimizer:** Cost-based optimization, statistics (❌ 0% - specification only, no code)
- **Schema Catalog:** Recursive schema, 7 catalog structures (✅ 100% complete, ~6,432 lines)
- **Code Quality:** RAII, comprehensive logging, const-correct APIs (✅ verified in audit)
- **CI/CD:** TSAN, ASAN, Helgrind, Valgrind, Clang-Tidy (⚠️ configs not verified)

### Known Limitations ⚠️

**Per Corrected Audit (Oct 25, 2025)**:

- **No Query Optimizer** (❌ 0% - cost model, statistics, plan selection all missing)
- **Incomplete Parser** (⚠️ 64% - missing UPDATE, DELETE for core CRUD)
- **Minimal Functions** (⚠️ 25-30% - only 20 functions vs. ~200+ in production databases)
- **Incomplete ONLINE Migration** (❌ Sprint 5 not implemented, ⚠️ Sprint 4 partial)
- **No WAL** (no crash recovery - Beta requirement)
- **No network layer** (local database only - Beta requirement)

### Remaining for Alpha-Ready

**Per Corrected Code Audit (Oct 25, 2025)**: ~240-380 hours estimated

**Critical Gaps by Priority**:
1. ❌ **Query Optimizer** (100-160 hours) - 0% implemented, specification only
2. ⚠️ **Parser Completion** (35-55 hours) - Add UPDATE/DELETE for core CRUD
3. ⚠️ **Function Library** (150-200 hours) - If comprehensive functions required vs. minimal viable
4. ⚠️ **ONLINE Migration** (26-33 hours) - Sprint 5 MigrationWorker if required

**See Detailed Audit Reports**:
- `/docs/audit/TYPE_SYSTEM_COMPLETENESS_AUDIT.md` - ✅ 90-95% complete
- `/docs/audit/INDEX_TYPE_COMPLETENESS_AUDIT.md` - ✅ 95-100% complete
- `/docs/audit/FUNCTION_COMPLETENESS_AUDIT.md` - ⚠️ 25-30% complete
- `/docs/audit/SCHEMA_STRUCTURE_AUDIT.md` - ✅ 100% complete
- `/docs/audit/PARSER_COVERAGE_AUDIT.md` - ⚠️ 64% complete
- `/docs/audit/QUERY_OPTIMIZATION_AUDIT.md` - ❌ 0% complete

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

1. **READ FIRST**: Review corrected audit reports in `/docs/audit/` (Oct 25, 2025):
   - `TYPE_SYSTEM_COMPLETENESS_AUDIT.md` - ✅ 90-95% complete
   - `INDEX_TYPE_COMPLETENESS_AUDIT.md` - ✅ 95-100% complete
   - `FUNCTION_COMPLETENESS_AUDIT.md` - ⚠️ 25-30% complete
   - `SCHEMA_STRUCTURE_AUDIT.md` - ✅ 100% complete
   - `PARSER_COVERAGE_AUDIT.md` - ⚠️ 64% complete
   - `QUERY_OPTIMIZATION_AUDIT.md` - ❌ 0% complete
2. Review [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md) for current state
3. Review [TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md](docs/planning/TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md) for priorities
4. Check [TODO.md](docs/development/TODO.md) for work items
5. Review developer documentation:
   - [Locking Protocol](docs/LOCKING_PROTOCOL.md)
   - [Error Handling Guide](docs/ERROR_HANDLING_GUIDE.md)
   - [Concurrency Patterns](docs/CONCURRENCY_PATTERNS.md)
   - [Resource Management](docs/RESOURCE_MANAGEMENT.md)
6. Follow [Coding Standards](docs/development/CODING_STANDARDS.md)
7. Run sanitizers before commit: `./tools/run_sanitizers.sh --all`
8. Run tests frequently with `ctest --output-on-failure`

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
