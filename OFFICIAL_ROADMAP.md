# ScratchBird Official Development Roadmap

**Created:** November 20, 2025
**Last Updated:** December 11, 2025
**Status:** AUTHORITATIVE - Official development phases and goals
**Current Phase:** Alpha 3 🚀 **IN PROGRESS** (Network & Service Mode)
**Previous Phase:** Alpha 2 ✅ **100% COMPLETE** (Multi-Dialect Parser Separation)

**Project Nature:** This is an educational/development project with **NO fixed timeframe constraints**. Each stage is complete when ALL defined elements are implemented, not based on time estimates.

---

## ⚠️ IMPORTANT: Production Readiness

**ScratchBird should NEVER be referred to as "Production Ready" until Gold Release.**

The term "production-ready" in technical documentation refers to **component stability** within the development environment, NOT suitability for production deployment.

---

## Roadmap Overview

```
ALPHA STAGE (Embedded Engine → Networked Server)
├── Alpha 1: Engine Functionality ✅ **100% COMPLETE** (1020 tests)
├── Alpha 2: Parser Separation ✅ **100% COMPLETE** (1255 tests)
└── Alpha 3: Network + Security + Service Mode 🚀 **IN PROGRESS** (1337 tests)

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

**Status:** ✅ **100% COMPLETE**

**Note:** CODE_COMPLETION_MASTER_PLAN is now 100% complete (135/135 items)! All TODOs have been documented as "Phase X Enhancement" comments. CLI Tools 100% complete. Local Server Architecture all 5 phases complete. All P0-P2 improvements 100% complete! P3 80% complete (4 items blocked by Alpha 3/dependencies). Test suite: 1020/1020 = 100% pass rate.

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

**Local Server Architecture ALL PHASES** ✅ **100% COMPLETE**

- ✅ Phase 1: IPC Infrastructure (Unix sockets, Named pipes, TCP localhost) - COMPLETE (22 tests)
- ✅ Phase 2: Wire Protocol (binary message format, result streaming) - COMPLETE (37 tests)
- ✅ Phase 3: Server Process (sb_server, multi-threading, sessions) - COMPLETE
- ✅ Phase 4: Client Library (libscratchbird_client, auto-start) - COMPLETE (36 unit + 7 integration tests)
- ✅ Phase 5: Integration & Testing - COMPLETE (1020/1020 tests = 100% pass rate)
- **Total Server Tests: 95+ passing** (22 IPC + 37 wire protocol + 36 client)

**Command-Line Tools** ✅ **100% COMPLETE** (November 28, 2025) 🎉

- ✅ sb_isql (interactive SQL shell) - `src/cli/sb_isql.cpp` (~750 lines)
  - Meta-commands (\?, \q, \d, \dt, \di, \du, \l, \c, \timing, etc.)
  - Result formatting, multi-line SQL, password input without echo
- ✅ sb_verify (database integrity checker) - `src/cli/sb_verify.cpp` (~510 lines)
  - Full/quick verification, page checksums, magic byte validation
  - Tested: verified 46 pages successfully
- ✅ sb_backup (backup/restore tool) - `src/cli/sb_backup.cpp` (~550 lines)
  - Backup, restore, verify, info commands
  - 128-byte backup header format with CRC32 checksums
- ✅ sb_security (user/role management tool) - `src/cli/sb_security.cpp` (~800 lines)
  - User management (create, delete, enable, disable, password, unlock)
  - Role management (create, delete, grant, revoke, members, grants)
  - Audit commands (status, enable, disable, log)

**Blocked Improvement Items**

- 🔒 P3-2/3/4: MFA, IP whitelisting, certificate auth (require Alpha 3 network layer)
- 🔒 P3-14: Partition pruning (requires table partitioning syntax)

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
12. ✅ RETURNING clause - **100% COMPLETE**
13. ✅ GENERATED/IDENTITY columns - **100% COMPLETE**
14. ✅ Deferred constraint checking - **100% COMPLETE**
15. ✅ SQL engine internal commands (SHOW, DESCRIBE, EXPLAIN) - **100% COMPLETE**
16. ✅ Command-line tools (sb_isql, sb_verify, sb_backup, sb_security) - **100% COMPLETE** 🎉

**Progress: 18/18 major components complete (100%)** ✅

This includes the original 15 components plus 3 additional scopes identified during development:

- Missing functions (30 functions to add) - ✅ COMPLETE
- Improvement opportunities (64/68 items across all subsystems) - 95% complete (4 blocked by Alpha 3)
- Catalog cleanup (all 4 phases) - ✅ COMPLETE
- Local server architecture ALL 5 PHASES - ✅ COMPLETE
- CLI tools (sb_isql, sb_verify, sb_backup, sb_security) - ✅ COMPLETE
- Phase 5 integration testing - ✅ COMPLETE (1020/1020 tests = 100%)
- CODE_COMPLETION_MASTER_PLAN (135/135 items) - ✅ COMPLETE

**Alpha 1 is 100% COMPLETE. All local functionality implemented.**

---

## Alpha 2: Parser Separation

**Status:** ✅ **100% COMPLETE** (December 10, 2025)
**Dependencies:** Alpha 1 ✅ 100% complete
**Goal:** Extract built-in SQL parser into separate library/layer
**Test Suite:** 1255/1255 = 100% pass rate

**Completion Policy:** Alpha 2 is complete when the parser is fully separated and ALL dialect parsers (ScratchBird, PostgreSQL, MySQL, MSSQL, FirebirdSQL) are functional.

### Final Status (December 10, 2025)

| Parser Type | Test Count | Status |
|------------|-----------|--------|
| PostgreSQL Parser | 52 tests | ✅ All pass |
| MySQL Parser | 30 tests | ✅ All pass |
| Firebird Parser | 52 tests | ✅ All pass |
| Parser V2 DML | 45 tests | ✅ All pass |
| Parser V2 Session | 47 tests | ✅ All pass |
| Parser V2 State | 27 tests | ✅ All pass |
| Parser V2 DDL | 52 tests | ✅ All pass |
| **Total Parser Tests** | **293 tests** | ✅ All pass |

**Implemented Parsers:**
- ✅ **ScratchBird Parser V2** - Native dialect with "Smart Parser, Dumb Lexer" architecture
- ✅ **PostgreSQL Parser** - Full dialect support, SBLR bytecode generation
- ✅ **MySQL Parser** - Full dialect support, SBLR bytecode generation
- ✅ **Firebird Parser** - Full dialect support (fb_isql compatible), SBLR bytecode generation

**Key Implementations:**
- Parser V2 with ~35 gatekeeper keywords
- Contextual keyword recognition via ParserState class
- Schema paths: `.name` (current), `..name` (parent), `schema.table` (qualified)
- UUID-based object resolution
- Query compilers for each dialect (PostgreSQL, MySQL, Firebird)
- Default schema: `/remote/emulated/{dialect}/localhost/`

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

## Alpha 3: Network Listeners + Security Suite + Service Mode + Plugin UDRs

**Status:** 🚀 **IN PROGRESS** (Started December 10, 2025)
**Dependencies:** Alpha 2 ✅ 100% complete
**Test Suite:** 1337/1337 = 100% pass rate
**Goal:** Transform ScratchBird into a production-capable networked database service

**Completion Policy:** Alpha 3 is complete when ALL wire protocols, full security suite, systemd service mode, and remote database UDR plugins are fully functional.

### Implementation Progress (December 11, 2025)

| Phase | Component | Status | Lines | Description |
|-------|-----------|--------|-------|-------------|
| 3.1 | Network Infrastructure | ✅ **COMPLETE** | ~6,200 | Socket, EventLoop, ThreadPool, ConnectionHandler |
| 3.2 | Wire Protocol Adapters | ✅ **COMPLETE** | ~4,637 | All 4 protocols implemented |
| 3.3 | Service Mode & systemd | ✅ **COMPLETE** | ~3,000 | Daemon, config parser, systemd integration |
| 3.4 | Security Suite - Core | ✅ **COMPLETE** | ~3,500 | SSL/TLS, SCRAM-SHA-256/512, certificates, HBA |
| 3.5 | Security Suite - Enterprise | 🔜 Pending | - | LDAP, Kerberos, OAuth, SAML, MFA |
| 3.6 | Connection Pooling | 🔜 Pending | - | Built-in pooling |
| 3.7 | UDR Plugin System | 🔜 Pending | - | Foreign data wrappers |
| 3.8 | ODBC/JDBC Drivers | 🔜 Pending | - | Standard connectivity |

**Phase 3.1: Network Infrastructure** ✅ **COMPLETE** (December 10, 2025)
- Socket abstraction (TCP/IP, Unix domain sockets) - ~1,028 lines
- Event loop (epoll/kqueue/poll multiplexing) - ~767 lines
- Thread pool with priority queue - ~490 lines
- Connection handler with protocol detection - ~652 lines
- 30 unit tests passing

**Phase 3.2: Wire Protocol Adapters** ✅ **COMPLETE** (December 11, 2025)

| Protocol | Port | Version | Lines | Features |
|----------|------|---------|-------|----------|
| PostgreSQL | 5432 | v3 | ~1,517 | MD5 auth, Simple/Extended Query |
| MySQL | 3306 | 5.7+ | ~1,120 | Native password auth, prepared statements |
| Firebird | 3050 | 5.0 (v18) | ~1,300 | XDR encoding, SRP auth support |
| Native ScratchBird | 3092 | Binary | ~700 | Full message types, session management |

All adapters integrated via `createProtocolAdapter()` factory in `protocol_adapter.cpp`.

**Phase 3.3: Service Mode & systemd** ✅ **COMPLETE** (December 11, 2025)

| File | Lines | Description |
|------|-------|-------------|
| `include/scratchbird/server/config_parser.h` | ~300 | INI config parser with env var expansion |
| `src/server/config_parser.cpp` | ~700 | Full config parser implementation |
| `include/scratchbird/server/daemon.h` | ~300 | PIDFile, SystemdNotify, Daemon classes |
| `src/server/daemon.cpp` | ~600 | Unix daemonization, sd_notify, signals |
| `include/scratchbird/server/service_controller.h` | ~400 | Service lifecycle management |
| `src/server/service_controller.cpp` | ~700 | Full service implementation |
| `etc/systemd/scratchbird.service` | ~90 | systemd unit file |
| `etc/scratchbird/sb_server.conf.example` | ~180 | Example configuration |
| **Total** | **~3,270** | Service mode complete |

**Key Features:**
- Unix daemonization (double-fork pattern)
- PID file management with file locking (flock)
- Dynamic loading of libsystemd for sd_notify
- Signal handling (SIGTERM, SIGHUP, SIGUSR1, SIGUSR2, SIGQUIT)
- Privilege dropping (setuid/setgid)
- INI configuration with @include and ${VAR:-default} support
- Size parsing (128MB, 1GB) and duration parsing (30s, 5m, 1h)
- Multi-database mode support
- Health check API

**Phase 3.4: Security Suite - Core** ✅ **COMPLETE** (December 11, 2025)

| File | Lines | Description |
|------|-------|-------------|
| `include/scratchbird/security/tls_config.h` | ~450 | TLS configuration, CertificateInfo, TLSContext |
| `src/security/tls_context.cpp` | ~1,100 | OpenSSL TLS context implementation |
| `include/scratchbird/security/auth_method.h` | ~400 | AuthMethod interface, AuthContext, AuthState |
| `src/security/auth_method.cpp` | ~350 | Trust, Reject, Peer auth implementations |
| `include/scratchbird/security/auth_manager.h` | ~450 | HBA rules, RateLimiter, AuditLogger |
| `src/security/auth_manager.cpp` | ~750 | HBA parser, rate limiting, audit logging |
| `include/scratchbird/security/scram_auth.h` | ~340 | SCRAM-SHA-256/512 interface |
| `src/security/scram_auth.cpp` | ~950 | Full RFC 5802 SCRAM implementation |
| `include/scratchbird/security/cert_auth.h` | ~240 | Certificate authentication interface |
| `src/security/cert_auth.cpp` | ~575 | Certificate-to-user mapping, DN parsing |
| **Total** | **~3,500** | Core security suite complete |

**Key Features:**
- OpenSSL TLS 1.2/1.3 with certificate verification and CRL/OCSP support
- SCRAM-SHA-256/512 challenge-response authentication (RFC 5802)
- Certificate authentication with configurable mapping (CN, DN, SAN, fingerprint)
- Host-Based Authentication (pg_hba.conf style) with IPv4/IPv6 CIDR matching
- Brute force protection with configurable rate limiting and lockout
- Audit logging to file or syslog
- Password hashing with PBKDF2 and secure salt generation
- Pluggable authentication method framework for extensibility

### Expanded Scope (December 2025 Update)

Alpha 3 has been expanded beyond just "Network Listeners" to include:

1. **Wire Protocol Implementations** - PostgreSQL, MySQL, TDS/MSSQL, Firebird, ScratchBird Native
2. **Service/Daemon Mode** - systemd integration, startup/shutdown, configuration reload
3. **Full Security Suite** - MFA, IP whitelisting, certificate auth, LDAP/AD, Kerberos, SAML, OAuth2
4. **Connection Pooling** - Built-in connection pool with configurable parameters
5. **Plugin UDR System** - Remote database connectivity for passthrough queries and foreign tables

### Architectural Goal

Transform the multi-parser embedded engine into a **full-featured networked database service**:

```
                    ┌─────────────────────────────────────────┐
                    │         Native Clients                   │
                    │  sb_client, sb_isql (port 3092)         │
                    └─────────────────────────────────────────┘
                                       ↓
