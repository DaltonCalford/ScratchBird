# ScratchBird Project Context

**Last Updated:** November 23, 2025
**Current Phase:** Alpha 1 - Engine Functionality (Local Operations)
**Progress:** ~99% of Alpha 1 (~11% of total project scope)
**Project Type:** Educational/Research (no time constraints)

> **MANDATORY:** Read [/MGA_RULES.md](/MGA_RULES.md) before ANY transaction or index work.
> **COMPLETE ROADMAP:** See [/OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md) for full project scope.

---

## Current Work: Alpha 1 Completion (~5% Remaining)

**Focus:** Complete missing functions for full database compatibility, then build command-line tools

### What's Working ✅

- **Core Engine:** 100% (MGA, Buffer Pool, TOAST, Transactions, Tablespaces)
- **Indexes:** 11/11 types production-ready with MGA compliance
- **Data Types:** 86/86 complete
- **Security:** 100% (users, roles, table/column/row-level permissions, RLS)
- **Catalog:** 40 tables (100% structures, 58% CRUD)
- **PSQL/Stored Procedures & Triggers:** 100% (control flow, cursors, exceptions, trigger firing, procedure invocation)
- **Advanced SQL:** 100% (CTEs recursive & non-recursive, MERGE, RETURNING, SAVEPOINT, Set Operations)
- **Constraint Features:** 100% (GENERATED columns STORED/VIRTUAL, IDENTITY columns, Deferred constraints)
- **SQL Engine Commands:** 100% (SHOW TABLES/DATABASES/COLUMNS/INDEXES/CREATE TABLE, DESCRIBE, EXPLAIN)
- **Views:** 100% (regular views, materialized views with full data population and refresh) 🎉

### What's Missing ❌

**PRIORITY 1: Missing Functions** (~207-312 hours estimated):
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

**PRIORITY 2: Command-Line Tools** (~90-110 hours estimated):
*To be started after all functions are implemented*

- sb_isql (interactive SQL shell)
- sb_verify (database integrity checker)
- sb_backup (backup/restore tool)
- sb_security (user/role management tool)

### Immediate Next Steps

**MANDATORY SEQUENCE:**

1. **Implement all 30 missing functions** (207-312 hours / 5-8 weeks)
   - Phase 1: Quick wins (string, hyperbolic, misc) - 22-39 hours
   - Phase 2: Regression functions - 45-63 hours
   - Phase 3: ROLLUP/CUBE/GROUPING SETS - 56-86 hours
   - Phase 4: Window functions - 24-34 hours
   - Phase 5: Remaining functions - 10-15 hours

2. **Build command-line tools** (90-110 hours / 2.5-3 weeks)
   - sb_isql (interactive SQL shell)
   - sb_verify (database integrity checker)
   - sb_backup (backup/restore tool)
   - sb_security (user/role management tool)

---

## After Alpha 1: The Full Vision

Alpha 1 represents approximately **11% of the total project scope**.

**For complete details, see [OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md).**

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

## The Universal Database Vision

**Goal:** A single database platform that can replace:
- PostgreSQL, MySQL, MSSQL, FirebirdSQL (relational SQL)
- Neo4j (graph database)
- MongoDB (document store)
- Redis (key-value store)
- Cassandra (column-family store)
- Elasticsearch (full-text search)
- InfluxDB (time-series)
- S3 (object storage)
- Kafka (stream processing)

**Key Capability:** Existing clients connect using their native protocols without modification. A PostgreSQL client can connect to ScratchBird and see a PostgreSQL database. A Neo4j client can run Cypher queries. A MongoDB client can execute document operations. All on the same underlying MGA engine with unified ACID transactions.

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
- [/OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md) - **Complete project scope and all phases**
- [/MGA_RULES.md](/MGA_RULES.md) - **Mandatory architecture rules**
- [/docs/IMPLEMENTATION_AUDIT.md](/docs/IMPLEMENTATION_AUDIT.md) - Complete code locations
- [/docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md](/docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md) - Alpha 1 work plan

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

**Current Focus:** Complete Alpha 1 (~5% remaining)

**Immediate Work:**
1. **Implement 30 missing functions** (207-312 hours / 5-8 weeks) - IN PROGRESS
   - Required for full PostgreSQL, MySQL, MSSQL, Firebird compatibility
   - Target: 153 total built-in functions (currently 123)
2. **Build CLI tools** (90-110 hours / 2.5-3 weeks) - AFTER functions complete
   - sb_isql, sb_verify, sb_backup, sb_security

**Next Major Milestones:**
1. Alpha 1 completion (~8-11 weeks total: functions + CLI tools)
2. Alpha 2: Multi-dialect parsers (PostgreSQL, MySQL, MSSQL, Firebird, ScratchBird)
3. Alpha 3: Network protocols (libpq, MySQL, TDS, native)
4. Beta 1-4: Distributed systems + NoSQL models
5. RC1-3: Native drivers + stabilization
6. Gold: Production release

**Full Details:** See [OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md)

---

**Last Updated:** November 23, 2025
**Status:** Alpha 1 - ~95% complete (~11% of total project)
**Priority:** Function implementation (80% complete: 123/153 functions)
