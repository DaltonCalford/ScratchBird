# ScratchBird Official Development Roadmap

**Created:** November 20, 2025
**Last Updated:** November 27, 2025
**Status:** AUTHORITATIVE - Official development phases and goals
**Current Phase:** Alpha 1 (97% Complete, Local Server Phase 1-4 ~85% Complete, 95 Server Tests Passing)

**Project Nature:** This is an educational/development project with **NO fixed timeframe constraints**. Each stage is complete when ALL defined elements are implemented, not based on time estimates.

---

## ⚠️ IMPORTANT: Production Readiness

**ScratchBird should NEVER be referred to as "Production Ready" until Gold Release.**

The term "production-ready" in technical documentation refers to **component stability** within the development environment, NOT suitability for production deployment.

---

## Roadmap Overview

```
ALPHA STAGE (Embedded Engine)
├── Alpha 1: Engine Functionality (97% Complete) ← CURRENT
├── Alpha 2: Parser Separation (Not Started)
└── Alpha 3: Network Listeners (Not Started)

BETA STAGE (Distributed & Multi-Model)
├── Beta 1: Cluster Implementation (Not Started)
├── Beta 2: Heterogeneous Clusters (Not Started)
├── Beta 3: Encryption & Advanced Indexes (Not Started)
└── Beta 4: NoSQL Dialects & Integration Tools (Not Started)

RELEASE CANDIDATE STAGE (Stabilization & Drivers)
├── RC1: Native Drivers + Beta User Testing (Not Started)
├── RC2: Iterative Bug Fixing (Not Started)
└── RC3: Final Stabilization (Not Started)

PRODUCTION RELEASE
└── Version 1.0 (Gold): Production Ready (Not Started)
```

---

# ALPHA STAGE: Embedded Database Engine

**Goal:** Complete, robust embedded database engine with local-only operations

---

## Alpha 1: Engine Functionality (LOCAL OPERATIONS ONLY)

**Status:** 97% Complete

**Note:** Local Server Architecture Phase 1-4 mostly complete! (IPC, Wire Protocol, sb_server process, Client Library). Phase 4 at ~85% (36 unit + 6 integration tests, 95 total server tests passing). Remaining work is Phase 4-5 completion (~10-20 hours) and CLI Tools (~90-110 hours). All P0-P2 improvements 100% complete! P3 70% complete (6 items blocked).

**Completion Policy:** Alpha 1 is NOT complete until ALL local (non-network) functionality is implemented. There are NO "nice to have" deferrals - if a command is local, it MUST be in Alpha 1.

### Scope Definition

**INCLUDES** - All local, non-network engine operations:
- SQL execution (SELECT, INSERT, UPDATE, DELETE, MERGE)
- DDL operations (CREATE/ALTER/DROP for all object types)
- Transaction management (BEGIN, COMMIT, ROLLBACK, SAVEPOINT)
- Constraint enforcement (CHECK, FOREIGN KEY, UNIQUE, PRIMARY KEY, DEFAULT, GENERATED, IDENTITY)
- Index operations (all 11 index types)
- Security (users, roles, permissions, RLS)
- Stored procedures and triggers
- Built-in functions (all 123 functions)
- PSQL procedural language
- Views (regular and materialized)
- Sequences and generators
- SQL engine internal commands (SHOW TABLES, SHOW COLUMNS, DESCRIBE, EXPLAIN, etc.)
- RETURNING clause (INSERT/UPDATE/DELETE with RETURNING)
- Common Table Expressions (CTEs) and recursive queries
- Command-line tools (sb_isql, sb_verify, sb_backup, sb_security)

**EXCLUDES** - Network operations:
- No network listeners
- No wire protocol handling
- No remote connections
- No client/server architecture

### Completion Status

#### ✅ COMPLETE (Components)

**Core Engine (100%)**
- MGA (Multi-Generational Architecture) - TIP-based visibility
- Buffer Pool & Pages - LRU caching, back-versioning
- TOAST - Large object storage
- Transactions - 4 isolation levels
- Tablespaces - Multi-file support

**Catalog System (40 tables - 100% structures, 58% CRUD)**
- 18-level schema hierarchy
- All 40 catalog table structures defined
- Core metadata (schemas, tables, columns, indexes, sequences, views)
- Security tables (users, roles, groups, permissions, policies)
- Dependency and comment tracking

**Indexes (11/11 types - 100%)**
- B-Tree, Hash, R-Tree, GIN, Bitmap
- GiST, HNSW, SP-GiST, BRIN
- LSM-Tree, Columnstore
- All MGA-compliant, production-ready

**Data Types (86/86 - 100%)**
- All numeric, string, temporal, binary types
- Special types (UUID, JSON/JSONB, XML, BOOLEAN)
- Spatial types (POINT, LINESTRING, POLYGON, etc.)
- Advanced types (ARRAY, RANGE, COMPOSITE, VECTOR, VARIANT)
- Network types (INET, CIDR, MACADDR)
- Text search types (TSVECTOR, TSQUERY)

**Built-in Functions (153/153 - 100%)** 🎉
- String (14), Aggregate (15), Window (17)
- JSON (13), Array (12), Date/Time (7)
- Mathematical (36), Bit Manipulation (14)
- Cryptographic (4), Statistical (16)
- XML (9), Spatial (40+), Regex (4)
- Text Search, Conditional (3)
- **Recently Added:** ROLLUP, CUBE, GROUPING SETS, regression functions, hyperbolic math

**Security System (100% - Phase 3.5 Complete)**
- User/role/group management
- Permission system (table, column, object-level)
- Row-Level Security (RLS) with DML enforcement
- GRANT/REVOKE statements
- SQL SECURITY DEFINER/INVOKER
- Password hashing (BCrypt)
- Permission cache (LRU, 60s TTL)

**Constraints (100% Complete)** 🎉
- NOT NULL, UNIQUE, PRIMARY KEY
- FOREIGN KEY (single and composite)
- CHECK constraints
- DEFAULT expressions
- GENERATED columns (STORED/VIRTUAL)
- IDENTITY columns (GENERATED ALWAYS/BY DEFAULT AS IDENTITY)
- Deferred constraint checking (DEFERRABLE, INITIALLY DEFERRED)
- Referential actions (CASCADE, SET NULL, SET DEFAULT)

**DDL Operations (100% Complete)**
- CREATE/ALTER/DROP TABLE
- CREATE/DROP INDEX
- CREATE/ALTER/DROP SEQUENCE
- CREATE/DROP VIEW (regular and materialized)
- ALTER TABLE (ADD/DROP/RENAME COLUMN, ALTER COLUMN TYPE)
- CREATE/ALTER/DROP TABLESPACE
- All security DDL (USER, ROLE, GROUP, POLICY)

**PSQL/Stored Procedures & Triggers (100% Complete)** 🎉
- Variable scope management and operations
- Control flow execution (IF, LOOP, WHILE, EXIT, RETURN)
- Exception handling (RAISE, TRY/EXCEPT)
- Cursor operations (DECLARE, OPEN, FETCH, CLOSE)
- Trigger firing mechanism (BEFORE/AFTER, FOR EACH ROW)
- Stored procedure/function invocation with OUT/INOUT parameters

**Advanced SQL Features (100% Complete)** 🎉
- Common Table Expressions (CTEs) - non-recursive and recursive
- Set operations (UNION, UNION ALL, INTERSECT, INTERSECT ALL, EXCEPT, EXCEPT ALL)
- MERGE statement (all 3 WHEN clause types)
- RETURNING clause (INSERT/UPDATE/DELETE)
- SAVEPOINT (nested transaction control)

**SQL Engine Commands (100% Complete)** 🎉
- SHOW TABLES, SHOW DATABASES, SHOW COLUMNS, SHOW INDEXES, SHOW CREATE TABLE
- DESCRIBE/DESC table introspection
- EXPLAIN query plan visualization

#### ✅ COMPLETE (Components) - CONTINUED

**Views (100% Complete)** 🎉
- ✅ CREATE VIEW / CREATE OR REPLACE VIEW
- ✅ CREATE MATERIALIZED VIEW with automatic column derivation
- ✅ DROP VIEW [IF EXISTS] [CASCADE | RESTRICT]
- ✅ REFRESH [CONCURRENTLY] MATERIALIZED VIEW with full data re-execution
- ✅ Query expansion (SELECT from views → underlying tables)
- ✅ Column projection
- ✅ WITH CHECK OPTION (parser + catalog)
- ✅ Physical materialization (table creation + data population from SELECT query)
- ✅ Materialized view refresh (delete + repopulate with fresh query results)