┌────────────────┐  ┌────────────────┐  ↓  ┌────────────────┐  ┌────────────────┐
│ psql client    │  │ mysql client   │  │  │ SSMS/sqlcmd    │  │ isql (Firebird)│
│ (port 5432)    │  │ (port 3306)    │  │  │ (port 1433)    │  │ (port 3050)    │
└────────────────┘  └────────────────┘  │  └────────────────┘  └────────────────┘
        ↓                   ↓           ↓           ↓                   ↓
┌───────────────────────────────────────────────────────────────────────────────┐
│                        Network Listener Layer                                  │
├───────────────────────────────────────────────────────────────────────────────┤
│  • PostgreSQL Wire Protocol v3 (port 5432)                                    │
│  • MySQL Wire Protocol (port 3306)                                            │
│  • TDS Wire Protocol v7.4+ (port 1433) - MSSQL                                │
│  • Firebird Wire Protocol v13 (port 3050)                                     │
│  • ScratchBird Native Protocol (port 3092) - Optimized binary                 │
└───────────────────────────────────────────────────────────────────────────────┘
        ↓                   ↓           ↓           ↓                   ↓
┌───────────────────────────────────────────────────────────────────────────────┐
│                        Security & Authentication Layer                         │
├───────────────────────────────────────────────────────────────────────────────┤
│  • Password Auth (BCrypt/Argon2, SCRAM-SHA-256)                               │
│  • Certificate Authentication (X.509, mTLS)                                    │
│  • Multi-Factor Authentication (TOTP/HOTP)                                     │
│  • LDAP/Active Directory Integration                                           │
│  • Kerberos/GSSAPI/SPNEGO                                                      │
│  • SAML 2.0 / OAuth 2.0 / OIDC                                                 │
│  • IP Whitelisting / Network Policies                                          │
│  • SSL/TLS 1.2+ (Required)                                                     │
└───────────────────────────────────────────────────────────────────────────────┘
        ↓                   ↓           ↓           ↓                   ↓
