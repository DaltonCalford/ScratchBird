# ScratchBird Project Context

**Last Updated:** November 23, 2025
**Current Phase:** Alpha 1 - Engine Functionality (Local Operations)
**Progress:** ~70% of Alpha 1 (~11% of total project scope)
**Project Type:** Educational/Research (no time constraints)

> **MANDATORY:** Read [/MGA_RULES.md](/MGA_RULES.md) before ANY transaction or index work.
> **COMPLETE ROADMAP:** See [/OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md) for full project scope.

---

## Current Work: Alpha 1 Completion (~30% Remaining)

**Note:** Due to additional work items identified during development (improvement opportunities, server architecture requirements), Alpha 1 is now estimated at 70% complete despite all originally planned features being functional.

**Focus:** Complete missing functions, implement improvement opportunities, build local server architecture, then CLI tools

### What's Working ✅

- **Core Engine:** 100% (MGA, Buffer Pool, TOAST, Transactions, Tablespaces)
- **Indexes:** 11/11 types complete with MGA compliance
- **Data Types:** 86/86 complete
- **Security:** 100% (users, roles, table/column/row-level permissions, RLS)
- **Catalog:** 40 tables (100% structures, 58% CRUD)
- **PSQL/Stored Procedures & Triggers:** 100% (control flow, cursors, exceptions, trigger firing, procedure invocation)
- **Advanced SQL:** 100% (CTEs recursive & non-recursive, MERGE, RETURNING, SAVEPOINT, Set Operations)
- **Constraint Features:** 100% (GENERATED columns STORED/VIRTUAL, IDENTITY columns, Deferred constraints)
- **SQL Engine Commands:** 100% (SHOW TABLES/DATABASES/COLUMNS/INDEXES/CREATE TABLE, DESCRIBE, EXPLAIN)
- **Views:** 100% (regular views, materialized views with full data population and refresh) 🎉

### What's Missing ❌

**PRIORITY 1: Missing Functions** (~207-312 hours / 5-8 weeks):
See [docs/planning/MISSING_FUNCTIONS_IMPLEMENTATION_PLAN.md](docs/planning/MISSING_FUNCTIONS_IMPLEMENTATION_PLAN.md) for complete details.

- **Advanced Grouping (CRITICAL):** ROLLUP, CUBE, GROUPING SETS, GROUPING() - 56-86 hours
- **Statistical Regression (HIGH):** REGR_SLOPE, REGR_INTERCEPT, REGR_R2, etc. (9 functions) - 45-63 hours
- **Hyperbolic Math (HIGH):** SINH, COSH, TANH, ASINH, ACOSH, ATANH, COT (7 functions) - 15-24 hours
- **String Functions (HIGH):** LPAD, RPAD, OVERLAY (3 functions) - 4-8 hours
- **Window Functions (HIGH):** NTH_VALUE, CUME_DIST, PERCENT_RANK (3 functions) - 24-34 hours
- **Date/Time (MEDIUM):** AGE (1 function) - 4-6 hours
- **Misc (LOW):** INITCAP, CBRT (2 functions) - 4-6 hours

**Current Status:** 123/153 functions complete (80%)
**After completion:** Full functional parity with PostgreSQL, MySQL, MSSQL, and Firebird

**PRIORITY 2: Improvement Opportunities** (~430-540 hours / 11-14 weeks):
See [docs/audit/IMPROVEMENT_OPPORTUNITIES.md](docs/audit/IMPROVEMENT_OPPORTUNITIES.md) for complete details.

- **P0 (Critical - 8 items):** 50-70 hours
  - Password policy enforcement, account lockout, security audit logging
  - Arithmetic overflow checking, NaN/Infinity handling
  - GIN parallel operations MGA bug, catalog sequence/charset operations

- **P1 (High - 15 items):** 80-120 hours
  - TRY/EXCEPT exception handling, SQLSTATE error codes
  - Cursor operations, stored procedure invocation
  - Foreign key completion, MERGE/RETURNING statements
  - TIP binary search, index-based FK lookups
  - Statistics table & ANALYZE, bulk loading for indexes

- **P2 (Medium - 25 items):** 100-150 hours
  - Performance optimizations (page table lock partitioning, dirty page counter, TOAST prefetching)
  - Feature completeness (GENERATED columns, deferred constraints, statement-level triggers)
  - Window function frames, statistical aggregates
  - Testing & quality improvements