**Critical Issues (P0) - 100% Complete** 🎉
- ✅ P0-1: Password Policy Enforcement
- ✅ P0-2: Account Lockout Mechanism
- ✅ P0-3: Security Audit Logging
- ✅ P0-4: Arithmetic Overflow Checking
- ✅ P0-5: NaN/Infinity Handling
- ✅ P0-6: GIN Parallel Operations MGA Bug
- ✅ P0-7: Catalog Sequence Operations
- ✅ P0-8: Charset/Collation Read Operations

**High-Priority Improvements (P1) - 100% Complete** ✅ 🎉
- ✅ P1-2: XID Wraparound Prevention, P1-3: SQLSTATE Error Codes
- ✅ P1-6: Foreign Key Actions (CASCADE/SET NULL/SET DEFAULT)
- ✅ P1-7: TIP Binary Search (N/A - using CLOG O(1) lookup)
- ✅ P1-1: TRY/EXCEPT Exception Handling (executor.cpp:19142) **ALREADY IMPLEMENTED!**
- ✅ P1-4: Cursor Operations (executor.cpp:18956+) **ALREADY IMPLEMENTED!**
- ✅ P1-5: Stored Procedure Invocation (executor.cpp:18321) **ALREADY IMPLEMENTED!**
- ✅ P1-8: Index-Based FK Lookups, P1-9: Constraints Table CRUD
- ✅ P1-10: Statistics & ANALYZE (100% - COMPLETE! commit 5676aae)
- ✅ P1-12: Session Timeout, P1-13: MERGE Statement, P1-14: RETURNING Clause
- ✅ P1-15: Multi-Geometry Functions
- ✅ P1-11: Bulk Index Loading (100% - bottom-up B-tree construction complete!)

**Medium-Priority Improvements (P2) - 100% Complete** ✅ 🎉
- ✅ P2-1: Page Table Lock Partitioning, P2-2: Dirty Page Counter
- ✅ P2-3: TOAST Chunk Prefetching, P2-4: Permission Cache TTL Reduction
- ✅ P2-5: Hash Index Directory Resize
- ✅ P2-6: GENERATED Columns, P2-7: Deferred Constraints
- ✅ P2-8: Statement-Level Triggers, P2-10: Statistical Aggregate Functions
- ✅ P2-11: Edge Case Test Suite, P2-12: Concurrent Transaction Tests
- ✅ P2-13: Performance Benchmark Suite, P2-14: Constraint Enforcement Tests
- ✅ P2-15: Role Cycle Detection, P2-16: Policy Expression Validation
- ✅ P2-17: Error Message Context
- ✅ P2-18: Materialized View Refresh Strategies, P2-19: Query Result Caching
- ✅ P2-20: Parallel Query Execution, P2-21: Prepared Statement Cache
- ✅ P2-22: Connection Pooling, P2-23: Backup/Restore Improvements
- ✅ P2-24: Query Planner Statistics, P2-25: Index Advisor

#### ⧗ IN PROGRESS (Components)

**Improvement Opportunities (95% Complete)**
- ✅ P0 Critical Issues (8/8 - 100% complete)
- ✅ P1 High Priority (15/15 - 100% complete) 🎉
- ✅ P2 Medium Priority (25/25 - 100% complete) 🎉
- 🔄 P3 Low Priority (14/20 - 70% complete, 6 blocked by Alpha 3/dependencies)

**Catalog Cleanup (ALL PHASES COMPLETE)** 🎉
- ✅ Phase A: CRUD operations (37 methods: dropSchema, Domain/UDR/Package/Emulation CRUD)
- ✅ Phase B: Structures (46 method declarations, 11 structs/enums: SchemaType, Synonyms, FDW, UDR modules)
- ✅ Phase C: System table page allocation (7 new table pages)
- ✅ Phase D: Virtual catalog infrastructure (~4,290 lines)
  - VirtualCatalogHandler, VirtualCatalogRouter
  - information_schema (12 views), pg_catalog (12 views)
  - mysql.* (6 tables), sys.* (8 views)
  - EmulationViewGenerator for on-demand Firebird RDB$*

#### ❌ NOT IMPLEMENTED (Remaining Items - ~5%)

**Local Server Architecture** (~10-20 hours remaining) - **IN PROGRESS**
- ✅ Phase 1: IPC Infrastructure (Unix sockets, Named pipes, TCP localhost) - COMPLETE (22 tests)
- ✅ Phase 2: Wire Protocol (binary message format, result streaming) - COMPLETE (37 tests)
- ✅ Phase 3: Server Process (sb_server, multi-threading, sessions) - COMPLETE
- ✅ Phase 4: Client Library (libscratchbird_client, auto-start) - ~85% COMPLETE (36 unit + 6 integration tests)
- ❌ Phase 5: Integration & Testing - 10-20 hours
- **Total Server Tests: 95 passing** (22 IPC + 37 wire protocol + 36 client)
- **Required before CLI tools**
- See [docs/planning/LOCAL_SERVER_ARCHITECTURE_PLAN.md](docs/planning/LOCAL_SERVER_ARCHITECTURE_PLAN.md)

**Command-Line Tools** (~90-110 hours estimated)
- ❌ sb_isql (interactive SQL shell) - HIGHEST PRIORITY
- ❌ sb_verify (database integrity checker)
- ❌ sb_backup (backup/restore tool)
- ❌ sb_security (user/role management tool)
- **All tools connect via libscratchbird_client**

**Blocked Improvement Items**
- 🔒 P3-2/3/4: MFA, IP whitelisting, certificate auth (require Alpha 3 network layer)
- 🔒 P3-14/15/20: Partition pruning, MV rewriting, join ordering (require dependencies)

### Alpha 1 Completion Criteria

**ALL items below MUST be complete before Alpha 1 is considered done:**

1. ✅ All 11 index types functional
2. ✅ All 86 data types supported
3. ✅ All 153 built-in functions implemented - **100% COMPLETE** 🎉
4. ✅ Security system complete (users, roles, permissions, RLS)
5. ✅ Constraint enforcement (CHECK, FK, UNIQUE, PK, DEFAULT, GENERATED, IDENTITY, Deferred)
6. ✅ Views fully functional (regular + materialized with data population) - **100% COMPLETE** 🎉
7. ✅ PSQL/stored procedure execution - **100% COMPLETE** 🎉
8. ✅ Trigger firing mechanism - **100% COMPLETE** 🎉
9. ✅ CTEs and recursive queries - **100% COMPLETE** 🎉
10. ✅ MERGE statement - **100% COMPLETE** 🎉
11. ✅ All P0 critical issues resolved - **100% COMPLETE** 🎉
11. ✅ RETURNING clause - **100% COMPLETE**
12. ✅ GENERATED/IDENTITY columns - **100% COMPLETE**
13. ✅ Deferred constraint checking - **100% COMPLETE**
14. ✅ SQL engine internal commands (SHOW, DESCRIBE, EXPLAIN) - **100% COMPLETE**
15. ❌ Command-line tools (sb_isql, sb_verify, sb_backup, sb_security)

**Progress: 16/18 major components complete (89%)**

This includes the original 15 components plus 3 additional scopes identified during development:
- Missing functions (30 functions to add) - ✅ COMPLETE
- Improvement opportunities (62/68 items across all subsystems) - 95% complete
- Catalog cleanup (all 4 phases) - ✅ COMPLETE
- Local server architecture (mandatory before CLI tools) - NOT STARTED

**No deferrals. Alpha 1 complete = ALL local functionality complete.**

---

## Alpha 2: Parser Separation

**Status:** Not Started
**Dependencies:** Alpha 1 must be 100% complete
**Goal:** Extract built-in SQL parser into separate library/layer

**Completion Policy:** Alpha 2 is complete when the parser is fully separated and ALL dialect parsers (ScratchBird, PostgreSQL, MySQL, MSSQL, FirebirdSQL) are functional.

### Architectural Goal

Transform the monolithic embedded engine into a **multi-parser system**:

```
┌─────────────────────────────────────────────┐
│         Client Applications                 │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│         Parser Layer (Pluggable)            │
├─────────────────────────────────────────────┤
│  • ScratchBird Parser (native dialect)     │
│  • PostgreSQL Parser (emulation)            │
│  • MySQL Parser (emulation)                 │
│  • MSSQL Parser (emulation)                 │
│  • FirebirdSQL Parser (emulation)           │
└─────────────────────────────────────────────┘
                    ↓
           SBLR Bytecode Interface
                    ↓
┌─────────────────────────────────────────────┐
│         Database Engine (Alpha 1)           │
│  • Storage, Indexes, Transactions           │
│  • Catalog, Security, Constraints           │
│  • SBLR Bytecode Executor                   │
└─────────────────────────────────────────────┘
```

### Core Requirements