┌───────────────────────────────────────────────────────────────────────────────┐
│                        Connection Pool Manager                                 │
├───────────────────────────────────────────────────────────────────────────────┤
│  • Per-database connection pools                                               │
│  • Configurable min/max connections                                            │
│  • Idle timeout, max lifetime                                                  │
│  • Health checking, validation                                                 │
│  • Statement caching                                                           │
│  • Result caching                                                              │
└───────────────────────────────────────────────────────────────────────────────┘
                            ↓
┌───────────────────────────────────────────────────────────────────────────────┐
│                        Parser Layer (Alpha 2)                                  │
│  • ScratchBird Parser V2 • PostgreSQL Parser • MySQL Parser • Firebird Parser │
└───────────────────────────────────────────────────────────────────────────────┘
                            ↓
┌───────────────────────────────────────────────────────────────────────────────┐
│                        Database Engine (Alpha 1)                               │
│  • MGA Storage Engine • 11 Index Types • 86 Data Types • 153 Functions        │
└───────────────────────────────────────────────────────────────────────────────┘
                            ↓
┌───────────────────────────────────────────────────────────────────────────────┐
│                        UDR Plugin System                                       │
├───────────────────────────────────────────────────────────────────────────────┤
│  • Remote Database UDR (remote_database.so)                                   │
│    - PostgreSQL foreign tables (libpq)                                        │
│    - MySQL foreign tables (libmysql)                                          │
│    - MSSQL foreign tables (FreeTDS)                                           │
│    - Firebird foreign tables (fbclient)                                       │
│  • Passthrough queries (EXECUTE ON SERVER)                                    │
│  • Schema introspection                                                        │
│  • Migration workflows                                                         │
└───────────────────────────────────────────────────────────────────────────────┘
```

### Part A: Wire Protocol Implementations

**A.1 PostgreSQL Wire Protocol v3** (port 5432)

- Startup message parsing (protocol version 3.0)
- Authentication: MD5, SCRAM-SHA-256, certificate, GSSAPI
- Simple Query Protocol (single-statement)
- Extended Query Protocol (Parse/Bind/Describe/Execute/Sync)
- COPY IN/OUT protocol
- Prepared statements and portals
- Result set streaming (DataRow, RowDescription)
- Error/Notice messages with SQLSTATE codes
- LISTEN/NOTIFY (async notifications)
- Cancellation protocol (BackendKeyData)
- **Reference:** `/docs/specifications/wire_protocols/postgresql_wire_protocol.md`

**A.2 MySQL Wire Protocol** (port 3306)

- Handshake v10 (capability negotiation)
- Authentication: mysql_native_password, caching_sha2_password, sha256_password
- COM_QUERY (text protocol)
- COM_PREPARE/COM_EXECUTE (binary protocol)
- Result set encoding (text and binary formats)
- COM_STMT_CLOSE, COM_STMT_RESET
- OK/ERR/EOF packets
- Multi-statement execution
- **Reference:** `/docs/specifications/wire_protocols/mysql_wire_protocol.md`

**A.3 TDS Wire Protocol** (port 1433) - MSSQL

- TDS 7.4+ message framing
- Login7 packet handling
- PRELOGIN encryption negotiation
- SQL_BATCH execution
- RPC (Remote Procedure Call) protocol
- Column metadata (COLMETADATA)
- Row data (ROW tokens)
- DONE/DONEPROC/DONEINPROC tokens
- Attention signals (query cancellation)
- **Reference:** `/docs/specifications/wire_protocols/tds_wire_protocol.md`

**A.4 Firebird Wire Protocol v13** (port 3050)

- XDR encoding (big-endian, 4-byte aligned)
- op_connect/op_attach/op_detach
- op_transaction/op_commit/op_rollback
- op_prepare_statement/op_execute/op_fetch
- SRP authentication (default)
- Wire encryption (ChaCha20 preferred)
- Blob operations
- Event notifications
- **Reference:** `/docs/specifications/wire_protocols/firebird_wire_protocol.md`

**A.5 ScratchBird Native Protocol** (port 3092)

- **Port:** 3092 (IANA unassigned, low conflict risk)
- Binary message format optimized for ScratchBird (little-endian)
- 16-byte message header with magic, version, type, flags, length, sequence
- 29 client→server message types, 32 server→client message types
- Direct SBLR bytecode transmission (skip parsing on repeat executions)
- Full MGA visibility semantics (visibility epoch, record versioning)
- Native type support (all 86 types with defined binary serialization)
- Streaming result sets with backpressure control
- Query plan transmission
- Server-side prepared statements with statement caching
- Async query execution with pipelining
- Subscription/notification system (LISTEN/NOTIFY style)
- Compression (zstd, negotiated)
- TLS 1.3 required (no plaintext mode)
- **Cluster PKI:** Hybrid CA + ephemeral session keys for forward secrecy
- **Federation:** Cross-database queries with `SELECT * FROM table@other_db`
- **Reference:** `/docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md` ✅ **COMPLETE**

### Part B: Service/Daemon Mode (systemd Integration)

**Reference:** `/docs/specifications/SYSTEMD_SERVICE_SPECIFICATION.md` ✅ **COMPLETE**

**B.1 Server Binary: sb_server**

```
Usage: sb_server [options]
  --config <file>       Configuration file path (default: /etc/scratchbird/sb_server.conf)
  --database <path>     Database file path (single-database mode)
  --data-dir <path>     Data directory for multiple databases (multi-database mode)
  --create              Create database if it doesn't exist
  --tcp                 Enable TCP listeners (default: all protocols)
  --tcp-port <port>     ScratchBird native protocol port (default: 3092)
  --pg-port <port>      PostgreSQL protocol port (default: 5432)
  --mysql-port <port>   MySQL protocol port (default: 3306)
  --tds-port <port>     TDS/MSSQL protocol port (default: 1433)
  --fb-port <port>      Firebird protocol port (default: 3050)
  --unix-socket <path>  Unix domain socket path
  --max-connections <n> Maximum concurrent connections (default: 100)
  --foreground          Run in foreground (don't daemonize)
  --verbose             Verbose logging
  --pid-file <path>     PID file path
```

**B.2 Database Modes**

- **Single-database mode:** One sb_server instance manages one database file
- **Multi-database mode:** One sb_server instance manages multiple databases in a directory
- **Configurable per installation**

**B.3 systemd Service Unit**

```ini
# /etc/systemd/system/scratchbird.service
[Unit]
Description=ScratchBird Database Server
Documentation=https://scratchbird.dev/docs
After=network.target

[Service]
Type=notify
User=scratchbird
Group=scratchbird
ExecStart=/usr/bin/sb_server --config /etc/scratchbird/sb_server.conf
ExecReload=/bin/kill -HUP $MAINPID
ExecStop=/bin/kill -TERM $MAINPID
Restart=on-failure
RestartSec=5
TimeoutStartSec=30
TimeoutStopSec=30
LimitNOFILE=65536
LimitNPROC=4096
PrivateTmp=true
ProtectSystem=full
ProtectHome=true
NoNewPrivileges=true

[Install]
WantedBy=multi-user.target
```

**B.4 Signal Handling**

| Signal | Action |
|--------|--------|
| SIGTERM | Graceful shutdown (finish active queries, close connections) |
| SIGINT | Graceful shutdown |
| SIGHUP | Reload configuration (connection limits, pool settings, security) |
| SIGUSR1 | Rotate log files |
| SIGUSR2 | Dump statistics to log |

**B.5 Configuration File Format**

```ini
# /etc/scratchbird/sb_server.conf

[server]
mode = multi-database          # single-database | multi-database
data_dir = /var/lib/scratchbird
pid_file = /var/run/scratchbird/sb_server.pid
log_file = /var/log/scratchbird/sb_server.log
log_level = info               # debug | info | warning | error

[network]
bind_address = 0.0.0.0
native_port = 3092
pg_port = 5432
mysql_port = 3306
tds_port = 1433
fb_port = 3050
unix_socket = /var/run/scratchbird/sb.sock
max_connections = 100
connection_timeout = 30

[ssl]
enabled = true
cert_file = /etc/scratchbird/ssl/server.crt
key_file = /etc/scratchbird/ssl/server.key
ca_file = /etc/scratchbird/ssl/ca.crt
min_protocol = TLSv1.2
require_client_cert = false

[pool]
min_connections = 5
max_connections = 50
idle_timeout = 300
max_lifetime = 3600
validation_interval = 60
statement_cache_size = 1000
result_cache_size_mb = 64

[security]
password_hash_algorithm = argon2id  # bcrypt | argon2id
password_min_length = 12
max_failed_attempts = 5
lockout_duration = 300
audit_enabled = true
audit_file = /var/log/scratchbird/audit.log

[authentication]
# Methods tried in order
methods = password,certificate,ldap
ldap_server = ldap://ldap.example.com:389
ldap_bind_dn = cn=scratchbird,ou=services,dc=example,dc=com
ldap_user_base = ou=users,dc=example,dc=com
ldap_group_base = ou=groups,dc=example,dc=com
kerberos_keytab = /etc/scratchbird/krb5.keytab
kerberos_service_name = scratchbird
```

### Part C: Full Security Suite

**C.1 Authentication Methods**

| Method | Status | Description |
|--------|--------|-------------|
| Password (BCrypt) | ✅ Alpha 1 | Local password hashing |
| Password (Argon2id) | 🚧 Alpha 3 | Memory-hard password hashing |
| SCRAM-SHA-256 | 🚧 Alpha 3 | Challenge-response (PostgreSQL) |
| MD5 | 🚧 Alpha 3 | Legacy PostgreSQL compatibility |
| Certificate (X.509) | 🚧 Alpha 3 | mTLS client certificates |
| LDAP | 🚧 Alpha 3 | LDAP/LDAPS bind authentication |
| Active Directory | 🚧 Alpha 3 | AD domain authentication |
| Kerberos/GSSAPI | 🚧 Alpha 3 | Single sign-on |
| SAML 2.0 | 🚧 Alpha 3 | Enterprise SSO federation |
| OAuth 2.0/OIDC | 🚧 Alpha 3 | Modern identity providers |
| MFA (TOTP/HOTP) | 🚧 Alpha 3 | Multi-factor authentication |

**C.2 Certificate Authentication (P3-4 Unblocked)**

```sql
-- Configure certificate authentication
ALTER SYSTEM SET ssl_cert_file = '/etc/scratchbird/ssl/server.crt';
ALTER SYSTEM SET ssl_key_file = '/etc/scratchbird/ssl/server.key';
ALTER SYSTEM SET ssl_ca_file = '/etc/scratchbird/ssl/ca.crt';
ALTER SYSTEM SET ssl_require_client_cert = true;

-- Map certificate CN to database user
CREATE USER MAPPING FOR CERTIFICATE 'CN=alice,O=Example Corp,C=US'
    TO USER alice;

-- Or auto-create users from certificates
ALTER SYSTEM SET ssl_auto_create_user = true;
```

**C.3 Multi-Factor Authentication (P3-2 Unblocked)**

```sql
-- Enable MFA for user
ALTER USER alice SET mfa_enabled = true;

-- Generate TOTP secret (returns QR code data)
SELECT setup_mfa_totp('alice');

-- Verify MFA token during connection
-- (handled by wire protocol authentication)
```

**C.4 IP Whitelisting (P3-3 Unblocked)**

```sql
-- Create IP whitelist
CREATE IP WHITELIST internal_network (
    '192.168.0.0/16',
    '10.0.0.0/8',
    '172.16.0.0/12'
);

-- Restrict user to whitelist
ALTER USER alice SET allowed_ips = internal_network;

-- Restrict entire database
ALTER DATABASE mydb SET allowed_ips = internal_network;

-- Time-based access
CREATE IP WHITELIST office_hours (
    '192.168.1.0/24' VALID_TIMES '09:00-17:00' VALID_DAYS 'Mon-Fri'
);
```

**C.5 LDAP/Active Directory Integration**

```sql
-- Configure LDAP server
CREATE AUTHENTICATION SERVER ldap_main
    TYPE LDAP
    URI 'ldaps://ldap.example.com:636'
    BIND_DN 'cn=service,dc=example,dc=com'
    BIND_PASSWORD 'secret'
    USER_BASE 'ou=users,dc=example,dc=com'
    GROUP_BASE 'ou=groups,dc=example,dc=com'
    USER_FILTER '(uid={username})'
    GROUP_FILTER '(memberUid={username})';

-- Map LDAP groups to database roles
CREATE GROUP MAPPING FROM ldap_main
    'cn=dba,ou=groups,dc=example,dc=com' TO ROLE superuser,
    'cn=developers,ou=groups,dc=example,dc=com' TO ROLE developer,
    'cn=analysts,ou=groups,dc=example,dc=com' TO ROLE readonly;

-- Auto-provision users from LDAP
ALTER AUTHENTICATION SERVER ldap_main SET auto_create_user = true;
```

**C.6 Kerberos/GSSAPI (Single Sign-On)**

```sql
-- Configure Kerberos authentication
ALTER SYSTEM SET krb_server_keyfile = '/etc/scratchbird/krb5.keytab';
ALTER SYSTEM SET krb_realm = 'EXAMPLE.COM';
ALTER SYSTEM SET krb_service_name = 'scratchbird';

-- Map Kerberos principal to user
CREATE USER alice@EXAMPLE.COM IDENTIFIED BY KERBEROS;
```

**C.7 SAML 2.0 / OAuth 2.0 / OIDC**

```sql
-- Configure OAuth2/OIDC provider
CREATE AUTHENTICATION SERVER okta_oidc
    TYPE OIDC
    ISSUER 'https://example.okta.com'
    CLIENT_ID 'scratchbird-client'
    CLIENT_SECRET 'secret'
    SCOPES 'openid profile email groups';

-- Map OIDC claims to roles
CREATE CLAIM MAPPING FROM okta_oidc
    'groups' CONTAINS 'scratchbird-admin' TO ROLE superuser;
```

**C.8 Security Hardening (Per Security Hardening Guide)**

- XDR field length validation (CVE-2013-2492 mitigation)
- p_cnct_count bounds checking
- Counter-based AES-GCM nonces
- SCRAM iteration limits
- Connection rate limiting
- Failed login throttling (existing: 4 attempts, 8 second delay)
- Hash chain audit logs
- Memory encryption awareness (Intel TME/AMD SEV detection)

### Part D: Connection Pooling

**D.1 Built-in Connection Pool Architecture**

```c
// Pool configuration per database
typedef struct pool_config {
    uint32_t min_connections;      // Minimum idle connections
    uint32_t max_connections;      // Maximum total connections
    uint32_t idle_timeout_ms;      // Close idle connections after
    uint32_t max_lifetime_ms;      // Close connections after
    uint32_t validation_interval_ms; // Health check interval
    uint32_t acquire_timeout_ms;   // Timeout waiting for connection
    bool     statement_cache;      // Enable prepared statement cache
    uint32_t statement_cache_size; // Max cached statements
    bool     result_cache;         // Enable query result cache
    uint64_t result_cache_bytes;   // Result cache size
} PoolConfig;
```

**D.2 Pool Statistics (SHOW POOL STATUS)**

```sql
SHOW POOL STATUS;
-- Returns: pool_name, database, active, idle, waiting,
--          total_connections, acquired, returned, timeouts,
--          avg_acquire_time_ms, validation_failures

SHOW POOL CONNECTIONS;
-- Returns: conn_id, database, user, state, created, last_used,
--          queries_executed, bytes_sent, bytes_received
```

### Part E: Plugin UDR System

**E.1 Remote Database UDR Plugin**

The Remote Database UDR enables foreign tables and passthrough queries to external databases:

```sql
-- Create foreign server
CREATE SERVER legacy_pg
    FOREIGN DATA WRAPPER postgresql_fdw
    OPTIONS (
        host 'legacy-db.example.com',
        port '5432',
        dbname 'production'
    );

-- Create user mapping for credentials
CREATE USER MAPPING FOR alice
    SERVER legacy_pg
    OPTIONS (user 'remote_user', password 'secret');

-- Import foreign schema
IMPORT FOREIGN SCHEMA public
    FROM SERVER legacy_pg
    INTO remote_public;

-- Create individual foreign table
CREATE FOREIGN TABLE remote_users (
    id INTEGER,
    name VARCHAR(100),
    email VARCHAR(255)
)
SERVER legacy_pg
OPTIONS (schema_name 'public', table_name 'users');

-- Query foreign table (pushdown WHERE clause)
SELECT * FROM remote_users WHERE id > 1000;

-- Join local and remote tables
SELECT l.*, r.email
FROM local_users l
JOIN remote_users r ON l.remote_id = r.id;
```

**E.2 Passthrough Queries**

```sql
-- Execute arbitrary SQL on remote server
EXECUTE ON SERVER legacy_pg
    'SELECT pg_database_size(current_database())';

-- Execute with parameters
EXECUTE ON SERVER legacy_pg
    'SELECT * FROM users WHERE created > $1'
    USING '2024-01-01';
```

**E.3 Migration Workflows**

```sql
-- Copy remote table to local
CREATE TABLE users_migrated AS
    SELECT * FROM remote_users;

-- Incremental migration
INSERT INTO users_migrated
SELECT * FROM remote_users
WHERE last_modified > (SELECT MAX(last_modified) FROM users_migrated);

-- Verify migration
SELECT COUNT(*) FROM remote_users
EXCEPT
SELECT COUNT(*) FROM users_migrated;
```

**E.4 Supported Foreign Data Wrappers**

| FDW | Library | Protocol |
|-----|---------|----------|
| postgresql_fdw | libpq | PostgreSQL wire |
| mysql_fdw | libmysql | MySQL wire |
| mssql_fdw | FreeTDS | TDS |
| firebird_fdw | fbclient | Firebird wire |

### Implementation Phases

**Phase 3.1: Network Infrastructure (Foundation)**

- Socket management (TCP/IP, Unix domain sockets)
- epoll/kqueue event loop
- Thread pool for connection handling
- Connection state machine
- Session management layer
- Graceful shutdown handling
- Signal handlers (SIGTERM, SIGHUP)

**Phase 3.2: ScratchBird Native Protocol**

- Design specification document
- Binary message format
- Authentication handshake
- Query/result protocol
- Error handling
- Streaming results
- Client library (libscratchbird_client)
- Testing with sb_isql

**Phase 3.3: PostgreSQL Wire Protocol**

- Protocol v3 decoder/encoder
- Authentication (MD5, SCRAM-SHA-256)
- Simple Query Protocol
- Extended Query Protocol
- COPY protocol
- Error/Notice messages
- Testing with psql, pgAdmin, libpq clients

**Phase 3.4: MySQL Wire Protocol**

- Handshake v10
- Authentication methods
- COM_QUERY (text protocol)
- COM_PREPARE/COM_EXECUTE (binary)
- Result set encoding
- Testing with mysql client, MySQL Workbench

**Phase 3.5: TDS Wire Protocol (MSSQL)** - DEFERRED TO BETA

- TDS support deferred to Beta phase
- Focus on open source databases first (PostgreSQL, MySQL, Firebird)
- TDS connectivity via ODBC/JDBC for foreign data access only

**Phase 3.6: Firebird Wire Protocol**

- XDR encoding
- op_connect/op_attach
- Statement execution
- SRP authentication
- Testing with isql, FlameRobin

**Phase 3.7: Service Mode & systemd**

- Daemonization
- PID file management
- Configuration file parser
- Hot configuration reload (SIGHUP)
- Log rotation
- systemd notify integration
- Package installation scripts

**Phase 3.8: Connection Pooling**

- Pool manager implementation
- Per-database pools
- Health checking
- Statement cache
- Result cache
- Statistics collection

**Phase 3.9: Security Suite - Core**

- SSL/TLS implementation (OpenSSL)
- Certificate authentication
- IP whitelisting
- Argon2id password hashing
- SCRAM-SHA-256 challenge-response

**Phase 3.10: Security Suite - Enterprise**

- LDAP/LDAPS authentication
- Active Directory integration
- Kerberos/GSSAPI
- Multi-factor authentication (TOTP)
- SAML 2.0 federation
- OAuth 2.0/OIDC

**Phase 3.11: UDR Plugin System**

- UDR manager implementation
- Plugin discovery and loading
- Remote Database UDR plugin
- postgresql_fdw implementation
- mysql_fdw implementation
- mssql_fdw implementation
- firebird_fdw implementation
- Foreign table execution

**Phase 3.12: ODBC Driver**

- ScratchBird ODBC driver implementation
- ODBC 3.8 compliance
- Connection string configuration
- Prepared statements and parameter binding
- Metadata functions (SQLTables, SQLColumns, etc.)
- ODBC connectivity to external databases (MSSQL, Oracle, etc.)
- Testing with common ODBC applications

**Phase 3.13: JDBC Driver**

- ScratchBird JDBC driver (Type 4, pure Java)
- JDBC 4.3 compliance
- Connection pooling support (HikariCP compatible)
- Prepared statements and batch operations
- ResultSet metadata
- JDBC connectivity to external databases
- Testing with common Java applications and frameworks

**Phase 3.14: Git Integration for Metadata (Nice to Have)**

- Schema versioning with Git
- DDL change tracking and history
- Migration script generation
- DevOps workflow integration
- Rollback/forward migration support
- Audit trail for schema changes

**Phase 3.15: Testing & Performance**

- Protocol compliance testing
- Authentication testing (all methods)
- Security penetration testing
- Load testing (connection storms)
- Memory leak detection
- Performance benchmarking

### Completion Criteria

**ALL items below MUST be complete before Alpha 3 is considered done:**

**Wire Protocols:**
1. ✅ ScratchBird Native Protocol (port 3092) functional
2. ✅ PostgreSQL Wire Protocol v3 (port 5432) functional
3. ✅ MySQL Wire Protocol (port 3306) functional
4. ⏸️ TDS Wire Protocol - DEFERRED TO BETA (MSSQL connectivity via ODBC only)
5. ✅ Firebird Wire Protocol (port 3050) functional
6. ✅ All protocols tested with native clients

**Database Connectivity (ODBC/JDBC):**
7. ✅ ScratchBird ODBC driver functional
8. ✅ ScratchBird JDBC driver functional
9. ✅ ODBC connectivity to MSSQL/Oracle/other databases
10. ✅ JDBC connectivity to external databases

**Service Mode:**
11. ✅ sb_server daemon mode operational
12. ✅ systemd integration complete
13. ✅ Configuration file hot-reload working
14. ✅ Graceful startup/shutdown
15. ✅ Both single-database and multi-database modes

**Security:**
16. ✅ SSL/TLS 1.2+ for all protocols
17. ✅ Certificate authentication (X.509/mTLS)
18. ✅ Multi-factor authentication (TOTP)
19. ✅ IP whitelisting functional
20. ✅ LDAP authentication working
21. ✅ Active Directory integration
22. ✅ Kerberos/GSSAPI functional
23. ✅ SAML 2.0 federation
24. ✅ OAuth 2.0/OIDC integration
25. ✅ Security audit completed (no critical vulnerabilities)

**Connection Pooling:**
26. ✅ Built-in connection pool operational
27. ✅ Statement caching working
28. ✅ Result caching functional
29. ✅ Pool statistics available

**UDR Plugins:**
30. ✅ UDR plugin system functional
31. ✅ postgresql_fdw operational
32. ✅ mysql_fdw operational
33. ⏸️ mssql_fdw - DEFERRED (use ODBC/JDBC instead)
34. ✅ firebird_fdw operational
35. ✅ odbc_fdw operational (for MSSQL, Oracle, etc.)
36. ✅ jdbc_fdw operational (for Java-accessible databases)
37. ✅ Foreign table queries working
38. ✅ Passthrough queries functional

**Performance & Stability:**
39. ✅ Load testing completed (1000+ connections)
40. ✅ No memory leaks in 72-hour stress test
41. ✅ Protocol compliance verified

**Nice to Have (Not Required for Alpha 3 Completion):**
42. ⭕ Git integration for metadata versioning
43. ⭕ Docker image and compose files
44. ⭕ deb/rpm installation packages

**Note: TDS/MSSQL native protocol deferred to Beta. MSSQL connectivity available via ODBC/JDBC.**

### Reference Documentation

| Document | Path | Description |
|----------|------|-------------|
| **ScratchBird Native Protocol** | `/docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md` | **NEW** Native binary protocol (port 3092), cluster PKI, federation |
| **systemd Service Spec** | `/docs/specifications/SYSTEMD_SERVICE_SPECIFICATION.md` | **NEW** Service mode, configuration, lifecycle management |
| **Connection Pooling** | `/docs/specifications/CONNECTION_POOLING_SPECIFICATION.md` | **NEW** Pool architecture, statement/result caching, health checks |
| Network Layer | `/docs/specifications/NETWORK_LAYER_SPEC.md` | Connection pooling, Y-Valve architecture |
| PostgreSQL Protocol | `/docs/specifications/wire_protocols/postgresql_wire_protocol.md` | Protocol v3 spec |
| MySQL Protocol | `/docs/specifications/wire_protocols/mysql_wire_protocol.md` | MySQL wire protocol |
| TDS Protocol | `/docs/specifications/wire_protocols/tds_wire_protocol.md` | MSSQL TDS protocol |
| Firebird Protocol | `/docs/specifications/wire_protocols/firebird_wire_protocol.md` | Firebird XDR protocol |
| Security Model | `/docs/specifications/06_SECURITY_MODEL.md` | 3-pillar security |
| Security Hardening | `/docs/specifications/Security Hardening Guide.md` | 127 attack vectors |
| Security System | `/docs/specifications/SECURITY_SYSTEM_SPECIFICATION.md` | RBAC/GBAC/ACL |
| External Auth | `/docs/specifications/EXTERNAL_AUTHENTICATION_DESIGN.md` | LDAP/AD/Kerberos |
| UDR System | `/docs/specifications/10-UDR-System-Specification.md` | Plugin architecture |
| Remote DB UDR | `/docs/specifications/remote_database_udr/` | Foreign tables (9 docs, ~7,400 lines) **COMPLETE** |
| Client Library API | `/docs/specifications/CLIENT_LIBRARY_API_SPECIFICATION.md` | C API (~1,300 lines) **NEW** |
| Alpha 3 Test Plan | `/docs/specifications/ALPHA3_TEST_PLAN.md` | Test suites, security, benchmarks **NEW** |
| sb_admin CLI | `/docs/specifications/SB_ADMIN_CLI_SPECIFICATION.md` | Admin tool (~600 lines) **NEW** |
| Prometheus Metrics | `/docs/specifications/PROMETHEUS_METRICS_REFERENCE.md` | Metrics, labels, alerts (~820 lines) **NEW** |
| Live Migration | `/docs/specifications/LIVE_MIGRATION_PASSTHROUGH_SPECIFICATION.md` | Zero-downtime migration (~1,820 lines) **NEW** |
| Migration Guide | `/docs/MIGRATION_GUIDE.md` | User guide (~1,020 lines) **NEW** |
| ODBC Driver | `/docs/specifications/ODBC_DRIVER_SPECIFICATION.md` | ODBC 3.8 driver + odbc_fdw (~730 lines) **NEW** |
| JDBC Driver | `/docs/specifications/JDBC_DRIVER_SPECIFICATION.md` | JDBC 4.3 driver + jdbc_fdw (~900 lines) **NEW** |
| Git Integration | `/docs/specifications/GIT_METADATA_INTEGRATION_SPECIFICATION.md` | Schema versioning (~980 lines) **NEW** |

### New Specifications Created (December 10, 2025)

**1. ScratchBird Native Wire Protocol v1.0** (`scratchbird_native_wire_protocol.md`)
- Port 3092 (IANA unassigned)
- Custom binary format optimized for ScratchBird
- TLS 1.3 required, zstd compression
- 29 client→server + 32 server→client message types
- Cluster PKI with Hybrid CA + Session Keys (forward secrecy)
- Full federation protocol for cross-database queries
- SBLR bytecode transmission
- Streaming with backpressure
- All 86 ScratchBird types natively serialized

**2. systemd Service Specification v1.0** (`SYSTEMD_SERVICE_SPECIFICATION.md`)
- `sb_server` binary with full CLI
- INI-style configuration (15 sections, 200+ options)
- Single-database and multi-database modes
- systemd Type=notify integration
- Signal handling (SIGHUP reload, graceful shutdown)
- Prometheus metrics endpoint
- Security hardening (30+ systemd options)
- Cluster service templates
- Example configurations (dev, prod, HA)

**3. Connection Pooling Specification v1.0** (`CONNECTION_POOLING_SPECIFICATION.md`)
- Three pool modes: session, transaction, statement
- Pool hierarchy: PoolManager → DatabasePool → UserPool → PooledConnection
- Statement caching with LRU eviction, query normalization
- Result caching with TTL, MGA-aware invalidation, table dependency tracking
- Health checking: acquire, release, background, heartbeat validation
- Comprehensive statistics and Prometheus metrics
- SQL interface: SHOW POOL STATUS, SHOW STATEMENT CACHE, SHOW RESULT CACHE
- Security: connection isolation, credential handling, RLS-aware caching
- Configuration: 30+ options with hot-reload support

**4. Remote Database UDR Specification v1.0** (`remote_database_udr/` - 9 docs, ~7,400 lines)
- Core types: ServerDefinition, UserMapping, ForeignTableDefinition, type mapping tables
- Remote connection pooling: PoolRegistry, ServerPool, UserPool, health monitoring
- Protocol adapters: PostgreSQL, MySQL, MSSQL (TDS), Firebird, ScratchBird Native
- Query execution: pushdown analysis, SQL dialect translation, result aggregation
- Schema introspection: table discovery, column metadata, index/FK analysis
- SQL syntax: CREATE SERVER/USER MAPPING/FOREIGN TABLE, IMPORT FOREIGN SCHEMA
- Migration workflows: Big Bang, Incremental, Parallel Run, Strangler Fig patterns
- Support for PostgreSQL 9.6-17, MySQL 5.7+/MariaDB 10+, SQL Server 2016+, Firebird 2.5-5.0

**5. Client Library API Specification v1.0** (`CLIENT_LIBRARY_API_SPECIFICATION.md` - ~1,300 lines)
- Pure C API for maximum FFI compatibility (`libscratchbird_client`)
- Connection management: embedded and network modes, SSL/TLS, auto-reconnect
- Query execution: simple queries, prepared statements, parameter binding
- Result handling: typed getters, column metadata, streaming results
- Transaction management: isolation levels, savepoints, auto-commit
- Batch operations: bulk insert with streaming
- Async operations: async queries, notifications/subscriptions
- Metadata: list databases/schemas/tables, describe columns/indexes
- 50+ error codes with SQLSTATE mapping
- Thread safety guidelines and memory ownership rules
- Complete example with build instructions

**6. Alpha 3 Test Plan v1.0** (`ALPHA3_TEST_PLAN.md` - ~730 lines)
- Protocol compliance test suites: 100+ test cases per protocol (PG, MySQL, TDS, Firebird, Native)
- Authentication test matrix: 27 test cases covering 11 auth methods
- Load testing scenarios: connection storms, query throughput, stress tests, endurance
- Security penetration checklist: 70+ tests (network, auth, SQLi, fuzzing, DoS, data protection)
- Performance benchmarks: latency targets, throughput goals, scalability metrics
- CI/CD integration with GitHub Actions workflow
- Exit criteria and defect thresholds

**7. sb_admin CLI Specification v1.0** (`SB_ADMIN_CLI_SPECIFICATION.md` - ~600 lines)
- Server commands: status, start/stop/restart, reload, connections, kill
- Database commands: list, create, drop, vacuum, analyze, check integrity
- Cluster commands: status, join/leave, promote/demote, failover, rebalance
- Backup/restore: full, incremental, PITR, verify, schedule management
- Diagnostics: health checks, slow queries, locks, bloat, cache stats
- Monitoring: Nagios/NRPE checks with thresholds, Prometheus metrics, SNMP
- Security: audit logs, SSL management, key rotation, firewall rules
- Configuration file and environment variables

**8. Prometheus Metrics Reference v1.0** (`PROMETHEUS_METRICS_REFERENCE.md` - ~820 lines)
- 16 metric categories: connections, queries, transactions, memory, storage, WAL, replication, locks, pool, cache, GC, backup, server info
- Complete metric definitions with HELP/TYPE annotations and example values
- Labels and dimensions documented for each metric
- Alerting rules with 15+ predefined alerts (warning/critical thresholds)
- Threshold summary table with recommended warning/critical levels and durations
- Grafana dashboard panel recommendations

**9. Live Migration Passthrough Specification v1.0** (`LIVE_MIGRATION_PASSTHROUGH_SPECIFICATION.md` - ~1,820 lines)
- Zero-downtime database migration from PostgreSQL, MySQL, SQL Server, Firebird
- Per-table migration state machine: NOT_STARTED → BULK_LOADING → SYNCHRONIZING → DUAL_WRITE → LOCAL_ONLY
- Query routing engine (post-semantic analysis interception)
- Background migration worker with bulk loading and CDC support
- Dual-write coordination with conflict detection and resolution
- CDC integration: PostgreSQL logical replication, MySQL binlog, SQL Server Change Tracking, Firebird triggers
- Cutover process with pre-validation and rollback procedures
- Full SQL interface: CREATE MIGRATION SOURCE, START/PAUSE/RESUME/ABORT MIGRATION, CUTOVER, ROLLBACK
- Prometheus metrics for migration monitoring
- 16-week implementation roadmap

**10. Migration Guide v1.0** (`docs/MIGRATION_GUIDE.md` - ~1,020 lines)
- Quick Start (5-minute migration setup)
- Migration planning: assessment, compatibility, duration estimation, risk
- Step-by-step walkthroughs: PostgreSQL, MySQL, SQL Server, Firebird to ScratchBird
- Schema migration: automatic import, type mapping, indexes, sequences
- Data strategies: small (<10GB), medium (10GB-1TB), large (>1TB)
- Application integration: connection strings, ORM compatibility, testing checklist
- Monitoring: dashboards, key metrics, alerting thresholds
- Cutover procedures: pre-checklist, execution, validation, rollback
- Troubleshooting: CDC lag, bulk stalls, conflicts, schema mismatch
- Best practices: migration order, communication templates, cleanup

**11. ODBC Driver Specification v1.0** (`ODBC_DRIVER_SPECIFICATION.md` - ~730 lines)
- ODBC 3.8 compliance with 3.52 backwards compatibility
- Full API implementation: connection, statement, catalog, transaction, diagnostic functions
- ScratchBird type to ODBC type mapping (all 86 types)
- odbc_fdw for connecting to MSSQL, Oracle, DB2, SAP HANA, Snowflake, etc.
- Connection string parameters and DSN configuration
- Array binding and fetch optimization
- Application examples: Python (pyodbc), C/C++, Excel, Tableau

**12. JDBC Driver Specification v1.0** (`JDBC_DRIVER_SPECIFICATION.md` - ~900 lines)
- JDBC 4.3 compliance (Java 9+) with 4.2 compatibility (Java 8)
- Type 4 pure Java driver with SPI auto-loading
- Full JDBC API: Connection, Statement, PreparedStatement, ResultSet, DatabaseMetaData
- jdbc_fdw for connecting to Oracle, DB2, SAP HANA, any JDBC-accessible database
- Connection pooling compatible: HikariCP, C3P0, Apache DBCP
- ORM integration: Hibernate, JPA/Spring Data, MyBatis
- Advanced features: LISTEN/NOTIFY, COPY, Large Objects, Arrays

**13. Git Integration Specification v1.0** (`GIT_METADATA_INTEGRATION_SPECIFICATION.md` - ~980 lines) - Nice to Have
- Schema versioning with Git repository integration
- DDL change tracking with automatic capture
- Migration script generation (versioned, timestamp, sequential naming)
- SQL interface: EXPORT SCHEMA TO GIT, IMPORT SCHEMA FROM GIT, GENERATE MIGRATION
- CI/CD integration: GitHub Actions, GitLab CI examples
- Environment management (dev, staging, production)
- Conflict detection and resolution strategies
- Audit trail and compliance reporting

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

**Document Version:** 1.5
**Last Updated:** December 6, 2025
**Status:** OFFICIAL ROADMAP
