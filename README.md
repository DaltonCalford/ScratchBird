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

**Version:** Alpha 1.0.3
**Status:** Educational/Development (Production-Ready Core)
**Last Updated:** October 21, 2025

### Latest Achievements

**Sprint 0, 4, 5, 6: Core MGA + ONLINE Migration (Oct 23, 2025)** 🎉
- ✅ **Sprint 0: CRITICAL Bug Fix** - Cross-page UPDATE MGA compliance, TID stability
- ✅ **Sprint 4: ONLINE Infrastructure** - State management, TIDResolver, write routing
- ✅ **Sprint 5: ONLINE Execution** - Copying, catch-up, atomic swap phases
- ✅ **Sprint 6: ONLINE Polish** - Error handling, rollback, cleanup

**Tablespace Implementation (Oct 20-23, 2025)**
- ✅ **Phase 1**: Core infrastructure - GPID addressing, file management, catalog
- ✅ **Phase 1.5**: TID migration - All 6 index types migrated to TID struct
- ✅ **Phase 2**: SQL DDL - CREATE/DROP/ALTER TABLESPACE
- ✅ **Phase 4**: Migration infrastructure - Parser, executor, batch processing
- ✅ **Phase 5**: OFFLINE migration - All heap pages, 5 index types, TOAST complete

**Total Tablespace Progress**: ~168-193 hours complete

### What's Implemented ✅

- **Storage Engine:** Buffer pool, page management, heap pages, TOAST, LZ4 compression (100%)
- **Transaction Management:** Firebird MGA, 4 isolation levels, sweep, GC (100%)
- **MVCC/MGA:** Back versioning, cross-page support, stable TIDs, N2O chains (100%)
- **Concurrency:** Multi-connection, locking, deadlock detection (100%)
- **Indexing:** B-tree, Hash, GIN, Bitmap, HNSW, BRIN (100%)
- **Tablespace:** Multi-file support, OFFLINE migration, ONLINE migration (95%)
- **Type System:** 30+ data types, UUIDv7, timezones, collations (95%)
- **Query Processing:** Lexer, parser, AST, semantic analyzer, bytecode, executor (72%)
- **Catalog:** System catalog with metadata persistence (75%)
- **Code Quality:** RAII, comprehensive logging, const-correct APIs (98%)
- **CI/CD:** TSAN, ASAN, Helgrind, Valgrind, Clang-Tidy (100%)

### Known Limitations ⚠️

- **No WAL** (no crash recovery - Beta requirement)
- **Limited SQL** (no JOINs, subqueries, advanced features)
- **No network layer** (local database only - Beta requirement)
- **Partial autoextend** (preallocation complete, autoextend remaining)

### Remaining for ALPHA

**Total Remaining**: ~78-120 hours
1. Complete autoextend (12-18 hours)
2. Attach/Detach operations (20-30 hours)
3. Advanced features (50-66 hours)

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

1. Review [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md) for current state
2. Review [TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md](docs/planning/TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md) for priorities
3. Check [TODO.md](docs/development/TODO.md) for work items
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