**1. SBLR-Only Engine API**
- Remove direct SQL parsing from engine
- Engine ONLY accepts SBLR bytecode
- C++ API: `Status execute(const std::vector<uint8_t>& sblr_bytecode, ResultSet* result)`
- No SQL strings in engine layer

**2. Parser Abstraction Layer**
- Abstract base class: `SQLParser`
- Interface: `virtual Status parse(const std::string& sql, std::vector<uint8_t>* bytecode) = 0`
- Parser registry/factory pattern
- Runtime parser selection

**3. Multiple Parser Implementations**

**ScratchBird Parser** (Primary):
- Native dialect (already implemented in `src/parser/parser.cpp`)
- Full feature support
- Extract into `libsb_parser_scratchbird.so`

**PostgreSQL Parser** (Emulation):
- PostgreSQL SQL dialect → SBLR translation
- Leverage existing PostgreSQL grammar knowledge
- Map PostgreSQL types to ScratchBird types
- Translate PostgreSQL functions to ScratchBird equivalents
- **Note:** May require extending SBLR opcodes for PostgreSQL-specific features

**MySQL Parser** (Emulation):
- MySQL SQL dialect → SBLR translation
- Handle MySQL-specific syntax (backticks, LIMIT offset,count, etc.)
- Map MySQL types to ScratchBird types
- **Note:** AUTO_INCREMENT → SEQUENCE translation

**MSSQL Parser** (Emulation):
- T-SQL dialect → SBLR translation
- Handle MSSQL-specific syntax ([], TOP, etc.)
- Map MSSQL types to ScratchBird types
- **Note:** IDENTITY → SEQUENCE translation

**FirebirdSQL Parser** (Emulation):
- FirebirdSQL dialect → SBLR translation
- This project originated as a FirebirdSQL refactoring
- Use FirebirdSQL as specification template
- Map Firebird types to ScratchBird types
- Handle Firebird-specific features (PSQL procedures, generators, etc.)

**4. Shared Components**
- Common lexer utilities (where applicable)
- Shared semantic analysis for type checking
- Unified bytecode generator targeting SBLR
- Error reporting framework

### Implementation Phases

**Phase 2.1: Engine API Refactoring**
- Remove SQL parsing from engine core
- Define clean SBLR-only API
- Update all engine internal calls
- Comprehensive API testing

**Phase 2.2: ScratchBird Parser Extraction**
- Extract parser into separate library
- Create `libsb_parser_scratchbird.so`
- Define parser plugin interface
- Integration testing with engine

**Phase 2.3: PostgreSQL Parser**
- Implement PostgreSQL grammar
- PostgreSQL → SBLR bytecode translation
- Type mapping layer
- Function mapping layer
- Comprehensive testing against PostgreSQL test suite

**Phase 2.4: MySQL Parser**
- Implement MySQL grammar
- MySQL → SBLR bytecode translation
- Type and function mapping
- Testing against MySQL test suite

**Phase 2.5: MSSQL Parser**
- Implement T-SQL grammar
- MSSQL → SBLR bytecode translation
- Type and function mapping
- Testing against MSSQL test suite

**Phase 2.6: FirebirdSQL Parser**
- Implement FirebirdSQL grammar
- FirebirdSQL → SBLR bytecode translation
- Type and function mapping
- PSQL procedure mapping
- Testing against FirebirdSQL test suite

**Phase 2.7: Parser Registry & Dynamic Loading**
- Parser factory pattern
- Dynamic library loading
- Parser capability negotiation
- Parser version compatibility

### Completion Criteria

**ALL items below MUST be complete before Alpha 2 is considered done:**

1. Engine accepts ONLY SBLR bytecode (no SQL strings)
2. ScratchBird parser as separate library
3. ALL emulation parsers functional: PostgreSQL, MySQL, MSSQL, FirebirdSQL
4. Parser plugin architecture with runtime selection
5. All Alpha 1 features accessible through all parsers
6. Parser factory pattern implemented
7. Dynamic library loading working
8. Comprehensive testing against native test suites for each dialect

**No deferrals. Alpha 2 complete = ALL parsers functional.**

---

## Alpha 3: Network Listeners

**Status:** Not Started
**Dependencies:** Alpha 2 must be 100% complete
**Goal:** Add network capability with wire protocol support

**Completion Policy:** Alpha 3 is complete when ALL wire protocols are functional and clients can connect successfully.

### Architectural Goal

Transform the multi-parser embedded engine into a **networked database server**:

```
┌────────────────┐  ┌────────────────┐  ┌────────────────┐
│ psql client    │  │ mysql client   │  │ SSMS client    │
│ (port 5432)    │  │ (port 3306)    │  │ (port 1433)    │
└────────────────┘  └────────────────┘  └────────────────┘
        ↓                   ↓                    ↓
┌─────────────────────────────────────────────────────────┐
│            Network Listener Layer                       │
├─────────────────────────────────────────────────────────┤
│  • PostgreSQL Wire Protocol (port 5432)                 │
│  • MySQL Wire Protocol (port 3306)                      │
│  • TDS Wire Protocol (port 1433) - MSSQL                │
│  • ScratchBird Native Protocol (port TBD)               │
└─────────────────────────────────────────────────────────┘
        ↓                   ↓                    ↓
┌─────────────────────────────────────────────────────────┐
│            Protocol → Parser Mapping                    │
│  • Wire protocol decoding                               │
│  • Authentication handling                              │
│  • Session management                                   │
│  • Result set serialization                             │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│            Parser Layer (Alpha 2)                       │
│  • ScratchBird Parser                                   │
│  • PostgreSQL Parser                                    │
│  • MySQL Parser                                         │
│  • MSSQL Parser                                         │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│            Database Engine (Alpha 1)                    │
└─────────────────────────────────────────────────────────┘
```

### Core Requirements

**1. Wire Protocol Implementations**

**PostgreSQL Wire Protocol** (port 5432):
- Startup message parsing
- Authentication (MD5, SCRAM-SHA-256)
- Query protocol (Simple Query, Extended Query)
- COPY protocol
- Prepared statements
- Portal management
- LISTEN/NOTIFY (defer to Beta if needed)
- **Reference:** `/docs/specifications/wire_protocols/postgresql_wire_protocol.md`

**MySQL Wire Protocol** (port 3306):
- Handshake protocol
- Authentication (mysql_native_password, caching_sha2_password)
- Command phase (COM_QUERY, COM_PREPARE, COM_EXECUTE)
- Result set encoding (text, binary)
- **Reference:** `/docs/specifications/wire_protocols/mysql_wire_protocol.md`

**TDS Protocol** (port 1433) - MSSQL:
- Login packet handling
- TDS message framing
- SQL batch execution
- RPC (Remote Procedure Call) protocol
- Attention signals (query cancellation)
- **Reference:** `/docs/specifications/wire_protocols/tds_wire_protocol.md`

**ScratchBird Native Protocol** (port TBD):
- Optimized for ScratchBird features
- Direct SBLR bytecode transmission (optional)
- Enhanced security options
- Future: Streaming, subscriptions

**2. Connection Management**
- Connection pooling
- Session state tracking
- Authentication integration with ScratchBird security system
- Multi-threaded connection handling
- Connection limits and throttling

**3. Result Set Serialization**
- Wire protocol-specific encoding
- Type mapping (ScratchBird → protocol-specific types)
- Large result set streaming
- Binary vs. text format support

**4. Error Handling**
- Protocol-specific error codes
- Error message translation
- SQLSTATE mapping

**5. Performance Optimizations**
- Zero-copy buffer management where possible
- Prepared statement caching
- Connection reuse
- Async I/O (epoll/kqueue/IOCP)

### Implementation Phases

**Phase 3.1: Network Infrastructure**
- Socket management (TCP/IP, Unix domain sockets)
- Thread pool for connection handling
- Connection state machine
- Session management layer
- **Reference:** `/docs/specifications/NETWORK_LAYER_SPEC.md`

**Phase 3.2: PostgreSQL Wire Protocol**
- Protocol decoder/encoder
- Authentication handlers
- Query execution integration
- Result set serialization
- Comprehensive testing with psql, pgAdmin, etc.

**Phase 3.3: MySQL Wire Protocol**
- Protocol decoder/encoder
- Authentication handlers
- Query execution integration
- Result set serialization
- Testing with mysql client, MySQL Workbench

**Phase 3.4: TDS Wire Protocol (MSSQL)**
- Protocol decoder/encoder
- Authentication handlers
- Query execution integration
- Result set serialization
- Testing with SSMS, Azure Data Studio

**Phase 3.5: ScratchBird Native Protocol**
- Design native protocol
- Implement decoder/encoder
- Security features
- Performance optimizations
- Client library development

