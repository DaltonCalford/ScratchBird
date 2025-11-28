# ScratchBird Database Engine

A multi-model database platform using Firebird MGA (Multi-Generational Architecture).

**See [OFFICIAL_ROADMAP.md](OFFICIAL_ROADMAP.md) for complete project scope and development phases.**

## Current Status

**Phase:** Alpha 1 - Engine Functionality (Local Operations)
**Progress:** ~97% of Alpha 1 complete (~3% of total project remaining)
**Remaining:** ~100-130 hours (3-4 weeks for Local Server Phase 5 + CLI Tools)
**Current Work:** ✅ Functions → ✅ P0-P3 → ✅ Catalog Cleanup → ✅ Phase 1-4 → 🔄 Phase 5 Server → CLI Tools
**Started:** June 2025 (5 months of evening/weekend development)
**Project Type:** Educational/Research (no time constraints)
**Last Updated:** November 27, 2025 (Phase 4 Client Library ~85% complete, 95 server tests passing)

**Detailed Status:** See [IMPLEMENTATION_STATUS_DASHBOARD.md](docs/IMPLEMENTATION_STATUS_DASHBOARD.md)

### What's Working ✅

#### Core Engine (100%)
- **MGA Architecture** - Firebird-style Multi-Generational Architecture with TIP-based visibility
- **Buffer Pool & Pages** - LRU caching, heap pages with back-versioning
- **TOAST** - Large object storage with MGA compliance
- **Transactions** - 4 isolation levels, deadlock detection, O(1) state lookups
- **Tablespaces** - Multi-file support with GPID addressing

#### Indexes (11/11 = 100%) 🎉
- B-Tree, Hash, R-Tree, GIN, Bitmap
- GiST, HNSW (vector), SP-GiST, BRIN
- LSM-Tree, Columnstore
- **Note:** "Production-ready" refers to component stability in development, not deployment readiness

#### Data Types (86/86 = 100%) 🎉
- Numeric, String, Temporal, Binary, Spatial
- JSON/JSONB, XML, UUID, ARRAY, RANGE, VECTOR
- Network types (INET, CIDR, MACADDR)
- Text search types (TSVECTOR, TSQUERY)

#### Built-in Functions (153/153 = 100%) 🎉
**Current:** 153 functions implemented
**Target:** 153 functions ✅ **COMPLETE** - Full PostgreSQL/MySQL/MSSQL/Firebird compatibility achieved!

**Implemented Categories:**
- String (14), Aggregate (15), Window (17)
- JSON (13), Array (12), Date/Time (7)
- Mathematical (36), Bit Manipulation (14)
- Cryptographic (4), Statistical (16), XML (9)
- Spatial (40+), Regex (4), Conditional (3)

**Recently Added (November 2025):**
- Advanced Grouping: ROLLUP, CUBE, GROUPING SETS, GROUPING() ✅
- Regression Functions: REGR_SLOPE, REGR_INTERCEPT, REGR_R2, REGR_COUNT, REGR_AVGX, REGR_AVGY, REGR_SXX, REGR_SYY, REGR_SXY ✅
- Hyperbolic Math: SINH, COSH, TANH, ASINH, ACOSH, ATANH, COT ✅
- String: LPAD, RPAD, OVERLAY, INITCAP ✅
- Window: NTH_VALUE, CUME_DIST, PERCENT_RANK ✅
- Date/Time: AGE ✅
- Math: CBRT ✅

**See [MISSING_FUNCTIONS_IMPLEMENTATION_STATUS.md](docs/planning/MISSING_FUNCTIONS_IMPLEMENTATION_STATUS.md) for implementation details.**

#### Security System (100%) 🎉
- User/role/group management with transitive membership
- Table-level, column-level, and row-level permissions
- Row-Level Security (RLS) with policy-based filtering
- SQL SECURITY DEFINER/INVOKER
- Password hashing (BCrypt)
- Permission cache with LRU eviction

#### Catalog System (40 tables) - ALL PHASES COMPLETE 🎉
- 18-level schema hierarchy with synonym support
- UUIDv7 identifiers (RFC 9562)
- 32+ object types (extended with FDW, UDR modules, Server Registry)
- Full CRUD for all catalog structures
- **Phase A:** 37 CRUD methods (dropSchema, Domain/UDR/Package/Emulation)
- **Phase B:** 46 method declarations, 11 new structures/enums
- **Phase C:** 7 new system table pages allocated
- **Phase D:** Virtual catalog infrastructure (~4,290 lines)
  - information_schema (12 SQL standard views)
  - pg_catalog (12 PostgreSQL views)
  - mysql.* (6 MySQL tables)
  - sys.* (8 SQL Server views)
  - On-demand Firebird RDB$* emulation

