# ScratchBird Database Engine

**Firebird-style MGA database engine** with multi-dialect wire compatibility and advanced distributed cluster capabilities.

**Current Phase:** Alpha Engine Core Completion (see `docs/planning/ENGINE_CORE_ALPHA_COMPLETION_PLAN.md`)
**Project Age:** ~6 months (July 2025 - present)
**Status:** Catalog bootstrap complete; tablespace routing in progress; 7 workstreams remaining; see `docs/IMPLEMENTATION_STATUS_DASHBOARD.md`

---

## Quick Overview

ScratchBird is a next-generation database management system that combines:

- **Firebird MGA Architecture** - Multi-Generational Architecture for true MVCC
- **Multi-Dialect Support** - ScratchBird native + Firebird/PostgreSQL/MySQL wire protocol compatibility (emulation layer)
- **Advanced Security** - Built-in encryption, masking, RLS/CLS, cryptographic audit chain
- **Distributed Ready** - Beta cluster specifications drafted; implementation deferred to Beta
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

ScratchBird has been split into multiple repositories to enable proper task segmentation and parallel development:

| Repository                  | Description                                                                            | Link                                                          |
| --------------------------- | -------------------------------------------------------------------------------------- | ------------------------------------------------------------- |
| **ScratchBird** (this repo) | Core database engine - storage, transactions, SBLR runtime, parsers, network layer     | You are here                                                  |
| **ScratchBird-driver**      | Language drivers for client connectivity (ODBC, JDBC, Python, Node.js, Go, Rust, etc.) | [GitHub](https://github.com/DaltonCalford/ScratchBird-driver) |
| **ScratchRobin**            | GUI database management and administration tools                                       | [GitHub](https://github.com/DaltonCalford/ScratchRobin)       |

### Project Separation Rationale

- **ScratchBird (Core Engine):** Focuses on the database kernel - MGA storage engine, catalog, transactions, SBLR bytecode runtime, SQL parsers (native + emulated dialects), wire protocols, and security subsystem.

- **ScratchBird-driver:** Contains all client-side drivers and connectivity libraries. Supports multiple programming languages (C++, C#/.NET, Java/JDBC, Python, Node.js/TypeScript, Go, Rust, PHP, Ruby, R, Pascal/Delphi) and connectivity standards (ODBC). This separation allows driver development to proceed independently once protocol specifications stabilize.

- **ScratchRobin:** A dedicated GUI tool for database administration, query execution, schema management, and monitoring. Separating the GUI allows for independent UI/UX iteration and cross-platform development without impacting core engine work.

This architecture enables parallel development across teams and ensures that driver updates or GUI improvements don't require full engine rebuilds.

---

## Current Status

### ✅ Alpha Scope - Completed

- Core engine (MGA, storage, SBLR runtime) for embedded, IPC, and network use
- Parser remediation and dialect bytecode alignment complete
- Engine-enforced security; parser/listener treated as untrusted
- Listener/pool/parser/server process operational with socket handoff per dialect
- Shared SBLR cache with per-connection compile caches
- **WS-5 Constraint Enforcement** All tests pass

### 🚧 In Progress - Engine Core Alpha Completion

Active work tracked in `docs/planning/ENGINE_CORE_ALPHA_COMPLETION_PLAN.md`:

| Workstream                  | Status         | Description                                                 |
| --------------------------- | -------------- | ----------------------------------------------------------- |
| WS-1 Catalog Bootstrap      | ✅ Done         | Root paths updated; migration/repair pass added             |
| WS-2 Tablespace Routing     | ✅ Done         | GPID wiring, tablespace header v2, file catalog integration |
| WS-3 Index Migration Safety | ✅ Done         | TID updates for all index types                             |
| WS-4 Scheduler/Job System   | ✅ Done         | Secure job runner and scheduling model                      |
| WS-5 Constraint Enforcement | 🚧 In Progress | PK/FK/UNIQUE/CHECK/NOT NULL enforcement                     |
| WS-6 Security Enforcement   | 📋 TODO        | View definer checks, RLS SELECT enforcement                 |
| WS-7 Monitoring Parity      | 📋 TODO        | sys.* views and MON$ sources                                |
| WS-8 Backup/Restore         | 📋 TODO        | Multi-tablespace coverage validation                        |
| WS-9 Cache/Buffer Plan      | 📋 TODO        | Cache and buffer remediation                                |

### 📋 Beta (Deferred)

- Cluster manager, multi-node coordination, and distributed scheduling
- Backup/ETL orchestration and NoSQL extensions beyond Alpha vectors
- Post-gold protocol expansions (e.g., TDS/MSSQL)

> **Note:** Driver development has moved to [ScratchBird-driver](https://github.com/DaltonCalford/ScratchBird-driver) for parallel development. GUI tools are in [ScratchRobin](https://github.com/DaltonCalford/ScratchRobin).

---

## Quick Start

### Build from Source

```bash
# Prerequisites: C++17 compiler, CMake 3.15+, OpenSSL

# Clone repository
git clone https://github.com/scratchbird/scratchbird.git
cd scratchbird

# Build
cmake -S . -B build
cmake --build build

# Run tests
ctest --output-on-failure -C Debug --test-dir build
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
├── core/           Core engine, catalog, transactions
├── sblr/           Bytecode runtime
├── parser/         SQL parsers (4 dialects)
├── optimizer/      Query optimizer
├── security/       Security subsystem
├── server/         Server components
├── odbc/           ODBC driver
├── network/        Network layer
└── [16 other modules]
```

See `PROJECT_STATS.md` for current file and LOC counts.

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

### Detailed Statistics

Run `./scripts/generate-all-stats.sh` to generate current statistics:

- **PROJECT_STATS.md** - Overall project metrics
- **docs/DOCUMENTATION_COVERAGE.md** - Documentation coverage analysis
- **docs/specifications/beta_requirements/COMPLETION_STATUS.md** - Beta requirements tracker

### Documentation Directories

- **docs/specifications/** - Technical specifications (430 files)
  - **Cluster Specification Work/** - Beta cluster architecture (19 files) ✅
  - **Security Design Specification/** - Security architecture (30 specs) ✅
  - **beta_requirements/** - Beta requirements tracking (70 directories)
- **docs/planning/** - Implementation plans (Plans 01-17)
- **docs/design/** - Architecture and design documents
- **docs/findings/** - Analysis and investigation reports
- **docs/development/** - Development guides and procedures
- **wiki/** - User-facing documentation (active; see `wiki/README.md`)

---

## Testing

### Test Suite

```bash
# Run all tests
ctest --test-dir build

# Run specific test categories
ctest --test-dir build -R unit        # Unit test suites
ctest --test-dir build -R integration # Integration test suites
ctest --test-dir build -R compat      # Compatibility tests
```

### Test Statistics

| Category                    | Count | Notes                           |
| --------------------------- | ----- | ------------------------------- |
| **Unit Test Suites**        | 73    | File count (`PROJECT_STATS.md`) |
| **Integration Test Suites** | 67    | File count (`PROJECT_STATS.md`) |
| **Benchmark Suites**        | 2     | File count (`PROJECT_STATS.md`) |
| **Total Test Suites**       | 142   | File count (`PROJECT_STATS.md`) |
| **Test Files (all)**        | 334   | Includes helpers/fixtures       |

Compatibility suites exist for PostgreSQL, MySQL, and Firebird; see `tests/compatibility/` for coverage scope.

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

## Beta Phase - Distributed Cluster

### Status: 📋 **Specifications Complete**

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

### Implementation Phases

1. **Core Cluster** (3-4 months) - Raft, CCE, mTLS, CA
2. **Data Distribution** (3-4 months) - Sharding, distributed query, replication
3. **Operations** (2-3 months) - Backup/restore, scheduler, observability

**Estimated Timeline:** 8-11 months after Alpha completion

See `docs/specifications/Cluster Specification Work/SBCLUSTER-SUMMARY.md` for complete overview.

---

## Development

### Project Structure

```
ScratchBird/
├── src/                    Source code
├── include/                Public headers
├── tests/                  Test suite
├── docs/                   Documentation
├── wiki/                   User documentation (infrastructure ready)
├── scripts/                Automation scripts
├── build/                  Build directory (generated)
├── MGA_RULES.md            MGA architecture rules (CRITICAL)
├── PROJECT_CONTEXT.md      Current work context
├── IMPLEMENTATION_STANDARDS.md  Implementation requirements
└── OFFICIAL_ROADMAP.md     Project roadmap
```

See `PROJECT_STATS.md` for current counts.

### Key Development Documents

- **MGA_RULES.md** - Absolute rules for Firebird MGA implementation (MUST READ)
- **IMPLEMENTATION_STANDARDS.md** - Standards for all implementation work
- **COMPLETION_VERIFICATION_CHECKLIST.md** - Task completion requirements
- **docs/development/AI_CONTEXT_MEMORY_GUIDE.md** - Context management for AI assistants
- **docs/development/AI_PARALLEL_DEVELOPMENT_GUIDE.md** - Parallel development workflows

### Contribution Guidelines

1. Read **MGA_RULES.md** - Non-negotiable architecture rules
2. Read **IMPLEMENTATION_STANDARDS.md** - Implementation requirements
3. Follow the checklist in **COMPLETION_VERIFICATION_CHECKLIST.md**
4. All features must include:
   - Restart/persistence tests
   - Negative/error tests
   - Full code path coverage
   - Documentation updates

### Code Quality

- **Test Status:** See `docs/IMPLEMENTATION_STATUS_DASHBOARD.md` for last run and gating notes
- **Coverage:** See `PROJECT_STATS.md` and `docs/DOCUMENTATION_COVERAGE.md`
- **Code Style:** C++17/20, consistent formatting
- **Documentation:** Comprehensive inline and external documentation
- **Security:** Built-in security by design

---

## Technology Stack

### Core Technologies

- **Language:** C++17/C++20
- **Build System:** CMake 3.15+
- **Testing:** Google Test, CTest
- **Logging:** spdlog
- **Compression:** LZ4
- **Security:** OpenSSL/BoringSSL (TLS 1.3)
- **Threading:** pthreads
- **Spatial:** GEOS, PROJ (optional)

### Standards Compliance

- **SQL Standards:** PostgreSQL, Firebird, MySQL compatibility
- **ODBC:** ODBC 3.x (in progress)
- **MGA Architecture:** Firebird-style Multi-Generational Architecture
- **RFC 2119:** Normative language in specifications
- **OpenTelemetry:** Observability (Beta)
- **TLS 1.3 / mTLS:** Transport security (Beta)

---

## Roadmap

### Alpha Phase (Current - Engine Core Completion)

- ✅ Plan 01: Core Storage & GC
- ✅ Plan 02: UUID Resolution & Rename/Move
- ✅ Plan 03: Security Context/Auth/Audit
- ✅ Plan 03B: Domain Infrastructure
- ✅ Plan 04: Domain DDL (all parsers complete)
- ✅ Listener/pool/parser/server process operational
- ✅ WS-1: Catalog Bootstrap complete
- 🚧 WS-2: Tablespace Routing + GPID Wiring (in progress)
- 📋 WS-3 through WS-9: Index migration, scheduler, constraints, security, monitoring, backup/restore, cache/buffer

See `docs/planning/ENGINE_CORE_ALPHA_COMPLETION_PLAN.md` for detailed workstream tracking.

### Pre-Beta Phase (Next)

After Alpha engine core completion:

- Finalize planning and specification phase
- Audit and verification of Alpha deliverables
- Prepare infrastructure for Beta cluster implementation
- Driver work proceeds in parallel at [ScratchBird-driver](https://github.com/DaltonCalford/ScratchBird-driver)

### Beta Phase (Planned)

**Specifications:** ✅ Complete (see `docs/specifications/Cluster Specification Work/`)

**Implementation:** 8-11 months estimated

- Distributed cluster with Raft consensus
- mTLS authentication and PKI
- Sharding and distributed query
- Replication and failover
- Automated backup/restore
- Distributed scheduler
- OpenTelemetry observability

### GA Phase (Future)

- Production hardening
- Performance tuning
- Additional driver support
- Extended SQL features
- Cloud-native deployment options

---

## Performance

- **Index Types:** 11 specialized indexes for different workloads
- **MVCC:** True multi-version concurrency control (Firebird MGA)
- **Compression:** LZ4 compression for storage efficiency
- **Query Optimizer:** Cost-based optimization with statistics
- **Vector Search:** HNSW index for high-performance similarity search

### Benchmarking

Benchmark tests included in test suite:

```bash
ctest --test-dir build -R benchmark
```

---

## Support & Community

### Getting Help

- **Documentation:** See `docs/` directory for comprehensive documentation
- **Issues:** Report bugs and feature requests on GitHub
- **Wiki:** User documentation at `wiki/` (infrastructure ready)

### Contributing

Contributions welcome! Please:

1. Review **MGA_RULES.md** and **IMPLEMENTATION_STANDARDS.md**
2. Ensure all tests pass
3. Add tests for new features
4. Update documentation
5. Submit pull request with clear description

---

## License

[License information to be added]

---

## Project Statistics

**For detailed, up-to-date statistics, run:**

```bash
./scripts/generate-all-stats.sh
```

**Generated reports:**

- `PROJECT_STATS.md` - Comprehensive project metrics
- `docs/DOCUMENTATION_COVERAGE.md` - Documentation coverage analysis
- `docs/specifications/beta_requirements/COMPLETION_STATUS.md` - Beta requirements tracker

**Quick Stats (snapshot; re-generate for current numbers):**

- 582 C++ source files
- 335 test files (142 test targets)
- 1,830 documentation files
- 1,605 commits (99 in last 30 days)
- 159 wiki pages (67 planned)
- 9 beta P0 requirements + 10 unspecified
- Last full ctest: 2,286 passed, 0 failed, 18 skipped

---

## Contact

- **Project:** ScratchBird Database Engine
- **Phase:** Alpha Engine Core Completion → Pre-Beta
- **Core Repository:** https://github.com/DaltonCalford/ScratchBird
- **Driver Repository:** https://github.com/DaltonCalford/ScratchBird-driver
- **GUI Tools Repository:** https://github.com/DaltonCalford/ScratchRobin
- **Documentation:** `docs/` directory

---

**Last Updated:** January 2026
**Next Milestone:** Complete Engine Core Alpha (WS-2 Tablespace Routing, then WS-3 through WS-9)
**Next Phase:** Pre-Beta preparation, then Beta cluster implementation
**Related Projects:** [ScratchBird-driver](https://github.com/DaltonCalford/ScratchBird-driver) | [ScratchRobin](https://github.com/DaltonCalford/ScratchRobin)