**Phase 3.6: Security & Authentication**
- SSL/TLS support (OpenSSL)
- Certificate management
- Integration with Alpha 1 security system
- Password encryption in transit
- **Reference:** `/docs/specifications/AUTH_CERTIFICATE_TLS.md`

**Phase 3.7: Performance & Testing**
- Load testing
- Connection storm handling
- Memory leak detection
- Protocol compliance testing
- Interoperability testing

### Completion Criteria

**ALL items below MUST be complete before Alpha 3 is considered done:**

1. Functional network listener on all 4 protocols (PostgreSQL, MySQL, TDS/MSSQL, ScratchBird native)
2. Client authentication working for all protocols
3. Query execution through network for all dialects
4. Connection pooling functional
5. SSL/TLS support implemented
6. Graceful connection handling (connect, disconnect, errors)
7. Successfully tested with native clients (psql, mysql, SSMS)
8. Connection monitoring and statistics
9. Load testing completed
10. No memory leaks detected

**No deferrals. Alpha 3 complete = ALL network functionality working.**

---

# BETA STAGE: Distributed & Multi-Model Database

**Goal:** Enterprise-grade distributed multi-model database with NoSQL capabilities and external system integration

---

## Beta 1: Cluster Implementation

**Status:** Not Started
**Dependencies:** Alpha 3 must be 100% complete
**Goal:** Multiple servers acting as single integrated platform

**Completion Policy:** Beta 1 is complete when cluster functionality is fully operational with automatic sharding and failover.

### Architectural Goal

Transform single-server system into a **distributed cluster**:

```
┌──────────────────────────────────────────────────────┐
│              Client Applications                     │
└──────────────────────────────────────────────────────┘
                        ↓
┌──────────────────────────────────────────────────────┐
│           Cluster Coordinator (Master)               │
│  • Query routing                                     │
│  • Shard mapping                                     │
│  • Load balancing                                    │
│  • Distributed transaction coordination              │
└──────────────────────────────────────────────────────┘
          ↓              ↓              ↓
┌───────────────┐ ┌───────────────┐ ┌───────────────┐
│ ScratchBird   │ │ ScratchBird   │ │ ScratchBird   │
│ Node 1        │ │ Node 2        │ │ Node 3        │
│ (Shard A)     │ │ (Shard B)     │ │ (Shard C)     │
└───────────────┘ └───────────────┘ └───────────────┘
```

### Core Requirements

**1. Cluster Membership & Discovery**
- Gossip protocol for node discovery
- Health checks and heartbeats
- Automatic node addition/removal
- Split-brain detection and resolution
- **Reference:** `/docs/specifications/REPLICATION_AND_SHADOW_PROTOCOLS.md`

**2. Data Distribution (Sharding)**
- Hash-based sharding
- Range-based sharding
- Consistent hashing for rebalancing
- Shard key selection strategies
- Automatic shard migration
- **Reference:** `/docs/specifications/DDL_TABLE_PARTITIONING.md`

**3. Replication**
- Master-slave replication
- Multi-master replication (optional)
- Synchronous vs. asynchronous replication
- Conflict resolution strategies
- Replica lag monitoring

**4. Distributed Transactions**
- Two-phase commit (2PC)
- Three-phase commit (3PC) (optional)
- Distributed deadlock detection
- Transaction coordinator
- **Reference:** `/docs/specifications/TRANSACTION_DISTRIBUTED.md`

**5. Query Routing**
- Parse query, determine affected shards
- Route to appropriate nodes
- Aggregate results from multiple shards
- Distributed JOIN optimization
- Query pushdown where possible

**6. Cluster Catalog**
- Global metadata (shard mappings, node locations)
- Replicated catalog for consistency
- Version tracking for schema changes

**7. Failover & High Availability**
- Automatic failover
- Replica promotion
- Quorum-based decisions
- No single point of failure

**8. Cluster Management Tools**
- sb_cluster_init (initialize cluster)
- sb_cluster_add_node (add node)
- sb_cluster_remove_node (remove node)
- sb_cluster_status (health monitoring)
- sb_cluster_rebalance (shard migration)

### Implementation Phases

**Phase 1.1: Cluster Membership**
- Gossip protocol implementation
- Node discovery and registration
- Health monitoring

**Phase 1.2: Sharding Infrastructure**
- Shard mapping catalog
- Hash and range partitioning
- Shard assignment algorithms
- Migration framework

**Phase 1.3: Distributed Transactions**
- Two-phase commit implementation
- Transaction coordinator
- Distributed deadlock detection
- Recovery protocols

**Phase 1.4: Query Routing**
- Query analyzer (determine affected shards)
- Multi-shard query execution
- Result aggregation
- Distributed query optimization

**Phase 1.5: Replication**
- Write-ahead log (WAL) streaming
- Replica synchronization
- Lag monitoring
- Failover mechanisms

**Phase 1.6: Cluster Tools & Testing**
- Management tools
- Monitoring and observability
- Chaos testing (network partitions, node failures)
- Load testing

### Completion Criteria

**ALL items below MUST be complete before Beta 1 is considered done:**

1. Cluster of 3+ nodes functional
2. Automatic sharding working (hash and range-based)
3. Distributed transactions (2PC) fully operational
4. Query routing and aggregation
5. Replication and failover
6. Cluster management tools (sb_cluster_* suite)
7. No data loss on single node failure
8. Zero-downtime shard rebalancing
9. Chaos testing passed (network partitions, node failures)
10. Load testing completed

**No deferrals. Beta 1 complete = ALL cluster functionality working.**

---

## Beta 2: Heterogeneous Clusters

**Status:** Not Started
**Dependencies:** Beta 1 must be 100% complete
**Goal:** Add non-ScratchBird servers to cluster

**Completion Policy:** Beta 2 is complete when non-ScratchBird databases can join the cluster and participate in federated queries.

### Architectural Goal

Enable **mixed database clusters**:

```
┌──────────────────────────────────────────────────────┐
│              Cluster Coordinator                     │
│  • Unified query routing                             │
│  • Cross-database query federation                   │
│  • Heterogeneous transaction coordination            │
└──────────────────────────────────────────────────────┘
     ↓              ↓              ↓              ↓
┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
│ScratchBird│  │PostgreSQL│  │  MySQL   │  │  MSSQL   │
│  Node     │  │  Node    │  │  Node    │  │  Node    │
└──────────┘  └──────────┘  └──────────┘  └──────────┘
```

### Core Requirements

**1. Foreign Data Wrappers (FDW)**
- PostgreSQL FDW
- MySQL FDW
- MSSQL FDW
- Oracle FDW (optional)
- **Reference:** `/docs/specifications/09_DDL_FOREIGN_DATA.md`

**2. Unified Catalog**
- Cross-database schema discovery
- Type mapping between different databases
- Capability negotiation (what features each DB supports)

**3. Distributed Query Federation**
- Cross-database JOINs
- Query rewriting for heterogeneous sources
- Predicate pushdown to each database
- Result merging with type coercion

**4. Transaction Coordination**
- XA transactions across different databases
- Heterogeneous 2PC
- Compensation transactions (SAGA pattern if XA not supported)

**5. Data Synchronization**
- CDC (Change Data Capture) from external databases
- Bi-directional sync (where possible)
- Conflict resolution strategies

### Implementation Phases

**Phase 2.1: FDW Framework**
- Abstract FDW interface
- FDW lifecycle management
- Capability negotiation

**Phase 2.2: PostgreSQL Integration**
- PostgreSQL FDW implementation
- Type mapping
- Query pushdown
- Testing with real PostgreSQL instances

**Phase 2.3: MySQL Integration**
- MySQL FDW implementation
- Type mapping
- Query pushdown

**Phase 2.4: MSSQL Integration**
- MSSQL FDW implementation (via TDS)
- Type mapping
- Query pushdown

**Phase 2.5: FirebirdSQL Integration**
- FirebirdSQL FDW implementation
- Type mapping
- Query pushdown
- Testing with real Firebird instances

**Phase 2.6: Federated Query Engine**
- Cross-database query planner
- Federated execution engine
- Result merging and type coercion
- Cost-based optimization

**Phase 2.7: Distributed Transactions (XA)**
- XA protocol implementation
- Heterogeneous 2PC
- SAGA pattern fallback
- Recovery mechanisms

### Completion Criteria

**ALL items below MUST be complete before Beta 2 is considered done:**

1. FDW for ALL supported external databases: PostgreSQL, MySQL, MSSQL, FirebirdSQL
2. Cross-database transactions (XA and SAGA patterns)
3. Query federation working across all database types
4. Type mapping complete for all databases
5. Bi-directional data sync operational
6. Conflict resolution strategies implemented
7. Testing completed with real external database instances
8. Performance benchmarking across heterogeneous queries

