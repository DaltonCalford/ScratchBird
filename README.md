# ScratchBird Database Engine

**Firebird-style MGA database engine** with multi-dialect wire compatibility and advanced distributed cluster capabilities.

**Current Phase:** Pre-Beta (Alpha Complete)
**Project Started:** July 2025
**Status:** All 9 Alpha workstreams complete; preparing for Beta

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
| **ScratchBird-driver**      | Language drivers and CLI tools (ODBC/JDBC/Python/Node.js/Go/Rust, sb_admin, sb_isql)   | [GitHub](https://github.com/DaltonCalford/ScratchBird-driver) |
| **ScratchRobin**            | GUI database management and administration tools                                       | [GitHub](https://github.com/DaltonCalford/ScratchRobin)       |

---

## Current Status

### Alpha Scope - Completed

- Core engine (MGA, storage, SBLR runtime) for embedded, IPC, and network use
- Parser remediation and dialect bytecode alignment
- Engine-enforced security; parser/listener treated as untrusted
- Listener/pool/parser/server process operational with socket handoff per dialect
- Shared SBLR cache with per-connection compile caches

Latest full test run: 2470 tests, 6 failures (2026-02-03).

### Beta (Deferred)

- Cluster manager, multi-node coordination, and distributed scheduling
- Backup/ETL orchestration and NoSQL extensions beyond Alpha vectors
- Post-gold protocol expansions (e.g., TDS/MSSQL)

> Drivers and CLI tools have moved to [ScratchBird-driver](https://github.com/DaltonCalford/ScratchBird-driver). GUI tools are in [ScratchRobin](https://github.com/DaltonCalford/ScratchRobin).

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
- **Index Manager** - 14 index types including HNSW/IVF for vector search

### Module Structure

```
src/
├── catalog/        Catalog management and sys.* views
├── client/         Client libraries
├── core/           Core engine, catalog, transactions, scheduler
├── executor/       Query execution
├── fdw/            Foreign data wrappers
├── geo/            Geospatial functions
├── git/            Version control integration
├── index/          Index structures (14 types)
├── network/        Network layer and wire protocols
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

- **ACID Transactions** - Full transaction support with Firebird MGA snapshot isolation
- **14 Index Types** - B-Tree, Hash, GIN, GiST, SP-GiST, BRIN, R-Tree, HNSW, IVF, Bitmap, Columnstore, LSM, Full-Text, Zone Map
- **56 Data Types** - Numeric, string, binary, date/time, spatial (7 geometry types), network (4 types), range (6 types), JSON/JSONB, XML, VECTOR, ARRAY, COMPOSITE, VARIANT
- **24 Window Functions** - ROW_NUMBER, RANK, DENSE_RANK, LAG, LEAD, FIRST_VALUE, LAST_VALUE, NTH_VALUE, NTILE, plus aggregates as windows
- **18 Aggregate Functions** - COUNT, SUM, AVG, MIN, MAX, ARRAY_AGG, STRING_AGG, STDDEV, VAR, CORR, COVAR, regression functions
- **Foreign Data Wrappers** - PostgreSQL, MySQL, Firebird adapters
- **Full-Text Search** - TSVector/TSQuery with GIN-based indexing
- **Spatial/Geographic** - PostGIS-compatible with WKT/WKB, GEOS, PROJ support
- **Vector Search** - HNSW and IVF indexes for AI/ML similarity
- **Stored Procedures** - Procedures, functions, packages, table/database triggers
- **Job Scheduler** - Cron-based scheduling with dependency management
- **Backup/Recovery** - Full, incremental, differential with compression and PITR

### Security Features

- **Authentication** - SCRAM, LDAP, Kerberos, OAuth, SAML, certificate, MFA (7 methods)
- **Authorization** - Role-based access control (RBAC) with granular privileges
- **Row-Level Security (RLS)** - Fine-grained row filtering with forced enforcement
- **Column-Level Security** - Column-level permissions and masking
- **Audit Logging** - 20+ event types (auth, access, DDL, system events)
- **Encryption** - At-rest and in-transit encryption with key management
- **Data Masking** - Built-in domain-level masking
- **Password Policy** - Account locking, password policy enforcement

### Wire Protocol Compatibility

- **Native ScratchBird Protocol** - Port 3092, TLS 1.3
- **Firebird Protocol** - Full wire compatibility
- **PostgreSQL Protocol** - Full wire compatibility
- **MySQL Protocol** - Full wire compatibility

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

Recent full suite: 2470 tests, 6 failures (2026-02-03).

### Test File Breakdown

| Category            | Files    |
| ------------------- | -------- |
| Unit tests          | 71       |
| Integration tests   | 67       |
| Stress tests        | 6        |
| Benchmarks          | 3        |
| SQL compatibility   | 13,303   |
| **Total C++ tests** | **343**  |

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

- **docs/specifications/** - Technical specifications (495 files)
  - **Cluster Specification Work/** - Beta cluster architecture
  - **Security Design Specification/** - Security architecture
  - **beta_requirements/** - Beta requirements tracking
- **docs/planning/** - Implementation plans and workstream trackers (41 files)
- **docs/findings/** - Analysis and investigation reports
- **docs/design/** - Architecture and design documents
- **docs/development/** - Development guides and procedures
- **wiki/** - User-facing documentation (145 pages)

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
├── src/                    Source code (586 files)
├── include/                Public headers (388 files)
├── tests/                  Test suite (343 C++ files + 13,303 SQL files)
├── docs/                   Documentation (1,926 files)
├── wiki/                   User documentation (145 pages)
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

### Alpha Phase - Complete

All 9 Alpha workstreams completed. See `docs/planning/ENGINE_CORE_ALPHA_COMPLETION_PLAN.md` for details.

### Pre-Beta Phase (Current)

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

**Quick Stats (snapshot as of February 2, 2026):**

| Metric                  | Value                             |
| ----------------------- | --------------------------------- |
| Production source files | 586                               |
| Production LOC          | 411,226                           |
| Test C++ files          | 343                               |
| SQL compatibility files | 13,303                            |
| Documentation files     | 1,926                             |
| Wiki pages              | 145                               |
| Git commits             | 1,650                             |
| Commits (last 30 days)  | 92                                |
| CTest results           | 2,470 passed, 6 failed, 0 skipped |

Run `./scripts/generate-all-stats.sh` to regenerate `PROJECT_STATS.md` and related reports.

---

- **Core Repository:** https://github.com/DaltonCalford/ScratchBird
- **Driver Repository:** https://github.com/DaltonCalford/ScratchBird-driver
- **GUI Tools:** https://github.com/DaltonCalford/ScratchRobin

**Last Updated:** February 3, 2026
**Next Milestone:** Pre-Beta audit and verification; driver work at [ScratchBird-driver](https://github.com/DaltonCalford/ScratchBird-driver)