#### PSQL/Stored Procedures & Triggers (100%) 🎉
- Variable scope management and operations
- Control flow (IF, LOOP, WHILE, EXIT, RETURN)
- Exception handling (RAISE, TRY/EXCEPT)
- Cursor operations (DECLARE, OPEN, FETCH, CLOSE)
- Trigger firing mechanism (BEFORE/AFTER, FOR EACH ROW)
- Stored procedure/function invocation with OUT/INOUT parameters

#### Advanced SQL Features (100%) 🎉
- Common Table Expressions (CTEs) - both non-recursive and recursive
- Set operations (UNION, UNION ALL, INTERSECT, INTERSECT ALL, EXCEPT, EXCEPT ALL)
- MERGE statement (all 3 WHEN clause types)
- RETURNING clause (INSERT/UPDATE/DELETE)
- SAVEPOINT (nested transaction control)

#### Constraint Features (100%) 🎉
- NOT NULL, UNIQUE, PRIMARY KEY, FOREIGN KEY, CHECK, DEFAULT
- GENERATED columns (STORED/VIRTUAL) with expression evaluation
- IDENTITY columns (GENERATED ALWAYS/BY DEFAULT AS IDENTITY)
- Deferred constraint checking (DEFERRABLE, INITIALLY DEFERRED)
- Referential actions (CASCADE, SET NULL, SET DEFAULT)

#### SQL Engine Commands (100%) 🎉
- SHOW TABLES, SHOW DATABASES, SHOW COLUMNS, SHOW INDEXES, SHOW CREATE TABLE
- DESCRIBE/DESC table introspection
- EXPLAIN query plan visualization

### What's Being Built 🚧

**Current Work (Alpha 1 - ~5% remaining, ~110-140 hours):**

- ✅ **Built-in Functions** ✅ **COMPLETE** (153/153) - See [archive](docs/planning/archive/)
- ✅ **P0-P2 Improvements** ✅ **ALL COMPLETE** (48/48 items) - See [archive](docs/planning/archive/)
- ✅ **P3 Low-Priority** 🔄 **70% COMPLETE** (14/20 items, 6 blocked by Alpha 3/dependencies)
- ✅ **Catalog Cleanup** ✅ **ALL PHASES COMPLETE** (Phases A-D, ~4,290 lines)
- ✅ **Local Server Phase 1** ✅ **COMPLETE** - IPC Infrastructure (22 tests passing)
- ✅ **Local Server Phase 2** ✅ **COMPLETE** - Wire Protocol (37 tests passing)
- ✅ **Local Server Phase 3** ✅ **COMPLETE** - sb_server process (session management, query execution)
- ✅ **Local Server Phase 4** ✅ **~85% COMPLETE** - Client Library (36 unit + 6 integration tests)

- 🔄 **NEXT: Local Server Architecture Phase 5** (10-20 hours)
  - **Phase 5:** Integration testing, performance benchmarks, documentation
  - **Note:** Server database initialization needs fixes for full integration testing
  - **Mandatory before CLI tools can function**
  - See [LOCAL_SERVER_ARCHITECTURE_PLAN.md](docs/planning/LOCAL_SERVER_ARCHITECTURE_PLAN.md)

- ⏳ **Command-Line Tools** (90-110 hours / 2.5-3 weeks)
  - sb_isql (interactive SQL shell)
  - sb_verify (database integrity checker)
  - sb_backup (backup/restore tool)
  - sb_security (user/role management tool)
  - *To be started after server architecture*

**Recently Completed (November 27, 2025):**
- ✅ **Local Server Architecture Phase 4: Client Library** 🎉
  - `include/scratchbird/client/connection.h` - Connection, ResultSet, PreparedStatement, ConnectionPool
  - `src/client/connection.cpp` - Full client implementation (~1,200 lines)
  - `tests/unit/test_client_connection.cpp` - 36 unit tests (all passing)
  - `tests/integration/test_client_server_integration.cpp` - 6 integration tests
  - Connection lifecycle, query execution, transactions, prepared statements
  - Connection pooling with acquire/release, auto-start mechanism
  - **Total Server Tests: 95 passing** (22 IPC + 37 wire protocol + 36 client)
- ✅ **Audit Report Issues FIXED** 🎉
  - MGA cross-page back version GPID bug fixed (heap_page.cpp)
  - Transaction visibility issues fixed (XID initialization, CLOG marking)
  - CatalogManager::createIndex deadlock fixed
  - IPC virtual destructor issue fixed (all concrete classes marked `final`)
  - Printf format string bugs fixed (test_columnstore_rle.cpp)
  - I/O return value checking fixed (test_page_management_edge_cases.cpp)
  - All 4 StorageEngineMGATest tests passing