**No deferrals. Beta 2 complete = ALL heterogeneous cluster features working.**

---

## Beta 3: Encryption & Advanced Indexes

**Status:** Not Started
**Dependencies:** Beta 2 must be 100% complete
**Goal:** Enterprise-grade security and advanced indexing

**Completion Policy:** Beta 3 is complete when ALL encryption features and advanced indexes (per specifications) are fully implemented.

### Part A: Database Encryption & Key Management

**1. Field-Level Encryption**
- Column-level encryption specification
- Transparent Data Encryption (TDE)
- Application-level encryption support
- Encrypted indexes (searchable encryption where possible)

**2. Key Management Server**
- Centralized key storage
- Key rotation support
- Multiple encryption keys per database
- Time-based key versioning (different keys over time)
- Key derivation functions (KDF)

**3. Encryption At Rest**
- Full database file encryption
- Encrypted backups
- Encrypted logs
- **Reference:** `/docs/specifications/DDL_TABLES.md` (encryption clauses)

**4. Encryption In Transit**
- SSL/TLS for all network connections (already in Alpha 3)
- End-to-end encryption for sensitive fields
- Certificate management

**5. Secure Key Storage**
- Hardware Security Module (HSM) integration
- Key escrow for disaster recovery
- Multi-tenant key isolation

### Part B: Advanced Indexes

**Implementation of advanced indexes specified in `/docs/specifications/`:**

**Already Implemented (Alpha 1):**
- ✅ B-Tree, Hash, R-Tree, GIN, Bitmap
- ✅ GiST, HNSW, SP-GiST, BRIN
- ✅ LSM-Tree, Columnstore

**Advanced/Specialized Indexes to Implement:**

**1. Bloom Filter Indexes**
- Space-efficient probabilistic index
- Fast negative lookups
- Optimal for "not exists" queries
- **Reference:** `/docs/specifications/BloomFilterIndex.md`

**2. Full-Text Search Indexes (Advanced)**
- Multi-language stemming
- Phrase search optimization
- Relevance ranking improvements
- Fuzzy matching

**3. Geospatial Indexes (Advanced)**
- 3D spatial indexes (for elevation data)
- Temporal-spatial indexes (for moving objects)
- Geographic coordinate system transformations

**4. Time-Series Indexes**
- Optimized for time-series data
- Downsampling and aggregation
- Retention policies

**5. Machine Learning Indexes**
- Learned indexes (replace B-trees with ML models)
- Adaptive index selection

**6. Graph Indexes**
- For graph database queries
- Neighbor traversal optimization

### Implementation Phases

**Phase 3.1: Key Management Infrastructure**
- Key server design and implementation
- Key rotation mechanism
- HSM integration framework

**Phase 3.2: Field-Level Encryption**
- Column encryption at rest
- Encrypted index support
- Decryption on query

**Phase 3.3: Full Database Encryption**
- File-level encryption
- Encrypted backup/restore
- Performance optimization

**Phase 3.4: Bloom Filter Index**
- Implementation per specification
- Integration with query planner
- Testing and benchmarking

**Phase 3.5: Advanced Full-Text Search**
- Multi-language stemming
- Phrase search optimization
- Relevance tuning

**Phase 3.6: Advanced Geospatial**
- 3D spatial indexes
- Temporal-spatial support
- Testing with real-world GIS data

**Phase 3.7: Time-Series Indexes**
- Index structure optimized for time-series
- Integration with query planner
- Performance benchmarking

**Phase 3.8: Machine Learning Indexes**
- Learned indexes (replace B-trees with ML models)
- Adaptive index selection

**Phase 3.9: Graph Indexes**
- For graph database queries
- Neighbor traversal optimization

### Completion Criteria

**ALL items below MUST be complete before Beta 3 is considered done:**

**Encryption:**
1. Key management server operational
2. Field-level encryption functional
3. Key rotation working
4. Full database encryption at rest
5. Encrypted backups
6. HSM integration complete
7. Multi-tenant key isolation
8. Encrypted index support

**Advanced Indexes:**
1. Bloom Filter index implemented (per specification)
2. Advanced full-text search (multi-language, phrase search, fuzzy matching)
3. Advanced geospatial (3D, temporal-spatial)
4. Time-series indexes fully optimized
5. Machine learning indexes operational
6. Graph indexes implemented

**No deferrals. Beta 3 complete = ALL encryption and advanced index features working.**

---

## Beta 4: NoSQL Dialects & Integration Tools

**Status:** Not Started
**Dependencies:** Beta 3 must be 100% complete
**Goal:** Multi-model database with NoSQL capabilities and external system integration

**Completion Policy:** Beta 4 is complete when ALL NoSQL dialects are functional and ALL integration tools are operational.

### Part A: NoSQL Dialect Support

**Rationale:** The advanced indexes in Beta 3 (Graph, Vector, Time-Series, etc.) enable NoSQL functionality, but require dedicated query dialects beyond SQL.

**Note on Multi-Dialect Support:** Since the parser is architecturally separated from the engine (Alpha 2), ScratchBird can support MULTIPLE query dialects for the same underlying data model. For example, graph data can be queried via Cypher, Gremlin, AND a native ScratchBird graph syntax.

**1. Graph Database Query Dialects**

Research and implement MULTIPLE graph query languages leveraging the same underlying graph index infrastructure:

**Cypher Dialect (Neo4j compatibility)**
- Full Cypher query language implementation
- Pattern matching syntax: `MATCH (n:Person)-[:KNOWS]->(m:Person)`
- Path finding: `shortestPath()`, `allShortestPaths()`
- Graph-specific aggregations
- Cypher → SBLR bytecode translation
- Testing with Neo4j compatibility benchmarks

**Gremlin Dialect (Apache TinkerPop compatibility)**
- Gremlin traversal language implementation
- Imperative traversal API: `g.V().has('name','alice').out('knows')`
- Step-based query execution
- Graph algorithms (PageRank, community detection)
- Gremlin → SBLR bytecode translation
- Testing with TinkerPop-compatible applications

**ScratchBird Native Graph Dialect**
- Custom graph query syntax optimized for ScratchBird
- SQL-like extensions for graph operations
- Integration with standard SQL queries (hybrid queries)
- Performance optimizations specific to MGA architecture
- May use best ideas from both Cypher and Gremlin

**Graph Infrastructure (shared across all dialects)**
- Node and edge traversal operations
- Pattern matching engine
- Path finding algorithms (Dijkstra, A*, BFS, DFS)
- Graph-specific aggregations
- Integration with graph indexes from Beta 3
- **Reference:** Graph database query specifications (to be created during research phase)

**2. Vector Similarity Search Extensions**
- Vector query syntax (k-NN, ANN queries)
- Similarity functions (cosine, euclidean, dot product)
- Vector algebra operations
- Integration with HNSW index from Alpha 1
- Hybrid queries (vector + traditional SQL)
- **Use Cases:** Semantic search, recommendation systems, ML embeddings

**3. Document Store Query Interface**
- Document-oriented query syntax (MongoDB-like)
- JSON path queries and manipulation
- Document validation and schema enforcement
- Integration with JSONB type from Alpha 1
- Collection-based operations

**4. Key-Value Query Interface**
- Simple key-value GET/SET/DELETE operations
- Atomic operations (INCR, DECR, etc.)
- TTL (time-to-live) support
- Pattern-based key scanning
- Integration with Hash index from Alpha 1

**5. Time-Series Query Extensions**
- Time-window queries
- Downsampling and aggregation functions
- Retention policies
- Integration with time-series indexes from Beta 3
- **Use Cases:** IoT data, metrics, monitoring, financial tick data

**6. Column-Family Store Interface**

Research and implement Cassandra/HBase-style wide-column storage:

- **CQL Dialect (Cassandra Query Language compatibility)**
  - Cassandra-compatible query syntax
  - Wide-column data model support
  - Partition keys and clustering columns
  - CQL → SBLR bytecode translation
  - Testing with Cassandra-compatible applications

- **Column-Family Operations**
  - Wide-row storage patterns
  - Column-oriented retrieval
  - Super columns and column families
  - Integration with Columnstore index from Alpha 1
  - Sparse column support

- **Use Cases:** Wide-column analytics, sparse data, high-write throughput scenarios
- **Reference:** Column-family store specifications (to be created during research phase)

**7. Full-Text Search Engine Interface**

Research and implement Elasticsearch/Solr-style search capabilities:

- **Search Query DSL**
  - Query DSL similar to Elasticsearch
  - Boolean queries (must, should, must_not, filter)
  - Full-text search with scoring
  - Fuzzy matching and wildcards
  - Aggregations and faceting
  - Search DSL → SBLR bytecode translation

