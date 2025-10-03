# ScratchBird Database Engine

A modern relational database engine built from scratch with MVCC transactions, flexible indexing, and clean architecture.

## Quick Start

```bash
# Build
mkdir build && cd build
cmake .. && make

# Run tests
ctest --output-on-failure
```

## Current Status

**Version:** Alpha 1.0.1 (75% complete)
**Latest:** See [Overall Project Status](docs/status/OVERALL_PROJECT_STATUS.md)

### Recent Milestones ✅
- B-Tree index complete (2,256 lines) - Range scans, compression, vacuum
- Hash index complete (2,254 lines) - All tests passing
- MVCC/MGA complete (~1,800 lines) - Full transaction support with CLOG
- Storage engine production-ready (~3,500 lines)

### Active Work 🔧
- Fixing database initialization hang
- Resolving executor compilation errors
- Enabling comprehensive test suite

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

1. Review the `IMPLEMENTATION_PLAN.md` in the `project/plan` directory.
2. Begin implementation of the next Alpha phase.
3. Progress is tracked in the `project/progress` directory.
4. Tests are created alongside implementation.

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
