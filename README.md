# ScratchBird Database Engine

An educational relational database engine built from scratch with MVCC transactions, B-tree/hash indexing, TOAST storage, and multi-version concurrency control.

## Quick Start

```bash
# Build
mkdir build && cd build
cmake .. && make

# Run tests
ctest --output-on-failure
```

## Current Status

**Version:** Alpha 1.0.1
**Status:** Educational/Development (Not Production Ready)
**Latest:** See [Current Status](docs/status/CURRENT_STATUS.md) | [Code Audit Report](docs/audits/audit_2025_10_06.md)

### What's Implemented ✅
- **Storage Engine:** Page management, buffer pool, heap pages, TOAST, compression (90%)
- **Transaction Management:** MVCC, MGA, CLOG, XID management, snapshot isolation (85%)
- **Indexing:** B-tree (2,256 lines) and hash indexes (2,254 lines) with scans (80%)
- **Type System:** 30+ data types, UUIDv7, timezones, 100+ collations (95%)
- **Query Processing:** Lexer, parser, AST, semantic analyzer, bytecode, executor (70%)
- **Catalog:** System catalog with metadata persistence (75%)

### Known Limitations ⚠️
- **No multi-connection support** (missing ConnectionContext - CRITICAL)
- **No cross-page UPDATE** (returns NOT_IMPLEMENTED)
- **No WAL** (no crash recovery)
- **Limited SQL** (no JOINs, subqueries, many features)
- **Locking disabled** in 15+ locations pending connection context

### Active Issues 🔧
- Missing thread-local storage for connection context (blocks multi-user)
- Parser missing advanced SQL features (15 test failures)
- Error handling inconsistencies throughout codebase
- See [audit report](docs/audits/audit_2025_10_06.md) for full analysis

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

1. Review [Current Status](docs/status/CURRENT_STATUS.md) and [Code Audit](docs/audits/audit_2025_10_06.md)
2. Check [TODO.md](docs/development/TODO.md) for prioritized work items
3. Follow [Coding Standards](docs/development/CODING_STANDARDS.md)
4. Run tests frequently with `ctest --output-on-failure`
5. See [Build Instructions](docs/development/BUILD_INSTRUCTIONS.md) for details

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