- **Search Infrastructure**
  - Advanced text analysis and tokenization
  - Relevance scoring (TF-IDF, BM25)
  - Multi-field search
  - Highlighting and snippets
  - Integration with GIN/GiST indexes and text search types from Alpha 1
  - Real-time indexing

- **Use Cases:** Log analysis, content search, e-commerce product search, autocomplete
- **Reference:** Search engine specifications (to be created during research phase)

**8. Stream Processing Interface**

Research and implement stream processing capabilities:

- **Streaming SQL Extensions**
  - Window functions for streaming data
  - Event time vs. processing time semantics
  - Watermarks and late data handling
  - Continuous queries
  - Stream joins and aggregations

- **Stream Infrastructure**
  - In-memory stream buffers
  - State management for streaming operators
  - Exactly-once semantics
  - Integration with Kafka (from Part B)
  - Integration with time-series indexes

- **Use Cases:** Real-time analytics, event processing, monitoring dashboards
- **Reference:** Stream processing specifications (to be created during research phase)

**9. Object/Blob Store Interface**

Research and implement object storage capabilities:

- **S3-Compatible API**
  - Bucket and object operations
  - Multipart uploads
  - Object metadata and tagging
  - Versioning
  - S3 API → SBLR bytecode translation

- **Blob Storage Operations**
  - Large binary object storage
  - Content-addressable storage
  - Deduplication
  - Integration with TOAST from Alpha 1
  - Tiered storage (hot/warm/cold)

- **Use Cases:** Media storage, backups, data lakes
- **Reference:** Object store specifications (to be created during research phase)

### Part B: Integration & Messaging Tools

**1. Apache Kafka Integration**
- Kafka producer/consumer implementation
- Change Data Capture (CDC) → Kafka topics
- Kafka topics → ScratchBird tables (streaming ingestion)
- Schema registry integration
- Offset management
- **Use Cases:** Event streaming, real-time analytics, data pipelines

**2. Message Queue Support**
- RabbitMQ integration
- Redis Pub/Sub integration
- Native message queue tables
- LISTEN/NOTIFY enhancements
- **Use Cases:** Task queues, event-driven architecture

**3. AI/Automation Agent Support**
- RESTful API for agent access
- GraphQL endpoint
- Agent authentication and authorization
- Query result streaming for agents
- Natural language query interface (experimental)
- **Use Cases:** AI agents, automation tools, chatbots

**4. Object Storage Integration**
- S3-compatible object storage integration
- Large object (LOB) external storage
- Tiered storage policies
- **Use Cases:** Storing large files, backups, archival

**5. Observability & Monitoring**
- Prometheus metrics exporter
- Grafana dashboard templates
- OpenTelemetry integration
- Distributed tracing support
- **Use Cases:** Production monitoring, performance analysis

### Implementation Phases

**Phase 4.0: NoSQL Research & Specification**
- Research each NoSQL model (Graph, Vector, Document, Key-Value, Time-Series, Column-Family, Search, Stream, Object/Blob)
- Study existing implementations (Neo4j, Cassandra, Elasticsearch, etc.)
- Create technical specifications for each model
- Define query language syntax for each dialect
- Design bytecode mappings (dialect → SBLR)
- Identify integration points with existing indexes
- Document use cases and benchmarks

**Phase 4.1: Graph Query Dialects**
- **Cypher Implementation**
  - Implement Cypher parser
  - Pattern matching engine
  - Cypher → SBLR bytecode translation
  - Testing with Neo4j compatibility suite
- **Gremlin Implementation**
  - Implement Gremlin parser
  - Traversal execution engine
  - Gremlin → SBLR bytecode translation
  - Testing with TinkerPop test suite
- **ScratchBird Native Graph Dialect**
  - Design native graph syntax
  - Hybrid SQL/graph query support
  - Implementation and testing
- **Shared Infrastructure**
  - Integration with graph indexes from Beta 3
  - Graph algorithm library
  - Performance optimization
  - Benchmarking (LDBC Social Network, etc.)

**Phase 4.2: Vector Query Extensions**
- Vector query syntax design
- k-NN and ANN query support
- Integration with HNSW index
- Hybrid queries (vector + traditional SQL)
- Similarity functions implementation
- Testing with embedding datasets
- Benchmarking (ANN benchmarks)

**Phase 4.3: Document Store Interface**
- MongoDB-compatible query syntax
- JSON path queries
- Document validation
- Integration with JSONB type
- Collection operations
- Testing with MongoDB compatibility suite

**Phase 4.4: Key-Value Interface**
- Redis-compatible command set
- Atomic operations
- TTL support
- Pattern scanning
- Integration with Hash index
- Testing with Redis protocol

**Phase 4.5: Time-Series Query Extensions**
- Time-window query syntax
- Downsampling functions
- Retention policy engine
- Integration with time-series indexes
- Testing with time-series workloads (InfluxDB benchmarks)

**Phase 4.6: Column-Family Store Interface**
- **CQL Implementation**
  - Cassandra Query Language parser
  - Wide-column data model
  - CQL → SBLR bytecode translation
  - Testing with Cassandra compatibility suite
- **Column-Family Operations**
  - Integration with Columnstore index
  - Sparse column support
  - Performance optimization
  - Benchmarking (YCSB)

**Phase 4.7: Full-Text Search Engine Interface**
- **Search DSL Implementation**
  - Elasticsearch-compatible query DSL
  - Boolean queries, scoring
  - Search DSL → SBLR bytecode translation
- **Search Infrastructure**
  - Text analysis and tokenization
  - Relevance scoring (TF-IDF, BM25)
  - Real-time indexing
  - Integration with GIN/GiST indexes
  - Testing and benchmarking

**Phase 4.8: Stream Processing Interface**
- Streaming SQL extensions
- Window functions for streams
- Event time semantics
- Watermarks and late data
- Continuous queries
- Integration with Kafka
- Testing with streaming benchmarks

**Phase 4.9: Object/Blob Store Interface**
- S3-compatible API implementation
- Bucket and object operations
- Multipart uploads
- Integration with TOAST
- Tiered storage
- Testing with S3 compatibility suite

**Phase 4.10: Kafka Integration**
- Kafka client library integration
- CDC → Kafka pipeline
- Kafka → ScratchBird ingestion
- Schema registry support
- Testing with real Kafka clusters

**Phase 4.11: Message Queue & Agent Support**
- RabbitMQ/Redis integration
- Agent API implementation (RESTful)
- GraphQL endpoint
- Authentication/authorization
- Natural language query interface (experimental)
- Testing and documentation

**Phase 4.12: Observability & Monitoring**
- Prometheus metrics exporter
- Grafana dashboard templates
- OpenTelemetry integration
- Distributed tracing support
- Testing and performance validation

### Completion Criteria

**ALL items below MUST be complete before Beta 4 is considered done:**

**Research & Specifications:**
1. Technical specifications created for all 9 NoSQL models
2. Query language syntax documented for each dialect
3. Bytecode mappings designed (dialect → SBLR)
4. Use cases and benchmarks identified

**Graph Database:**
1. Cypher dialect fully functional (Neo4j compatibility)
2. Gremlin dialect fully functional (TinkerPop compatibility)
3. ScratchBird native graph dialect operational
4. All graph dialects tested with appropriate benchmarks
5. Graph algorithm library complete

**Vector Database:**
1. Vector similarity search operations working
2. k-NN and ANN queries functional
3. Hybrid vector + SQL queries operational
4. Tested with embedding datasets

**Document Store:**
1. MongoDB-compatible query interface operational
2. JSON path queries working
3. Document validation functional
4. Tested with MongoDB compatibility suite

**Key-Value Store:**
1. Redis-compatible interface complete
2. Atomic operations functional
3. TTL support working
4. Tested with Redis protocol

**Time-Series Database:**
1. Time-series query extensions implemented
2. Time-window queries functional
3. Downsampling and retention policies working
4. Tested with time-series benchmarks

**Column-Family Store:**
1. CQL (Cassandra Query Language) dialect functional
2. Wide-column data model operational
3. Partition keys and clustering columns working
4. Tested with Cassandra compatibility suite
5. Benchmarked with YCSB

**Full-Text Search Engine:**
1. Elasticsearch-compatible query DSL functional
2. Boolean queries and scoring working
3. Text analysis and tokenization complete
4. Real-time indexing operational
5. Tested with search benchmarks

**Stream Processing:**
1. Streaming SQL extensions implemented
2. Window functions for streams working
3. Event time semantics and watermarks functional
4. Continuous queries operational
5. Tested with streaming benchmarks

**Object/Blob Store:**
1. S3-compatible API functional
2. Bucket and object operations working
3. Multipart uploads operational
4. Tiered storage implemented
5. Tested with S3 compatibility suite

