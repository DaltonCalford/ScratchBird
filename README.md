# ScratchBird Database Engine

A multi-model database platform using Firebird MGA (Multi-Generational Architecture).

This project started as a refactor of the FirebirdSQL project.  The AI kept getting confused due to older/legacy ways of producing the parser as well as confusing Firebird's MGA vs postgreSQL's way of transaction handling.

So the project was restarted, from first principles and all steps taken are kept in the git history.   This is a total rewrite, using extensive specifications and detailed design goals.

**See [OFFICIAL_ROADMAP.md](OFFICIAL_ROADMAP.md) for complete project scope and development phases.**

## Current Status

**Phase:** Alpha 3 - Network & Service Mode 🚀 **IN PROGRESS**
**Previous:** Alpha 1 & Alpha 2 ✅ **COMPLETE**
**Test Suite:** 1337/1337 tests = 100% pass rate
**Started:** June 2025 (6 months of development)
**Project Type:** Educational/Research (no time constraints)
**Last Updated:** December 11, 2025

### Alpha 3 Progress (Current Phase)

| Component | Status | Description |
|-----------|--------|-------------|
| Network Infrastructure | ✅ Complete | Socket, EventLoop, ThreadPool, ConnectionHandler (~6,200 lines) |
| Wire Protocol Adapters | ✅ Complete | PostgreSQL, MySQL, Firebird, Native (~4,637 lines) |
| Service Mode & systemd | ✅ Complete | Daemon, config parser, PID management (~3,270 lines) |
| Security Suite | ✅ Complete | SSL/TLS, SCRAM-SHA-256/512, certificates, HBA (~3,500 lines) |
| Connection Pooling | 🔜 Pending | Built-in pooling with caching |
| UDR Plugin System | 🔜 Pending | Foreign data wrappers |
| ODBC/JDBC Drivers | 🔜 Pending | Standard connectivity |

**Wire Protocols Implemented:**
| Protocol | Port | Version |
|----------|------|---------|
| PostgreSQL | 5432 | v3 (MD5 auth, Simple/Extended Query) |
| MySQL | 3306 | 5.7+ (native password, prepared statements) |
| Firebird | 3050 | 5.0 (XDR encoding, SRP auth) |
| Native ScratchBird | 3092 | Binary (full message types) |

### Completed Phases

- **Alpha 1:** Core engine, storage, indexes, transactions, parser v1 ✅
- **Alpha 2:** Parser v2.0, multi-dialect support (Firebird, MySQL, PostgreSQL) ✅

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

### Alpha 3 Specifications (NEW - December 2025)

| Specification | Description |
|--------------|-------------|
| [Native Wire Protocol](docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md) | Port 3092, cluster PKI, federation |
| [systemd Service](docs/specifications/SYSTEMD_SERVICE_SPECIFICATION.md) | Service mode, configuration, lifecycle |
| [Connection Pooling](docs/specifications/CONNECTION_POOLING_SPECIFICATION.md) | Pool architecture, caching |
| [Remote Database UDR](docs/specifications/remote_database_udr/) | Foreign data wrappers (9 docs) |
| [Client Library API](docs/specifications/CLIENT_LIBRARY_API_SPECIFICATION.md) | C API (libscratchbird_client) |
| [ODBC Driver](docs/specifications/ODBC_DRIVER_SPECIFICATION.md) | ODBC 3.8 driver + odbc_fdw |
| [JDBC Driver](docs/specifications/JDBC_DRIVER_SPECIFICATION.md) | JDBC 4.3 driver + jdbc_fdw |
| [Live Migration](docs/specifications/LIVE_MIGRATION_PASSTHROUGH_SPECIFICATION.md) | Zero-downtime migration |
| [Migration Guide](docs/MIGRATION_GUIDE.md) | User migration guide |
| [Alpha 3 Test Plan](docs/specifications/ALPHA3_TEST_PLAN.md) | Test suites, security, benchmarks |
| [sb_admin CLI](docs/specifications/SB_ADMIN_CLI_SPECIFICATION.md) | Admin tool, monitoring |
| [Prometheus Metrics](docs/specifications/PROMETHEUS_METRICS_REFERENCE.md) | Metrics, labels, alerts |
| [Git Integration](docs/specifications/GIT_METADATA_INTEGRATION_SPECIFICATION.md) | Schema versioning (Nice to Have) |

### Other Specifications

- **[docs/specifications/](docs/specifications/)** - SQL dialect, DDL, security, indexes, etc.
- **[docs/planning/](docs/planning/)** - Implementation plans and status

## Development Timeline

**Work Completed:** Alpha 1 & Alpha 2 (June-December 2025)
**Current Progress:** ~15% of total project scope
**Current Phase:** Alpha 3 (~27-38 weeks estimated)
**Estimated Remaining:** ~3-3.5 years (single developer, evenings/weekends, AI assistance)

This is an **educational/research project with no fixed deadlines**. Each phase completes when ALL defined features are implemented.

## Project Structure

```
ScratchBird/
├── src/
│   ├── core/          # Storage engine, indexes, transactions, catalog
│   ├── parser/        # SQL parsers (V2, Firebird, MySQL, PostgreSQL)
│   ├── server/        # Server daemon, sessions, IPC
│   ├── protocol/      # Wire protocol implementations
│   └── sblr/          # SBLR bytecode interpreter & query compilers
├── include/           # Public headers
├── tests/
│   ├── unit/          # Unit tests
│   └── integration/   # Integration tests
├── docs/
│   ├── specifications/ # SQL dialect, DDL, NoSQL models, wire protocols
│   ├── planning/       # Implementation roadmaps
│   └── status/         # Completion reports
├── OFFICIAL_ROADMAP.md # Complete project scope
├── PROJECT_CONTEXT.md  # Current work status
└── MGA_RULES.md        # Architecture rules (mandatory)
```

## Project Goals

Planned capabilities:

- Support multiple SQL dialects (PostgreSQL, MySQL, FirebirdSQL) - TDS/MSSQL deferred
- Implement 9 NoSQL models (Graph, Vector, Document, Key-Value, Time-Series, Column-Family, Search, Stream, Object/Blob)
- Enable distributed clustering with heterogeneous databases
- Provide wire protocol compatibility for existing clients
- ODBC/JDBC connectivity for universal database access
- Unified ACID transactions across all data models

## License

See [LICENSE](LICENSE) file.
