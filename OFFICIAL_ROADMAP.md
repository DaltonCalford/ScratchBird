# ScratchBird Official Development Roadmap

**Created:** November 20, 2025
**Last Updated:** November 20, 2025
**Status:** AUTHORITATIVE - Official development phases and goals
**Current Phase:** Alpha 1 (99% Complete)

**Project Nature:** This is an educational/development project with **NO fixed timeframe constraints**. Each stage is complete when ALL defined elements are implemented, not based on time estimates.

---

## ⚠️ IMPORTANT: Production Readiness

**ScratchBird should NEVER be referred to as "Production Ready" until Gold Release.**

The term "production-ready" in technical documentation refers to **component stability** within the development environment, NOT suitability for production deployment.

---

## Roadmap Overview

```
ALPHA STAGE (Embedded Engine)
├── Alpha 1: Engine Functionality (99% Complete) ← CURRENT
├── Alpha 2: Parser Separation (Not Started)
└── Alpha 3: Network Listeners (Not Started)

BETA STAGE (Distributed Systems)
├── Beta 1: Cluster Implementation (Not Started)
├── Beta 2: Heterogeneous Clusters (Not Started)
└── Beta 3: Encryption & Advanced Indexes (Not Started)

RELEASE CANDIDATE STAGE (Stabilization)
├── RC1: Feature Complete + Beta User Testing (Not Started)
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

**Status:** 99% Complete
**Current Document:** `/docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md`

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

**Built-in Functions (123/123 - 100%)**
- String (11), Aggregate (6), Window (8)
- JSON (13), Array (12), Date/Time (6)
- Mathematical (29), Bit Manipulation (14)
- Cryptographic (4), Statistical (7)
- XML (9), Spatial (40+), Regex (4)
- Text Search, Conditional (3)

**Security System (100% - Phase 3.5 Complete)**
- User/role/group management
- Permission system (table, column, object-level)
- Row-Level Security (RLS) with DML enforcement
- GRANT/REVOKE statements
- SQL SECURITY DEFINER/INVOKER
- Password hashing (BCrypt)
- Permission cache (LRU, 60s TTL)

**Constraints (90% Complete)**
- NOT NULL, UNIQUE, PRIMARY KEY
- FOREIGN KEY (single and composite)
- CHECK constraints
- DEFAULT expressions
- Referential actions (CASCADE, SET NULL, SET DEFAULT)

**DDL Operations (Complete)**
- CREATE/ALTER/DROP TABLE
- CREATE/DROP INDEX
- CREATE/ALTER/DROP SEQUENCE
- CREATE/DROP VIEW (regular and materialized)
- ALTER TABLE (ADD/DROP/RENAME COLUMN, ALTER COLUMN TYPE)
- CREATE/ALTER/DROP TABLESPACE
- All security DDL (USER, ROLE, GROUP, POLICY)

#### ⧗ IN PROGRESS (Components)

**Views (80% Complete)**
- ✅ CREATE VIEW / CREATE OR REPLACE VIEW
- ✅ CREATE MATERIALIZED VIEW
- ✅ DROP VIEW [IF EXISTS] [CASCADE | RESTRICT]
- ✅ REFRESH [CONCURRENTLY] MATERIALIZED VIEW
- ✅ Query expansion (SELECT from views → underlying tables)
- ✅ Column projection
- ✅ WITH CHECK OPTION (parser + catalog)
- ⧗ Physical materialization (table creation + data population) - **20% remaining**
- ⧗ Updatable views (INSERT/UPDATE/DELETE through views)

**Catalog CRUD (58% Complete)**
- ⧗ Stored code operations (Procedures, Parameters, Domains, UDR, Packages)
- ⧗ Emulation table operations (Types, Servers, Databases)
- ⧗ Some infrastructure operations (Statistics)

#### ❌ NOT IMPLEMENTED (Critical Gaps)

**PSQL/Stored Procedures**
- ⧗ Bytecode execution (90% stubbed)
- ❌ Trigger firing (CREATE works, execution doesn't)
- ❌ Exception handling (TRY/CATCH)
- ❌ Cursors (result set iteration)

**Advanced SQL Features**
- ❌ Common Table Expressions (CTEs) - WITH clause
- ❌ Recursive queries (WITH RECURSIVE)
- ❌ MERGE statement (complex upsert)
- ❌ RETURNING clause (INSERT/UPDATE/DELETE)

**Missing Constraint Features**
- ❌ GENERATED columns (STORED/VIRTUAL)
- ❌ IDENTITY columns (auto-increment)
- ❌ Deferred constraint checking

**SQL Engine Internal Commands**
- ❌ SHOW TABLES / SHOW DATABASES / SHOW COLUMNS
- ❌ DESCRIBE TABLE
- ❌ EXPLAIN query plans
- ❌ System catalog queries (pg_catalog equivalent)

**Command-Line Tools**
- ❌ sb_isql (interactive SQL shell)
- ❌ sb_verify (database integrity checker)
- ❌ sb_backup (backup/restore tool)
- ❌ sb_security (user/role management tool)

### Alpha 1 Completion Criteria

**ALL items below MUST be complete before Alpha 1 is considered done:**

1. ✅ All 11 index types functional
2. ✅ All 86 data types supported
3. ✅ All 123 built-in functions implemented
4. ✅ Security system complete (users, roles, permissions, RLS)
5. ✅ Constraint enforcement (CHECK, FK, UNIQUE, PK, DEFAULT)
6. ⧗ Views fully functional (materialized + updatable) - **80% complete**
7. ❌ PSQL/stored procedure execution
8. ❌ Trigger firing mechanism
9. ❌ CTEs and recursive queries
10. ❌ MERGE statement
11. ❌ RETURNING clause
12. ❌ GENERATED/IDENTITY columns
13. ❌ Deferred constraint checking
14. ❌ SQL engine internal commands (SHOW, DESCRIBE, EXPLAIN)
15. ❌ Command-line tools (sb_isql, sb_verify, sb_backup, sb_security)

**No deferrals. No "nice to have" items. Alpha 1 complete = ALL local functionality complete.**

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

# BETA STAGE: Distributed Systems

**Goal:** Enterprise-grade distributed database system

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

# RELEASE CANDIDATE STAGE: Stabilization

**Goal:** Feature-complete, thoroughly tested, ready for production evaluation

---

## RC1: Feature Complete + Beta User Testing

**Status:** Not Started
**Dependencies:** Beta 3 must be 100% complete
**Goal:** All planned features implemented, initial testing and debugging

### Scope

**Feature Freeze:**
- **NO new features** after this point
- Only bug fixes and performance improvements
- Documentation finalization
- **ALL features from Alpha 1-3 and Beta 1-3 must be complete**

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
2. **Beta User Documentation** - Installation, configuration, migration guides
3. **Known Issues List** - Public tracker of known bugs and limitations
4. **Performance Baselines** - Benchmark results for reference
5. **Security Audit Report** - External security review findings
6. **Test Coverage Report** - Code coverage statistics

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

**Documentation:**
- 100% of features documented
- Migration guides tested by beta users
- All examples verified to work

**Beta User Feedback:**
- Positive feedback from majority of beta testers
- No showstopper issues reported
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
3. ✅ All planned features complete (Alpha 1-3, Beta 1-3)
4. ✅ 95%+ test coverage
5. ✅ No memory leaks in 7-day stress test
6. ✅ Cluster scales to 10+ nodes with linear performance
7. ✅ All 4 wire protocols fully compliant
8. ✅ Security audit passed (no critical/high vulnerabilities)
9. ✅ Encryption working and audited

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

**Document Version:** 1.0
**Last Updated:** November 20, 2025
**Status:** OFFICIAL ROADMAP