- **P3 (Low - 20+ items):** 200+ hours
  - Password expiration, MFA (requires Alpha 3)
  - DECIMAL fixed-point, SIMD vector operations
  - Advanced index features, partition pruning
  - Telemetry, structured logging, query profiler

**Total Improvement Opportunities:** 61 items across all priority levels

**PRIORITY 2.5: Local Server Architecture** (~140-190 hours / 3.5-4.5 weeks):
See [docs/planning/LOCAL_SERVER_ARCHITECTURE_PLAN.md](docs/planning/LOCAL_SERVER_ARCHITECTURE_PLAN.md) for complete details.

*Transition from embedded to client-server model - MANDATORY before CLI tools*

- **Phase 1:** IPC Infrastructure (Unix sockets, Named pipes, TCP localhost) - 40-50 hours
- **Phase 2:** Wire Protocol (binary message format, streaming) - 30-40 hours
- **Phase 3:** Server Implementation (sb_server, multi-threading, sessions) - 40-50 hours
- **Phase 4:** Client Library (libscratchbird_client, auto-start) - 20-30 hours
- **Phase 5:** Integration & Testing (security, performance, cross-platform) - 10-20 hours

**Key Features:**
- Database files opened exclusively by server process
- Platform-appropriate IPC (Unix sockets on Linux/macOS, Named pipes on Windows)
- Auto-start server when client connects
- Authentication and session management
- Multi-client support
- Upgrade path to Alpha 3 network protocols

**PRIORITY 3: Command-Line Tools** (~90-110 hours / 2.5-3 weeks):
*To be started after server architecture is implemented*

- sb_isql (interactive SQL shell) - connects via libscratchbird_client
- sb_verify (database integrity checker) - connects via libscratchbird_client
- sb_backup (backup/restore tool) - connects via libscratchbird_client
- sb_security (user/role management tool) - connects via libscratchbird_client

### Immediate Next Steps

**MANDATORY SEQUENCE:**

1. **Implement all 30 missing functions** (207-312 hours / 5-8 weeks)
   - Phase 1: Quick wins (string, hyperbolic, misc) - 22-39 hours
   - Phase 2: Regression functions - 45-63 hours
   - Phase 3: ROLLUP/CUBE/GROUPING SETS - 56-86 hours
   - Phase 4: Window functions - 24-34 hours
   - Phase 5: Remaining functions - 10-15 hours

2. **Implement all improvement opportunities** (430-540 hours / 11-14 weeks)
   - P0 (Critical): Security, correctness, MGA bugs - 50-70 hours
   - P1 (High): Core features, performance - 80-120 hours
   - P2 (Medium): Optimizations, completeness - 100-150 hours
   - P3 (Low): Enhancements, advanced features - 200+ hours

3. **Implement local server architecture** (140-190 hours / 3.5-4.5 weeks)
   - Phase 1: IPC infrastructure (Unix sockets, named pipes, TCP) - 40-50 hours
   - Phase 2: Wire protocol (message format, streaming) - 30-40 hours
   - Phase 3: Server implementation (sb_server process) - 40-50 hours
   - Phase 4: Client library (libscratchbird_client) - 20-30 hours
   - Phase 5: Integration & testing - 10-20 hours

4. **Build command-line tools** (90-110 hours / 2.5-3 weeks)
   - sb_isql (interactive SQL shell)
   - sb_verify (database integrity checker)
   - sb_backup (backup/restore tool)
   - sb_security (user/role management tool)

**Total Alpha 1 Remaining Work:** 867-1,152 hours (22-29 weeks)

---

## After Alpha 1: Remaining Phases

Alpha 1 represents approximately **11% of the total project scope**. See [OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md) for complete details.

### Summary of Remaining Phases (~89% of project)

**Alpha 2: Parser Separation**
- Extract parser into separate library
- Implement 5 SQL dialect parsers: ScratchBird, PostgreSQL, MySQL, MSSQL, FirebirdSQL
- All dialects translate to same SBLR bytecode

**Alpha 3: Network Listeners**
- 4 wire protocols: PostgreSQL, MySQL, TDS/MSSQL, ScratchBird native
- Client authentication, SSL/TLS
- Connection pooling

