# ScratchBird Database Engine

A multi-model database platform using Firebird MGA (Multi-Generational Architecture).

This project started as a refactor of the FirebirdSQL project.  The AI kept getting confused due to older/legacy ways of producing the parser as well as confusing Firebird's MGA vs postgreSQL's way of transaction handling.

So the project was restarted, from first principles and all steps taken are kept in the git history.   This is a total rewrite, using extensive specifications and detailed design goals.

**See [OFFICIAL_ROADMAP.md](OFFICIAL_ROADMAP.md) for complete project scope and development phases.**

## Current Status

**Phase:** Alpha 2 - Parser Separation ✅ **IN PROGRESS**
**Progress:** ✅ 100% of Alpha 1 complete (test suite: 1123/1123 = 100% pass rate)
**Current Work:** Parser v2.0 - Context-sensitive "Smart Parser, Dumb Lexer" architecture
**Started:** June 2025 (6 months of evening/weekend development)
**Project Type:** Educational/Research (no time constraints)
**Last Updated:** December 6, 2025

### Recent Completions
- **Parser Audit:** Complete audit of current parser in `/docs/planning/current_parser/` (13 documents)
- **Parser v2.0 Plan:** Full implementation plan at `/docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md`
- **PSQL Dispatch:** CREATE/DROP TRIGGER/FUNCTION/PROCEDURE now fully dispatched

**Detailed Status:** See [IMPLEMENTATION_STATUS_DASHBOARD.md](docs/IMPLEMENTATION_STATUS_DASHBOARD.md) 

## Quick Start

**For detailed build instructions, dependency installation, and troubleshooting, see [BUILD_ENVIRONMENT.md](BUILD_ENVIRONMENT.md).**

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update && sudo apt-get install -y \
    cmake build-essential git python3 pkg-config \
    liblz4-dev libgeos-dev libproj-dev libxml2-dev libssl-dev

# Clone repository
git clone https://github.com/DaltonCalford/ScratchBird.git
cd ScratchBird

# Build
mkdir build && cd build
cmake .. && make -j$(nproc)

# Test
ctest --output-on-failure
```

**See [BUILD_ENVIRONMENT.md](BUILD_ENVIRONMENT.md) for:**

- Platform-specific installation instructions (macOS, Fedora, etc.)
- Optional dependency details
- Development tools setup
- Troubleshooting guide

## MGA Architecture (Firebird Style)

**CRITICAL:** ScratchBird uses **Firebird MGA**, NOT PostgreSQL MVCC.

### Key Principles

- **TIP-based visibility** - Transaction Inventory Pages, O(1) state lookups
- **In-place updates** - Primary record modified, old data in back versions
- **Stable TIDs** - Indexes never change unless indexed column changes
- **No snapshots** - Zero PostgreSQL MVCC contamination

**Before ANY transaction/index work:** Read [MGA_RULES.md](MGA_RULES.md)

```cpp
// ✅ CORRECT - Firebird MGA
if (isVersionVisible(tuple->xmin, current_xid)) { ... }

// ❌ WRONG - PostgreSQL MVCC (forbidden)
if (isSnapshotVisible(tuple, snapshot)) { ... }  // NEVER USE
```

## Documentation

### Essential Reading

- **[BUILD_ENVIRONMENT.md](BUILD_ENVIRONMENT.md)** - Build setup, dependencies, and troubleshooting
- **[OFFICIAL_ROADMAP.md](OFFICIAL_ROADMAP.md)** - Complete project scope and development phases
- **[PROJECT_CONTEXT.md](PROJECT_CONTEXT.md)** - Current work and immediate next steps
- **[MGA_RULES.md](MGA_RULES.md)** - Mandatory MGA architecture rules

### Specifications

- **[docs/specifications/](docs/specifications/)** - SQL dialect, DDL, security, indexes, etc.
- **[docs/planning/](docs/planning/)** - Implementation plans and status

## Development Timeline

**Work Completed:** 5 months (June-November 2025)
**Current Progress:** ~11% of total project scope
**Estimated Remaining:** ~3.5-4 years (single developer, evenings/weekends, AI assistance)

This is an **educational/research project with no fixed deadlines**. Each phase completes when ALL defined features are implemented.

## Project Structure

```
ScratchBird/
├── src/
│   ├── core/          # Storage engine, indexes, transactions, catalog
│   ├── parser/        # SQL parser
│   └── sblr/          # SBLR bytecode interpreter
├── include/           # Public headers
├── tests/
│   ├── unit/          # Unit tests
│   └── integration/   # Integration tests
├── docs/
│   ├── specifications/ # SQL dialect, DDL, NoSQL models
│   ├── planning/       # Implementation roadmaps
│   └── status/         # Completion reports
├── OFFICIAL_ROADMAP.md # Complete project scope
├── PROJECT_CONTEXT.md  # Current work status
└── MGA_RULES.md        # Architecture rules (mandatory)
```

## Project Goals

Planned capabilities:

- Support multiple SQL dialects (PostgreSQL, MySQL, MSSQL, FirebirdSQL)
- Implement 9 NoSQL models (Graph, Vector, Document, Key-Value, Time-Series, Column-Family, Search, Stream, Object/Blob)
- Enable distributed clustering with heterogeneous databases
- Provide wire protocol compatibility for existing clients
- Unified ACID transactions across all data models

## License

See [LICENSE](LICENSE) file.