**Integration Tools:**
1. Kafka producer/consumer operational
2. CDC → Kafka pipeline working
3. Message queue integrations complete (RabbitMQ, Redis)
4. Agent API fully functional (RESTful)
5. GraphQL endpoint operational
6. Natural language query interface (experimental) implemented
7. Observability stack complete (Prometheus, Grafana, OpenTelemetry)

**No deferrals. Beta 4 complete = ALL 9 NoSQL models + ALL integration features working.**

---

# RELEASE CANDIDATE STAGE: Stabilization

**Goal:** Feature-complete, thoroughly tested, ready for production evaluation, with native drivers for all major languages

---

## RC1: Feature Complete + Native Drivers + Beta User Testing

**Status:** Not Started
**Dependencies:** Beta 4 must be 100% complete
**Goal:** All planned features implemented, native drivers for all major languages, initial testing and debugging

### Scope

**Feature Freeze:**
- **NO new features** after this point (except native drivers)
- Only bug fixes and performance improvements
- Documentation finalization
- **ALL features from Alpha 1-3 and Beta 1-4 must be complete**

**Native Driver Development:**

RC1 includes development of native ScratchBird drivers for all major programming languages and database connectivity standards. Beta users need these drivers to effectively test the database.

**1. Standard Database Connectivity Drivers**
- **ODBC (Open Database Connectivity)**
  - Full ODBC 3.8 compliance
  - Support for all SQL data types
  - Connection pooling
  - Driver manager registration
  - Testing with ODBC applications (Excel, Tableau, Power BI)

- **JDBC (Java Database Connectivity)**
  - Full JDBC 4.2 compliance
  - Type 4 (pure Java) driver
  - Connection pooling support
  - Prepared statement caching
  - Testing with Java applications (Spring, Hibernate)

**2. Native Language Drivers**

**C++ Driver**
- Header-only or compiled library option
- Modern C++17/20 API
- Exception safety
- RAII resource management
- Async query support
- Integration with standard library types

**C Driver**
- Pure C API (C99/C11)
- Thread-safe
- Callback-based async support
- Compatible with C++ via extern "C"
- Minimal dependencies

**C# / .NET Driver**
- .NET Standard 2.0+ support
- ADO.NET provider implementation
- Entity Framework Core provider
- Async/await support
- LINQ query support

**Rust Driver**
- Idiomatic Rust API
- tokio async runtime support
- Type-safe query builder
- Connection pooling (bb8/deadpool)
- Compile-time SQL validation (optional)

**Pascal / Delphi Driver**
- Free Pascal and Delphi compatibility
- Object Pascal API
- Component-based architecture
- VCL/FMX component suite
- Testing with Lazarus and Delphi IDE

**Python Driver**
- Python 3.8+ support
- DB-API 2.0 (PEP 249) compliance
- Async support (asyncio)
- SQLAlchemy dialect
- Pandas integration
- Type hints (PEP 484)

**Go Driver**
- database/sql interface implementation
- Context support
- Connection pooling
- Prepared statement caching
- Testing with popular Go frameworks

**Node.js / JavaScript Driver**
- Promise-based API
- TypeScript definitions
- Async/await support
- Connection pooling
- Sequelize adapter

**Ruby Driver**
- ActiveRecord adapter
- Connection pooling
- Prepared statement support
- Testing with Rails

**PHP Driver**
- PDO (PHP Data Objects) driver
- MySQLi-compatible interface
- Prepared statement support
- Testing with Laravel, Symfony

**3. Driver Development Infrastructure**
- Unified test suite for all drivers
- Compliance testing framework
- Performance benchmarking
- Documentation and examples for each driver
- CI/CD pipeline for driver builds

**4. Client Libraries (Optional)**
- CLI tools (sbcli - interactive shell)
- GUI administration tool
- Migration utilities

**Beta User Program:**
- Recruit 10-50 beta users/organizations
- Provide RC1 build with full documentation
- Gather feedback on bugs, performance, usability
- Track issues in public issue tracker

**Testing Focus:**

**1. Stress Testing**
- High concurrency (1000+ simultaneous connections)
- Large datasets (100+ GB databases)
- Long-running transactions
- Memory leak detection (Valgrind, AddressSanitizer)
- CPU profiling (perf, gprof)

**2. Chaos Engineering**
- Network partitions in cluster
- Random node failures
- Disk full scenarios
- OOM conditions
- Clock skew

**3. Compatibility Testing**
- All 4 wire protocols tested with native clients
- Cross-database federation scenarios
- Migration from PostgreSQL/MySQL/MSSQL
- Upgrade testing (Alpha → Beta → RC)

**4. Security Audit**
- Penetration testing
- Privilege escalation attempts
- SQL injection prevention
- Authentication bypass attempts
- Encryption verification

**5. Performance Benchmarking**
- TPC-C, TPC-H benchmarks
- Comparison with PostgreSQL, MySQL
- Cluster scalability testing
- Query optimization validation

**6. Documentation Review**
- All features documented
- Migration guides complete
- API reference complete
- Tutorials and examples
- Troubleshooting guides

### Deliverables