**Beta 1: Cluster Implementation**
- Distributed architecture with automatic sharding
- Replication and failover
- Distributed transactions (2PC)

**Beta 2: Heterogeneous Clusters**
- Foreign Data Wrappers for PostgreSQL, MySQL, MSSQL, FirebirdSQL
- Cross-database query federation
- XA distributed transactions

**Beta 3: Encryption & Advanced Indexes**
- Field-level and database-level encryption
- Key management server
- Advanced indexes (Bloom Filter, ML indexes, Graph indexes, etc.)

**Beta 4: NoSQL Dialects & Integration Tools** (MAJOR PHASE)
- **9 NoSQL models** with dedicated query dialects:
  1. Graph Database (Cypher, Gremlin, ScratchBird native)
  2. Vector Database (k-NN, ANN queries)
  3. Document Store (MongoDB-compatible)
  4. Key-Value Store (Redis-compatible)
  5. Time-Series Database (InfluxDB-style)
  6. Column-Family Store (Cassandra CQL)
  7. Full-Text Search (Elasticsearch DSL)
  8. Stream Processing (continuous queries)
  9. Object/Blob Store (S3-compatible)
- Integration tools: Kafka, message queues, AI agents, observability

**RC1: Native Drivers**
- 12 language drivers: ODBC, JDBC, C++, C, C#, Rust, Pascal, Python, Go, Node.js, Ruby, PHP
- Beta user testing

**RC2/RC3: Stabilization**
- Bug fixing, performance optimization
- Security audits

**Gold: Production Release**
- Full feature completion
- All quality criteria met

---

## MGA Architecture (Firebird Style)

**CRITICAL:** ScratchBird uses **Firebird MGA**, NOT PostgreSQL MVCC.

### Mandatory Rules
- **TIP-based visibility only** - `isVersionVisible(xmin, current_xid)`
- **In-place updates** - Primary record modified, old data in back versions
- **Stable TIDs** - Indexes never change unless indexed column changes
- **No snapshots** - Zero PostgreSQL MVCC contamination

```cpp
// ✅ CORRECT - Firebird MGA
if (isVersionVisible(tuple->xmin, current_xid)) { ... }

// ❌ WRONG - PostgreSQL MVCC (NEVER USE)
if (isSnapshotVisible(tuple, snapshot)) { ... }
```

**Read [MGA_RULES.md](/MGA_RULES.md) before ANY transaction or index work.**

---

## Development Timeline

**Work Completed:** 5 months (June-November 2025)
- Single developer, evenings/weekends
- AI chatbot assistance (Claude, limited token usage)

**Current Progress:** ~11% of total project scope

**Realistic Projection:**
- Alpha 1 completion: ~1-2 months
- Remaining project (Alpha 2 through Gold): ~3.5-4 years
- **Total estimated timeline:** ~4 years

**Note:** This is an educational/research project with NO fixed deadlines. Each phase completes when ALL defined features are implemented, not based on time estimates.

---

## Critical File Locations

### Documentation
- [/OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md) - Complete project scope and all phases
- [/MGA_RULES.md](/MGA_RULES.md) - Mandatory architecture rules
- [/docs/IMPLEMENTATION_AUDIT.md](/docs/IMPLEMENTATION_AUDIT.md) - Complete code locations

### Specifications
- [/docs/specifications/](/docs/specifications/) - SQL dialect, DDL, NoSQL models, indexes

### Core Implementation
```
src/core/buffer_pool.cpp            - Buffer management
src/core/heap_page.cpp               - Record storage with back-versioning
src/core/toast.cpp                   - Large object storage
src/core/transaction_manager.cpp    - TIP-based transactions
src/core/btree.cpp                   - B-Tree index
src/core/catalog_manager.cpp        - System catalog
src/parser/parser.cpp                - SQL parser
src/sblr/executor.cpp                - SBLR bytecode interpreter
```

---

## Development Guidelines

### For AI Assistants

**MANDATORY READING:**
1. Read `/MGA_RULES.md` at session start
2. Re-read `/MGA_RULES.md` after context compaction
3. Read `/MGA_RULES.md` BEFORE any transaction or index work
4. Refer to `/docs/IMPLEMENTATION_AUDIT.md` for function signatures

