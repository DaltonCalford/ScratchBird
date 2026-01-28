# ScratchBird Database Engine

**Firebird-style MGA database engine** with multi-dialect wire compatibility and advanced distributed cluster capabilities.

**Current Phase:** Alpha Engine Core Completion (see `docs/planning/ENGINE_CORE_ALPHA_COMPLETION_PLAN.md`)
**Project Started:** July 2025
**Status:** 5 of 9 Alpha workstreams complete; WS-6 Security Enforcement in progress

---

## Quick Overview

ScratchBird is a next-generation database management system that combines:

- **Firebird MGA Architecture** - Multi-Generational Architecture for true MVCC
- **Multi-Dialect Support** - ScratchBird native + Firebird/PostgreSQL/MySQL wire protocol compatibility (emulation layer)
- **Advanced Security** - Built-in encryption, masking, RLS/CLS, cryptographic audit chain
- **Distributed Ready** - Beta cluster specifications complete; implementation deferred to Beta
- **Modern C++** - High-performance C++17/20 implementation

### Key References

| Area                     | Link                                  |
| ------------------------ | ------------------------------------- |
| **Project Metrics**      | `PROJECT_STATS.md`                    |
| **Documentation Index**  | `docs/INDEX.md`                       |
| **Specifications Index** | `docs/specifications/README.md`       |
| **Findings & Plans**     | `docs/findings/` and `docs/planning/` |
| **Audit Outputs**        | `docs/audit/`                         |

---

## Related Projects

ScratchBird has been split into multiple repositories for parallel development:

| Repository                  | Description                                                                            | Link                                                          |
| --------------------------- | -------------------------------------------------------------------------------------- | ------------------------------------------------------------- |
| **ScratchBird** (this repo) | Core database engine - storage, transactions, SBLR runtime, parsers, network layer     | You are here                                                  |
| **ScratchBird-driver**      | Language drivers for client connectivity (ODBC, JDBC, Python, Node.js, Go, Rust, etc.) | [GitHub](https://github.com/DaltonCalford/ScratchBird-driver) |
| **ScratchRobin**            | GUI database management and administration tools                                       | [GitHub](https://github.com/DaltonCalford/ScratchRobin)       |

---

## Current Status

### Alpha Scope - Completed

- Core engine (MGA, storage, SBLR runtime) for embedded, IPC, and network use
- Parser remediation and dialect bytecode alignment
- Engine-enforced security; parser/listener treated as untrusted
- Listener/pool/parser/server process operational with socket handoff per dialect
- Shared SBLR cache with per-connection compile caches

### Engine Core Alpha Workstreams

Active work tracked in `docs/planning/ENGINE_CORE_ALPHA_COMPLETION_PLAN.md`:

| Workstream                  | Status | Description                                                 |
| --------------------------- | ------ | ----------------------------------------------------------- |
| WS-1 Catalog Bootstrap      | Done   | Root paths updated; migration/repair pass added             |
| WS-2 Tablespace Routing     | Done   | GPID wiring, tablespace header v2, file catalog integration |
| WS-3 Index Migration Safety | Done   | TID updates for all index types                             |
| WS-4 Scheduler/Job System   | Done   | Secure job runner, cron scheduling, dependency management   |
| WS-5 Constraint Enforcement | Done   | PK/FK/UNIQUE/CHECK/NOT NULL enforcement                     |
| WS-6 Security Enforcement   | Done   | View definer checks, RLS SELECT enforcement                 |
| WS-7 Monitoring Parity      | Done   | sys.* views and MON$ sources                                |
| WS-8 Backup/Restore         | Done   | Multi-tablespace coverage validation                        |
| WS-9 Cache/Buffer Plan      | Done   | Cache and buffer remediation                                |

Latest full test run: 2396 tests, 0 failures (2026-02-02).

### Beta (Deferred)

- Cluster manager, multi-node coordination, and distributed scheduling
- Backup/ETL orchestration and NoSQL extensions beyond Alpha vectors
- Post-gold protocol expansions (e.g., TDS/MSSQL)

> Driver development has moved to [ScratchBird-driver](https://github.com/DaltonCalford/ScratchBird-driver). GUI tools are in [ScratchRobin](https://github.com/DaltonCalford/ScratchRobin).

---

## Quick Start

### Build from Source

```bash
# Prerequisites: C++17 compiler, CMake 3.15+, OpenSSL

# Clone repository
git clone https://github.com/DaltonCalford/ScratchBird.git
cd ScratchBird

# Build
cmake -S . -B build
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure
```

### Run the Server

```bash
# Start ScratchBird server
./build/bin/sb_server

# Native: 3092 | PostgreSQL: 5432 | MySQL: 3306 | Firebird: 3050
```

---

## Architecture

### Core Components

- **Storage Engine** - Multi-Generational Architecture (MGA) with MVCC
- **SBLR Runtime** - ScratchBird Bytecode Language Runtime
- **SQL Parsers** - 4 dialects: V2 (native), Firebird, PostgreSQL, MySQL
- **Query Optimizer** - Cost-based optimization with push-down support
- **Security Subsystem** - Authentication, authorization, encryption, masking, audit
- **Wire Protocol** - Native ScratchBird + emulated Firebird/PostgreSQL/MySQL protocols
- **Index Manager** - 11 index types including HNSW for vector search

### Module Structure

```
src/
├── catalog/        Catalog management and sys.* views
├── cli/            Command-line tools (sb_admin, sb_isql)
├── client/         Client libraries
├── core/           Core engine, catalog, transactions, scheduler
├── executor/       Query execution
├── fdw/            Foreign data wrappers
├── geo/            Geospatial functions
├── git/            Version control integration
├── index/          Index structures (11 types)
├── network/        Network layer and wire protocols
├── odbc/           ODBC driver
├── optimizer/      Query optimizer
├── parser/         SQL parsers (V2, Firebird, PostgreSQL, MySQL)
├── pool/           Connection and buffer pooling
├── protocol/       Protocol handling
├── sblr/           Bytecode runtime
├── security/       Security subsystem
├── server/         Server components
├── spatial/        Spatial operations
└── testing/        Test utilities
```

---

## Features

### Database Capabilities

- **ACID Transactions** - Full transaction support with snapshot isolation
- **11 Index Types** - B-Tree, Hash, GIN, GiST, SP-GiST, BRIN, R-Tree, HNSW, Bitmap, Columnstore, LSM
- **86 Data Types** - Including complex types (RECORD, VARIANT, ARRAY, GEOMETRY, VECTOR)
- **Advanced Domains** - WITH blocks for SECURITY, INTEGRITY, VALIDATION, QUALITY
- **Foreign Data Wrappers** - Connect to external data sources
- **Full-Text Search** - Built-in text search capabilities
- **Spatial/Geographic** - PostGIS-compatible spatial operations
- **Vector Search** - HNSW index for AI/ML vector similarity
- **Job Scheduler** - Cron-based scheduling with dependency management

### Security Features

- **Authentication** - Password, certificate, and multi-factor authentication
- **Authorization** - Role-based access control (RBAC)
- **Row-Level Security (RLS)** - Fine-grained row filtering
- **Column-Level Security (CLS)** - Column masking and encryption
- **Audit Logging** - Comprehensive audit trail
- **Encryption** - At-rest and in-transit encryption
- **Data Masking** - Built-in domain-level masking

### Wire Protocol Compatibility

- **Native ScratchBird Protocol** - Port 3092, TLS 1.3
- **Firebird Protocol** - Full wire compatibility
- **PostgreSQL Protocol** - Full wire compatibility
- **MySQL Protocol** - Full wire compatibility
- **ODBC** - ODBC 3.x driver (in progress)

---

## Testing

### Test Suite

```bash
# Run all tests
ctest --test-dir build --output-on-failure

# Run with network tests enabled
SCRATCHBIRD_TEST_NETWORK=1 ctest --test-dir build --output-on-failure

# Run specific test categories
ctest --test-dir build -R unit        # Unit tests
ctest --test-dir build -R integration # Integration tests
ctest --test-dir build -R benchmark   # Benchmarks
```

Recent full suite: 2396 tests, 0 failures (2026-02-02).

### Test Results (January 25, 2026)

| Metric    | Value |
| --------- | ----- |
| Tests run | 2,347 |
| Failures  | 0     |
| Skips     | 0     |

### Test File Breakdown

| Category            | Files   |
| ------------------- | ------- |
| Unit tests          | 234     |
| Integration tests   | 69      |
| Stress tests        | 6       |
| Benchmarks          | 2       |
| SQL compatibility   | 12,483  |
| **Total C++ tests** | **337** |

Compatibility suites exist for PostgreSQL, MySQL, Firebird, and ScratchBird native; see `tests/compatibility/`.

---

## Documentation

### Essential Documents

| Document                        | Description                                              |
| ------------------------------- | -------------------------------------------------------- |
| **MGA_RULES.md**                | Firebird MGA architecture rules (CRITICAL - must follow) |
| **OFFICIAL_ROADMAP.md**         | Project roadmap and milestones                           |
| **PROJECT_CONTEXT.md**          | Current work context and status                          |
| **IMPLEMENTATION_STANDARDS.md** | Implementation requirements and standards                |
| **PROJECT_STATS.md**            | Detailed project statistics (auto-generated)             |

### Documentation Directories

- **docs/specifications/** - Technical specifications
  - **Cluster Specification Work/** - Beta cluster architecture
  - **Security Design Specification/** - Security architecture (30 specs)
  - **beta_requirements/** - Beta requirements tracking
- **docs/planning/** - Implementation plans and workstream trackers
- **docs/findings/** - Analysis and investigation reports (12 audits)
- **docs/design/** - Architecture and design documents
- **docs/development/** - Development guides and procedures
- **wiki/** - User-facing documentation (168 pages)

---

## Beta Phase - Distributed Cluster

### Status: Specifications Complete

Complete distributed cluster architecture specified and ready for implementation after Alpha completion.

### Cluster Features (Planned)

- **Raft Consensus** - Leader election, log replication, configuration management
- **mTLS Security** - Mutual TLS for all inter-node communication
- **Certificate Authority** - Full PKI infrastructure
- **Sharding** - Consistent hashing (16-256 shards)
- **Distributed Query** - Scatter-gather with push-down optimization
- **Replication** - Asynchronous WAL streaming (RF=2)
- **Automated Backup** - Per-shard backups with cluster-consistent snapshots
- **Distributed Scheduler** - Cron-like job scheduling
- **Observability** - OpenTelemetry-native metrics and tracing
- **Cryptographic Audit Chain** - Hash-linked, immutable, tamper-evident

See `docs/specifications/Cluster Specification Work/SBCLUSTER-SUMMARY.md` for complete overview.

---

## Development

### Project Structure

```
ScratchBird/
├── src/                    Source code (20 modules, 243 files)
├── include/                Public headers (256 files)
├── tests/                  Test suite (337 C++ files + 12,483 SQL files)
├── docs/                   Documentation (1,652 files)
├── wiki/                   User documentation (168 pages)
├── scripts/                Automation scripts
├── tools/                  Development tools
├── MGA_RULES.md            MGA architecture rules (CRITICAL)
├── PROJECT_CONTEXT.md      Current work context
├── IMPLEMENTATION_STANDARDS.md  Implementation requirements
└── OFFICIAL_ROADMAP.md     Project roadmap
```

### Contribution Guidelines

1. Read **MGA_RULES.md** - Non-negotiable architecture rules
2. Read **IMPLEMENTATION_STANDARDS.md** - Implementation requirements
3. Follow the checklist in **COMPLETION_VERIFICATION_CHECKLIST.md**
4. All features must include:
   - Restart/persistence tests
   - Negative/error tests
   - Full code path coverage
   - Documentation updates

---

## Technology Stack

- **Language:** C++17/C++20
- **Build System:** CMake 3.15+
- **Testing:** Google Test, CTest
- **Logging:** spdlog
- **Compression:** LZ4
- **Security:** OpenSSL/BoringSSL (TLS 1.3)
- **Threading:** pthreads
- **JSON:** nlohmann/json
- **Spatial:** GEOS, PROJ (optional)
- **XML:** libxml2 (optional)

---

## Roadmap

### Alpha Phase (Current)

- ✅ Plans 01-04: Core storage, UUID resolution, security context, domain DDL
- ✅ Listener/pool/parser/server process operational
- ✅ WS-1: Catalog bootstrap
- ✅ WS-2: Tablespace routing + GPID wiring
- ✅ WS-3: Index migration safety (all 11 index types)
- ✅ WS-4: Scheduler/job system
- ✅ WS-5: Constraint enforcement (PK/FK/UNIQUE/CHECK/NOT NULL)
- ✅ WS-9: Cache/buffer remediation
- ✅ WS-6: Security enforcement (view definer, RLS SELECT)
- ✅ WS-7: Monitoring parity (sys.* views, MON$ sources)
- ✅ WS-8: Backup/restore multi-tablespace validation

See `docs/planning/ENGINE_CORE_ALPHA_COMPLETION_PLAN.md` for detailed tracking.

### Pre-Beta Phase (Next)

- Audit and verification of Alpha deliverables
- Prepare infrastructure for Beta cluster implementation
- Driver work proceeds in parallel at [ScratchBird-driver](https://github.com/DaltonCalford/ScratchBird-driver)

### Beta Phase (Planned)

Specifications complete (see `docs/specifications/Cluster Specification Work/`):

- Distributed cluster with Raft consensus
- mTLS authentication and PKI
- Sharding and distributed query
- Replication and failover
- Automated backup/restore
- Distributed scheduler
- OpenTelemetry observability

### GA Phase (Future)

- Production hardening and performance tuning
- Extended SQL features
- Cloud-native deployment options

---

## License

Licensed under the [Initial Developer's Public License Version 1.0 (IPL 1.0)](https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/).

---

## Project Statistics

**Quick Stats (snapshot as of January 25, 2026):**

| Metric                  | Value                             |
| ----------------------- | --------------------------------- |
| Production source files | 499                               |
| Production LOC          | 424,686                           |
| Test C++ files          | 337                               |
| Test LOC                | 121,824                           |
| SQL compatibility files | 12,483                            |
| Total LOC (all C++)     | 546,510                           |
| Documentation files     | 1,652                             |
| Wiki pages              | 168                               |
| Git commits             | 1,618                             |
| Commits (last 30 days)  | 111                               |
| CTest results           | 2,347 passed, 0 failed, 0 skipped |

Run `./scripts/generate-all-stats.sh` to regenerate `PROJECT_STATS.md` and related reports.

---

- **Core Repository:** https://github.com/DaltonCalford/ScratchBird
- **Driver Repository:** https://github.com/DaltonCalford/ScratchBird-driver
- **GUI Tools:** https://github.com/DaltonCalford/ScratchRobin

**Last Updated:** January 25, 2026
**Next Milestone:** Complete remaining Alpha workstreams (WS-6 Security, WS-7 Monitoring, WS-8 Backup, WS-9 Cache)