1. **RC1 Build** - Binary releases for Linux, macOS, Windows
2. **Native Drivers Package** - All language drivers (ODBC, JDBC, C++, C, C#, Rust, Pascal, Python, Go, Node.js, Ruby, PHP)
3. **Beta User Documentation** - Installation, configuration, migration guides
4. **Driver Documentation** - Installation and usage guides for each driver
5. **Known Issues List** - Public tracker of known bugs and limitations
6. **Performance Baselines** - Benchmark results for reference
7. **Security Audit Report** - External security review findings
8. **Test Coverage Report** - Code coverage statistics

### Exit Criteria (Move to RC2)

**Bugs:**
- Zero **critical** bugs (data corruption, crashes, security vulnerabilities)
- < 10 **major** bugs (significant functional issues)
- < 50 **minor** bugs (cosmetic, low-impact issues)

**Performance:**
- Cluster scales to 10+ nodes
- Handles 1000+ concurrent connections
- 100+ GB database tested successfully
- No memory leaks detected in 72-hour stress test

**Native Drivers:**
- ALL drivers functional (ODBC, JDBC, C++, C, C#, Rust, Pascal, Python, Go, Node.js, Ruby, PHP)
- Each driver passes compliance tests
- Documentation complete for each driver
- Example applications working for each language

**Documentation:**
- 100% of features documented
- Migration guides tested by beta users
- All examples verified to work
- Driver documentation complete

**Beta User Feedback:**
- Positive feedback from majority of beta testers
- No showstopper issues reported
- Drivers tested in real applications
- Feature requests logged for post-1.0

---

## RC2: Iterative Bug Fixing

**Status:** Not Started
**Dependencies:** RC1 completion criteria met
**Goal:** Address all critical and major bugs found in RC1

### Scope

**Based on RC1 Findings:**
- Fix all critical bugs
- Fix all major bugs
- Fix as many minor bugs as possible
- Performance improvements based on benchmarks
- Usability improvements based on beta feedback

**Continuous Testing:**
- Regression testing after each fix
- Beta users test RC2 build
- Automated test suite expansion
- Additional stress testing scenarios

**Areas of Focus (Typical):**
- Cluster stability improvements
- Transaction isolation edge cases
- Query optimizer improvements
- Wire protocol conformance issues
- Error message clarity
- Installation/upgrade issues

### Deliverables

1. **RC2 Build** - Updated binaries with fixes
2. **Bug Fix Report** - Detailed list of issues fixed since RC1
3. **Regression Test Suite** - Expanded automated tests
4. **Performance Improvements** - Measured improvements over RC1
5. **Updated Documentation** - Reflecting any behavior changes

### Exit Criteria (Move to RC3 or Gold)

**If Major Issues Found:**
- All critical bugs fixed → Move to RC3
- All major bugs fixed
- 80%+ minor bugs fixed

**If Minimal Issues Found:**
- Zero critical bugs
- Zero major bugs
- 90%+ minor bugs fixed
- Positive feedback from beta users
- Performance meets or exceeds targets
- → **Consider for Gold Release** (skip RC3)

---

## RC3: Final Stabilization

**Status:** Not Started (Conditional)
**Dependencies:** RC2 completion, significant issues found in RC2
**Goal:** Final bug fixes before Gold

### Scope

**Only Created If Needed:**
- RC3 is ONLY created if RC2 revealed significant issues
- If RC2 is stable, skip directly to Gold

**Focus:**
- Final critical/major bug fixes from RC2
- Last-minute performance tuning
- Final documentation updates
- Final beta user validation

### Exit Criteria (Move to Gold)

**Zero Tolerance:**
- Zero critical bugs
- Zero major bugs
- 95%+ minor bugs fixed or deferred to 1.1

**Confidence:**
- Beta users report stable operation
- No data corruption in any scenario
- Cluster failover working reliably
- All wire protocols fully compliant
- Performance benchmarks meet targets

**Production Readiness:**
- Migration tools tested and validated
- Backup/restore verified
- Monitoring and observability complete
- Support documentation complete
- **Unanimous agreement from development team that it's ready**

---

# PRODUCTION RELEASE

---

## Version 1.0 (Gold): Production Ready

**Status:** Not Started
**Dependencies:** RC2 or RC3 completion criteria met
**Goal:** Official production release

### Production Readiness Criteria

**Technical Criteria:**
1. ✅ Zero critical bugs
2. ✅ Zero major bugs
3. ✅ All planned features complete (Alpha 1-3, Beta 1-4)
4. ✅ All native drivers functional (ODBC, JDBC, C++, C, C#, Rust, Pascal, Python, Go, Node.js, Ruby, PHP)
5. ✅ 95%+ test coverage
6. ✅ No memory leaks in 7-day stress test
7. ✅ Cluster scales to 10+ nodes with linear performance
8. ✅ All wire protocols fully compliant
9. ✅ Security audit passed (no critical/high vulnerabilities)
10. ✅ Encryption working and audited
11. ✅ NoSQL dialects functional (Graph, Vector, Document, Key-Value, Time-Series)
12. ✅ Integration tools operational (Kafka, message queues, agents, object storage)

**Documentation Criteria:**
1. ✅ Complete user documentation
2. ✅ Complete administrator documentation
3. ✅ Complete developer documentation (for extensions)
4. ✅ Migration guides (from PostgreSQL, MySQL, MSSQL)
5. ✅ Troubleshooting guides
6. ✅ Performance tuning guides
7. ✅ API reference complete

**Operational Criteria:**
1. ✅ Installation tested on all supported platforms
2. ✅ Upgrade path from Alpha/Beta tested
3. ✅ Backup/restore thoroughly tested
4. ✅ Monitoring and observability tools available
5. ✅ Support infrastructure in place
6. ✅ Community forum or support channel active

**Legal/Business Criteria:**
1. ✅ Licensing finalized
2. ✅ Terms of service (if applicable)
3. ✅ Support SLA defined (for commercial version)
4. ✅ Trademark/branding finalized

### Release Deliverables

**Software:**
1. Binary packages for Linux (Ubuntu, RHEL, Debian)
2. Binary packages for macOS (Intel, Apple Silicon)
3. Binary packages for Windows (x64)
4. Docker images
5. Kubernetes Helm charts
6. Source code release (if open source)

**Documentation:**
1. User manual (PDF + HTML)
2. Administrator guide (PDF + HTML)
3. Developer guide (PDF + HTML)
4. API reference documentation
5. Migration guides
6. Quickstart tutorials
7. Video tutorials (optional)

**Tools:**
1. sb_isql (interactive SQL shell)
2. sb_verify (integrity checker)
3. sb_backup (backup/restore)
4. sb_security (user/role management)
5. sb_cluster_* (cluster management tools)
6. Migration tools (from PostgreSQL/MySQL/MSSQL)
7. Monitoring integrations (Prometheus, Grafana)

**Support:**
1. Public issue tracker
2. Community forum
3. Documentation website
4. Commercial support offerings (if applicable)
5. Training materials

### Post-1.0 Roadmap (Future)

**Version 1.1+:**
- Bug fixes from production deployments
- Performance optimizations
- Minor feature additions based on user feedback
- Additional language bindings
- Additional platform support

**Version 2.0+ (Future Major Release):**
- Breaking changes if needed
- Major architectural improvements
- New major features
- Advanced analytics capabilities
- Machine learning integration

---

## Critical Success Factors

**Technical:**
1. Maintain MGA architectural purity (no PostgreSQL contamination)
2. Comprehensive testing at each phase
3. Security-first mindset
4. Performance benchmarking against targets
5. Code quality and maintainability

**Process:**
1. Clear phase completion criteria
2. No scope creep within phases
3. Regular progress reviews
4. Beta user feedback integration
5. Developer consensus on readiness

**Team:**
1. Sufficient developer resources
2. Expertise in distributed systems (Beta stage)
3. Security expertise (Beta 3)
4. QA/testing resources
5. Documentation expertise

---

## Important Reminders

### ⚠️ NEVER Call "Production Ready" Before Gold

The phrase **"production ready"** should NEVER be used in public-facing documentation, marketing, or communications until Version 1.0 (Gold) is released.

**Why:**
- Alpha/Beta are development phases with incomplete features
- RC is for testing, not production deployment
- Data loss or corruption could occur
- Security vulnerabilities may exist
- Performance may be inadequate
- No support guarantees

**Acceptable Terms Before Gold:**
- "Development version"
- "Alpha release" / "Beta release"
- "Release candidate"
- "Testing build"
- "Pre-production"

**ONLY After Gold:**
- "Production ready"
- "Production release"
- "Stable release"
- "General availability"

---

## Project Vision: The Universal Database Engine

ScratchBird is being designed as a **universal multi-model database engine** that can emulate the full functionality expected by any database client, while providing capabilities far beyond traditional databases.

**Key Differentiators:**

1. **Multi-Dialect SQL Support**
   - Native ScratchBird SQL
   - PostgreSQL emulation (complete wire protocol + dialect)
   - MySQL emulation (complete wire protocol + dialect)
   - MSSQL emulation (TDS protocol + T-SQL dialect)
   - FirebirdSQL emulation (dialect compatibility)
   - Clients can use their native tools without modification

2. **Multi-Model NoSQL Support (9 Models)**
   - **Graph Database** - Cypher (Neo4j), Gremlin (TinkerPop), and ScratchBird native dialects
   - **Vector Database** - Similarity search for AI embeddings and semantic search
   - **Document Store** - MongoDB-compatible JSON document operations
   - **Key-Value Store** - Redis-compatible atomic operations
   - **Time-Series Database** - Optimized for temporal data with retention policies
   - **Column-Family Store** - Cassandra-compatible wide-column storage (CQL)
   - **Full-Text Search Engine** - Elasticsearch-compatible search DSL
   - **Stream Processing** - Continuous queries and event stream processing
   - **Object/Blob Store** - S3-compatible API for large binary objects
   - ALL on the same underlying MGA engine with shared transaction semantics

3. **Enterprise Distributed Systems**
   - Horizontal scaling with automatic sharding
   - Multi-master replication
   - Heterogeneous clusters (ScratchBird + PostgreSQL + MySQL + MSSQL + Firebird)
   - Distributed transactions across database types
   - Query federation

4. **Modern Integration Ecosystem**
   - Kafka event streaming integration
   - Message queue support (RabbitMQ, Redis)
   - AI/automation agent APIs
   - Object storage integration (S3-compatible)
   - Observability stack (Prometheus, Grafana, OpenTelemetry)

5. **Universal Client Support**
   - Native drivers for 12+ languages
   - ODBC and JDBC standard compliance
   - Wire protocol compatibility allows existing clients to connect
   - GraphQL and REST APIs for modern applications

6. **Firebird MGA Architecture**
   - Pure Multi-Generational Architecture (not MVCC)
   - Superior transaction isolation
   - No PostgreSQL contamination
   - Battle-tested architecture refined over decades

**The End Result:**

A database engine that can:
- **Replace** up to 9+ specialized databases in a single deployment:
  - PostgreSQL, MySQL, MSSQL, FirebirdSQL (relational SQL)
  - Neo4j (graph database)
  - MongoDB (document store)
  - Redis (key-value store)
  - Cassandra (column-family store)
  - Elasticsearch (full-text search)
  - InfluxDB (time-series)
  - S3 (object storage)
  - Kafka (stream processing)
- **Integrate** with existing databases in heterogeneous clusters
- **Scale** from embedded use to massive distributed clusters
- **Support** legacy applications with full protocol compatibility
- **Enable** modern applications with NoSQL and streaming capabilities
- **Unify** data models under a single transaction engine (ACID across all models)
- **Provide** a stable, educational platform for database research and development

This is not just a database—it's a **universal data platform** that demonstrates what's possible when you combine the best ideas from relational, NoSQL, distributed systems, and stream processing into a cohesive, principled MGA architecture.

**Research & Educational Value:**

Beta 4 requires deep research into each NoSQL model's technical specifications, query semantics, and implementation strategies. This makes ScratchBird an excellent educational platform for understanding:
- How different data models map to a unified storage engine
- How query dialects translate to a common bytecode representation
- How specialized indexes enable different access patterns
- How transactional semantics extend across diverse data models

---

**Document Version:** 1.3
**Last Updated:** November 27, 2025
**Status:** OFFICIAL ROADMAP