**DO:**
- ✅ Use Firebird MGA model (TIP-based visibility)
- ✅ Maintain stable TIDs
- ✅ In-place updates with back versions
- ✅ Follow error handling patterns (Status enum, ErrorContext)
- ✅ Use RAII for all resources

**DON'T:**
- ❌ Use PostgreSQL MVCC patterns
- ❌ Implement Snapshot structures
- ❌ Use forward-versioning
- ❌ Update index TIDs unless indexed column changes
- ❌ Skip reading `/MGA_RULES.md`

**CRITICAL:** Violating MGA rules means the code is architecturally WRONG and must be rewritten.

---

## Recent Accomplishments

**November 23, 2025 (Latest):**
- ✅ **Build Environment Documentation:** Comprehensive BUILD_ENVIRONMENT.md with platform-specific setup
- ✅ **Missing Functions Analysis:** Complete cross-database comparison identifying 30 missing functions
- ✅ **Implementation Plan:** Detailed 5-phase roadmap for achieving full database parity
- ✅ **Views 100% COMPLETE:** Materialized views with full column derivation and data population, REFRESH with query re-execution 🎉
- ✅ **Constraint Features 100% COMPLETE:** GENERATED columns (STORED/VIRTUAL), IDENTITY columns, Deferred constraint checking
- ✅ **SHOW/DESCRIBE Commands 100% COMPLETE:** SHOW TABLES/DATABASES/COLUMNS/INDEXES/CREATE TABLE, DESCRIBE
- ✅ **PSQL/Triggers 100% COMPLETE:** Control flow, cursors, exceptions, trigger firing, procedure invocation
- ✅ **Advanced SQL 100% COMPLETE:** CTEs (recursive & non-recursive), MERGE, RETURNING, SAVEPOINT, Set Operations

**November 2025:**
- ✅ Views implementation (CREATE VIEW, MATERIALIZED VIEW with physical storage, REFRESH)
- ✅ Index system documentation (900+ lines)
- ✅ Columnstore TIP integration
- ✅ 123 core built-in functions complete (XML, Cryptographic, Statistical, Mathematical, Bit Manipulation)
- ✅ Security Phase 3.5 complete (RLS DML enforcement, SQL object permissions)
- ✅ Foreign key Phase C (table-level syntax, composite FKs, disk persistence)
- ✅ Constraint system (CHECK, DEFAULT, UNIQUE, FK enforcement)

---

## Summary

**Current Focus:** Complete Alpha 1 (~30% remaining)

**Immediate Work:**
1. **Implement 30 missing functions** (207-312 hours / 5-8 weeks) - IN PROGRESS
   - Required for full PostgreSQL, MySQL, MSSQL, Firebird compatibility
   - Target: 153 total built-in functions (currently 123)

2. **Implement 61 improvement opportunities** (430-540 hours / 11-14 weeks) - AFTER functions
   - P0 (Critical): 8 items - security, correctness, MGA bugs
   - P1 (High): 15 items - core features, performance
   - P2 (Medium): 25 items - optimizations, completeness
   - P3 (Low): 20+ items - enhancements, advanced features

3. **Implement local server architecture** (140-190 hours / 3.5-4.5 weeks) - AFTER improvements
   - Transition from embedded to client-server model
   - IPC infrastructure, wire protocol, sb_server, libscratchbird_client
   - Mandatory before CLI tools can function

4. **Build CLI tools** (90-110 hours / 2.5-3 weeks) - AFTER server architecture
   - sb_isql, sb_verify, sb_backup, sb_security
   - All tools connect via libscratchbird_client

**Next Major Milestones:**
1. Alpha 1 completion (~22-29 weeks total: 867-1,152 hours remaining)
2. Alpha 2: Multi-dialect parsers (PostgreSQL, MySQL, MSSQL, Firebird, ScratchBird)
3. Alpha 3: Network protocols (libpq, MySQL, TDS, native)
4. Beta 1-4: Distributed systems + NoSQL models
5. RC1-3: Native drivers + stabilization
6. Gold: Production release

**Full Details:** See [OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md)

---

**Last Updated:** November 23, 2025
**Status:** Alpha 1 - 70% complete (~11% of total project)
**Current Priority:** Functions (123/153) → Improvements (61 items) → Server architecture → CLI tools