- ✅ **Local Server Architecture Phase 3: Server Implementation** 🎉
  - `include/scratchbird/server/server_session.h` - Session management header
  - `include/scratchbird/server/scratchbird_server.h` - Main server class
  - `src/server/server_session.cpp` - Session and SessionManager implementation
  - `src/server/scratchbird_server.cpp` - Server lifecycle, accept loop, client handling
  - `src/server/sb_server_main.cpp` - Server executable with CLI argument parsing
  - Multi-threaded client handling, graceful shutdown, PID file management
  - Signal handling (SIGTERM, SIGINT, SIGHUP), query execution pipeline
- ✅ **Local Server Architecture Phase 2: Wire Protocol** 🎉
  - `include/scratchbird/protocol/wire_protocol.h` - Protocol definitions (~750 lines)
  - `src/protocol/wire_protocol.cpp` - Full implementation (~1,000 lines)
  - `tests/unit/test_wire_protocol.cpp` - 37 unit tests (all passing)
  - 12-byte message header, 22 message types, UUID v4 sessions
  - Result streaming, transaction messages, ping/pong keepalive
- ✅ **Local Server Architecture Phase 1: IPC Infrastructure** 🎉
  - Unix domain sockets (Linux/macOS), Named pipes (Windows), TCP fallback
  - Peer credential retrieval, connection statistics, server detection
  - 22 unit tests (all passing)
- ✅ **Catalog Cleanup ALL PHASES** 🎉 - See [docs/planning/README.md](docs/planning/README.md)
- ✅ **P3 Low-Priority** (14/20 = 70% COMPLETE)
- ✅ **P0-P2 Improvements** (48/48 = 100% COMPLETE)
- ✅ **Built-in Functions** (153/153 = 100% COMPLETE)
- ✅ Views (100% COMPLETE - materialized views with full data population)

**After Alpha 1 (~88% of project remaining):**

See **[OFFICIAL_ROADMAP.md](OFFICIAL_ROADMAP.md)** for comprehensive details:

- **Alpha 2:** Parser separation + 5 SQL dialects (ScratchBird, PostgreSQL, MySQL, MSSQL, FirebirdSQL)
- **Alpha 3:** Network listeners + 4 wire protocols
- **Beta 1-3:** Distributed clustering + encryption + advanced indexes
- **Beta 4:** 9 NoSQL models (Graph, Vector, Document, Key-Value, Time-Series, Column-Family, Search, Stream, Object/Blob)
- **RC1:** 12 native language drivers (ODBC, JDBC, C++, C, C#, Rust, Pascal, Python, Go, Node.js, Ruby, PHP)
- **Gold:** Production release

## Project Scope Highlights

### Multi-Dialect SQL Support (Alpha 2-3)
- **5 SQL dialects:** ScratchBird, PostgreSQL, MySQL, MSSQL, FirebirdSQL
- **Native wire protocol compatibility** for existing clients
- **Pluggable parser architecture** - separate parsers target single SBLR bytecode engine

### Multi-Model NoSQL Support (Beta 4)
**9 NoSQL models, each with dedicated query dialects:**
1. **Graph Database** - Cypher (Neo4j), Gremlin (TinkerPop), ScratchBird native
2. **Vector Database** - Similarity search, k-NN, ANN queries
3. **Document Store** - MongoDB-compatible JSON operations
4. **Key-Value Store** - Redis-compatible atomic operations
5. **Time-Series Database** - Temporal queries, retention policies
6. **Column-Family Store** - Cassandra-compatible CQL
7. **Full-Text Search** - Elasticsearch-compatible DSL
8. **Stream Processing** - Continuous queries, event time semantics
9. **Object/Blob Store** - S3-compatible API

### Distributed Systems (Beta 1-3)
- Horizontal scaling with automatic sharding
- Heterogeneous clusters (ScratchBird + PostgreSQL + MySQL + MSSQL + Firebird)
- Distributed transactions (2PC, XA)
- Query federation across database types
- Encryption (field-level, database-level, key management)

### Integration Ecosystem (Beta 4)
- Kafka event streaming (CDC, ingestion)
- Message queues (RabbitMQ, Redis Pub/Sub)
- AI/automation agent APIs (REST, GraphQL)
- Object storage (S3-compatible)
- Observability (Prometheus, Grafana, OpenTelemetry)

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
