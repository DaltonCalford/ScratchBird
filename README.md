# ScratchBird Database Engine

**Firebird-style MGA database engine** with multi-dialect wire compatibility and advanced distributed cluster capabilities.

**Current Phase:** ✅ **Alpha Complete** - Ready for Beta
**Project Started:** July 2025  
**Status:** All Alpha workstreams complete; 3,600+ tests passing (99.8%)

---

### **Note to new visitors**

ScratchBird Alpha is **complete and fully functional**. All 84+ NOT_IMPLEMENTED stubs have been implemented across the core engine. The code is ready to be built and tested.

If you are curious, clone the directories and have your friendly local AI analyze the code base - tell it to find out the capabilities of the project from the implemented source code. This will give you a good understanding of what is done.

The initial preview will be a Docker container with the database engine and an AppImage or standalone executable so that you can test the project without any problems of getting rid of it afterward.

This project has become my answer to the constant "Damn I wish I had the ability to...." issues I have encountered over 35 years of database use.

I have been seeing multiple clones of my project(s) via the tracker but I have not received any feedback yet - don't be afraid, I need feedback and I don't bite.

I am sure there are things others have encountered over the years and wish they had a tool to cover it.

Thanks for your interest in the project.

---

## Quick Overview

ScratchBird is a next-generation database management system that combines:

- **Firebird MGA Architecture** - Multi-Generational Architecture for true MVCC
- **Multi-Dialect Support** - ScratchBird native + Firebird/PostgreSQL/MySQL wire protocol compatibility (full protocol implementation)
- **Advanced Security** - Built-in encryption, masking, RLS/CLS, cryptographic audit chain, SCRAM-SHA-256/512 authentication
- **Distributed Ready** - Beta cluster specifications complete; implementation deferred to Beta
- **Modern C++** - High-performance C++17/20 implementation

### Key References

| Area                     | Link                                  |
| ------------------------ | ------------------------------------- |
| **Alpha Completion Report** | `ALPHA_COMPLETION_REPORT.md`       |
| **Project Metrics**      | `PROJECT_STATS.md`                    |
| **Documentation Index**  | `docs/INDEX.md`                       |
| **Specifications Index** | `docs/specifications/README.md`       |
| **Findings & Plans**     | `docs/findings/` and `docs/planning/` |
| **Release Targets**      | `docs/planning/RELEASE_TARGETS.md`    |
| **Feature Catalog**      | `docs/FEATURE_CATALOG.md`             |

---

## Related Projects

ScratchBird has been split into multiple repositories for parallel development:

| Repository                  | Description                                                                            | Link                                                          |
| --------------------------- | -------------------------------------------------------------------------------------- | ------------------------------------------------------------- |
| **ScratchBird** (this repo) | Core database engine - storage, transactions, SBLR runtime, parsers, network layer     | You are here                                                  |
| **ScratchBird-driver**      | Language drivers and CLI tools (ODBC/JDBC/Python/Node.js/Go/Rust, sb_admin, sb_isql)   | [GitHub](https://github.com/DaltonCalford/ScratchBird-driver) |
| **ScratchRobin**            | GUI database management and administration tools                                       | [GitHub](https://github.com/DaltonCalford/ScratchRobin)       |

---

## Alpha Completion Status ✅

### Alpha Scope - Complete

All 9 Alpha workstreams have been completed with **19,400+ lines of code** across **73 files**:

| Component | Status | Lines Added |
|-----------|--------|-------------|
| EngineIPCSessionHandler | ✅ Complete | ~3,200 |
| PostgreSQL Parser Agent | ✅ Complete | ~2,800 |
| MySQL Parser Agent | ✅ Complete | ~2,600 |
| Firebird Parser Agent | ✅ Complete | ~2,400 |
| SCRAM-SHA-256/512 Auth | ✅ Complete | ~1,800 |
| Type Mapping System | ✅ Complete | ~2,200 |
| COPY Flow Control | ✅ Complete | ~1,300 |
| Schema Introspection | ✅ Complete | ~1,300 |
| UnixSocketIPCChannel | ✅ Complete | ~1,400 |
| UDR Connectors (69 stubs) | ✅ Complete | ~690 |

**Test Results:** 3,600+ tests, 3,593 passing (99.8% pass rate)

### Features Delivered in Alpha

- ✅ Full wire protocol support: PostgreSQL 3.0, MySQL 4.1+, Firebird XDR
- ✅ SCRAM-SHA-256/512 authentication (RFC 5802/7677 compliant)
- ✅ Complete type mapping (140+ type conversions)
- ✅ Session management with LRU statement cache
- ✅ COPY protocol with credit-based flow control
- ✅ Schema introspection (pg_catalog, information_schema, RDB$ views)
- ✅ Multi-transport IPC (Unix socket, TCP loopback)

### Beta (Planned)

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

### Test Server (for Development)

For driver development, GUI testing, and security validation:

```bash
# Setup test server
./scripts/test-server-user.sh setup

# Start test server
./scripts/test-server-user.sh start

# Connect (bootstrap mode - any user/pass)
scratchbird://anyuser:anypass@127.0.0.1:3092/testdb

# View status
./scripts/test-server-user.sh status
```

**Documentation:**
- [Test Server Specification](docs/specifications/testing/test_server/README.md)
- [Operations Guide](docs/specifications/testing/test_server/OPERATIONS.md)
- [Security Testing](docs/specifications/testing/test_server/SECURITY_TESTING.md)

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

### Three-Tier Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      CLIENT APPLICATIONS                                 │
├─────────────────────────────────────────────────────────────────────────┤
│  PostgreSQL Client   │   MySQL Client   │   Firebird Client   │  ...   │
└──────────────────────┴──────────────────┴─────────────────────┴─────────┘
                            │           │           │
                            ▼           ▼           ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    PARSER AGENTS (Wire Protocols)                        │
├─────────────────────────────────────────────────────────────────────────┤
│  PostgreSQL Parser   │   MySQL Parser   │   Firebird Parser   │  ...   │
│  (Wire Protocol 3.0) │  (Protocol 4.1+) │   (XDR Protocol)    │        │
└──────────────────────┴──────────────────┴─────────────────────┴─────────┘
                            │           │           │
                            ▼           ▼           ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    SBWP (ScratchBird Wire Protocol)                      │
│                         Standardized Message Format                      │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         IPC CHANNEL (Unix Socket)                        │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    ENGINE IPC SESSION HANDLER                            │
│  • LRU Statement Cache  • Transaction Management  • COPY Flow Control   │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         SCRATCHBIRD ENGINE                               │
│  • SBLR Execution  • MVCC  • Storage  • Indexing  • Security            │
└─────────────────────────────────────────────────────────────────────────┘
```

### Module Structure

```
src/
├── catalog/        Catalog management and sys.* views
├── client/         Client libraries (parser agents)
├── core/           Core engine, catalog, transactions, scheduler
├── executor/       Query execution
├── fdw/            Foreign data wrappers
├── geo/            Geospatial functions
├── git/            Version control integration
├── index/          Index structures (14 types)
├── ipc/            IPC infrastructure
├── network/        Network layer and wire protocols
├── optimizer/      Query optimizer
├── parser/         SQL parsers (V2, Firebird, PostgreSQL, MySQL)
├── pool/           Connection and buffer pooling
├── protocol/       Protocol handling
├── sblr/           Bytecode runtime
├── security/       Security subsystem
├── server/         Server components
├── spatial/        Spatial operations
├── types/          Type mapping system
└── udr/            Universal Database Route connectors
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

- **Authentication** - SCRAM-SHA-256/512, LDAP, Kerberos, OAuth, SAML, certificate, MFA (7 methods)
- **Authorization** - Role-based access control (RBAC) with granular privileges
- **Row-Level Security (RLS)** - Fine-grained row filtering with forced enforcement
- **Column-Level Security** - Column-level permissions and masking
- **Audit Logging** - 20+ event types (auth, access, DDL, system events)
- **Encryption** - At-rest and in-transit encryption with key management
- **Data Masking** - Built-in domain-level masking
- **Password Policy** - Account locking, password policy enforcement

### Wire Protocol Compatibility

| Protocol | Port | Status | Features |
|----------|------|--------|----------|
| Native ScratchBird | 3092 | ✅ Complete | TLS 1.3, SBWP v1.1 |
| PostgreSQL | 5432 | ✅ Complete | Wire Protocol 3.0, SSL, SCRAM |
| MySQL | 3306 | ✅ Complete | Protocol 4.1+, TLS, prepared statements |
| Firebird | 3050 | ✅ Complete | XDR Protocol, SRP auth, BLOBs |

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

# Run Alpha component tests
ctest --test-dir build -R "EngineIPCSessionHandler|PostgreSQLParserAgent|MySQLParserAgent|FirebirdParserAgent|SCRAMAuth|TypeMapping|SchemaIntrospection|COPYFlowControl|UnixSocketChannel"
```

**Latest Test Run:** 3,600+ tests, 99.8% pass rate (2026-02-06)

### Test Assets

| Asset type               | Count  | Notes |
| ------------------------ | ------ | ----- |
| C++ test source files    | 352    | `tests/` tree compiled into CTest binaries |
| SQL compatibility scripts| 13,303 | Executed by compatibility harnesses |
| Alpha component tests    | 9      | 670+ test cases for Alpha completion |

Compatibility suites exist for PostgreSQL, MySQL, Firebird, and ScratchBird native; see `tests/compatibility/`.

---

## Documentation

### Essential Documents

| Document                        | Description                                              |
| ------------------------------- | -------------------------------------------------------- |
| **ALPHA_COMPLETION_REPORT.md**  | Detailed Alpha completion report                         |
| **ALPHA_COMPLETION_SUMMARY_2026-02-06.md** | Alpha implementation summary                |
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

Complete distributed cluster architecture specified and ready for implementation.

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
├── tests/                  Test suite (352 C++ files + 13,303 SQL files)
├── docs/                   Documentation (1,926 files)
├── wiki/                   User documentation (145 pages)
├── scripts/                Automation scripts
├── tools/                  Development tools
├── MGA_RULES.md            MGA architecture rules (CRITICAL)
├── PROJECT_CONTEXT.md      Current work context
├── IMPLEMENTATION_STANDARDS.md  Implementation requirements
├── OFFICIAL_ROADMAP.md     Project roadmap
└── ALPHA_COMPLETION_REPORT.md  Alpha completion details
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

### Alpha Phase - ✅ COMPLETE

All 9 Alpha workstreams completed with 19,400+ lines of code:
- EngineIPCSessionHandler with LRU cache
- PostgreSQL/MySQL/Firebird Parser Agents (full protocol support)
- SCRAM-SHA-256/512 authentication
- Type Mapping (140+ conversions)
- COPY Flow Control
- Schema Introspection
- UnixSocketIPCChannel
- UDR Connector stubs (all 69 implemented)

### Pre-Beta Phase (Current)

- Integration testing with real database clients
- Performance benchmarking
- Infrastructure preparation for Beta cluster implementation
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

**Quick Stats (snapshot as of February 6, 2026):**

| Metric                  | Value                             |
| ----------------------- | --------------------------------- |
| Production source files | 586                               |
| Production LOC          | 411,226                           |
| Alpha completion LOC    | ~19,400                           |
| Test C++ files          | 352                               |
| SQL compatibility files | 13,303                            |
| Documentation files     | 1,926                             |
| Wiki pages              | 145                               |
| Git commits             | 1,650+                            |
| Commits (last 30 days)  | 92                                |
| CTest results           | 3,600+ passed, 99.8% pass rate    |

Run `./scripts/generate-all-stats.sh` to regenerate `PROJECT_STATS.md` and related reports.

---

- **Core Repository:** https://github.com/DaltonCalford/ScratchBird
- **Driver Repository:** https://github.com/DaltonCalford/ScratchBird-driver
- **GUI Tools:** https://github.com/DaltonCalford/ScratchRobin

**Last Updated:** February 6, 2026  
**Status:** ✅ Alpha Complete - 19,400+ lines, 84+ stubs implemented, 3,600+ tests passing  
**Next Milestone:** Pre-Beta integration testing and benchmarking
