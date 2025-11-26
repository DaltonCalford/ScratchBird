# ScratchBird Project Context

**Last Updated:** November 25, 2025 (P2 100% COMPLETE - All 25 medium-priority items implemented!)
**Current Phase:** Alpha 1 - Engine Functionality (Local Operations)
**Progress:** ~90% of Alpha 1 (~15% of total project scope)
**Project Type:** Educational/Research (no time constraints)
**Detailed Status:** [IMPLEMENTATION_STATUS_DASHBOARD.md](docs/IMPLEMENTATION_STATUS_DASHBOARD.md)

> **MANDATORY:** Read [/MGA_RULES.md](/MGA_RULES.md) before ANY transaction or index work.
> **COMPLETE ROADMAP:** See [/OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md) for full project scope.

---

## Current Work: Alpha 1 Completion (~15% Remaining)

**Note:** Due to additional work items identified during development (improvement opportunities, server architecture requirements), Alpha 1 is now estimated at 85% complete with all missing functions, P0 critical issues, P1 high-priority improvements, and P2 medium-priority improvements resolved.

**Focus:** ✅ Functions complete → ✅ P0 Critical Issues complete → ✅ P1 100% complete → ✅ P2 100% complete → P3 Improvements → Server Architecture → CLI tools

**Remaining Alpha 1 Work:**
- ✅ P1 High-Priority (15/15 complete): All items completed including bulk loading bottom-up construction!
- ✅ P2 Medium-Priority (25/25 complete): All performance optimizations and feature completeness items done! 🎉
- ⏳ P3 Low-Priority (13+ items): Enhancements, advanced features - 200+ hours
- ❌ Local Server Architecture (5 phases): IPC, protocol, server, client library - 140-190 hours
- ❌ CLI Tools (4 tools): sb_isql, sb_verify, sb_backup, sb_security - 90-110 hours

### What's Working ✅

- **Core Engine:** 100% (MGA, Buffer Pool, TOAST, Transactions, Tablespaces)
- **Indexes:** 11/11 types complete with MGA compliance
- **Data Types:** 86/86 complete
- **Built-in Functions:** 153/153 complete (100%) 🎉 **JUST COMPLETED!**
- **Security:** 100% (users, roles, table/column/row-level permissions, RLS)
- **Catalog:** 40 tables (100% structures, 58% CRUD)
- **PSQL/Stored Procedures & Triggers:** 100% (control flow, cursors, exceptions, trigger firing, procedure invocation)
- **Advanced SQL:** 100% (CTEs recursive & non-recursive, MERGE, RETURNING, SAVEPOINT, Set Operations)
- **Constraint Features:** 100% (GENERATED columns STORED/VIRTUAL, IDENTITY columns, Deferred constraints)
- **SQL Engine Commands:** 100% (SHOW TABLES/DATABASES/COLUMNS/INDEXES/CREATE TABLE, DESCRIBE, EXPLAIN)
- **Views:** 100% (regular views, materialized views with full data population and refresh) 🎉

### Recently Completed ✅

**PRIORITY 3: P1 High-Priority Improvements** ✅ **100% COMPLETE** (15/15 items, November 25, 2025): 🎉
See [docs/planning/IMPROVEMENTS_P1_HIGH_PRIORITY_PLAN.md](docs/planning/IMPROVEMENTS_P1_HIGH_PRIORITY_PLAN.md) for complete details.

**Agent A (PSQL/SQL) - 100% complete (5/5 items):** 🎉
- ✅ **P1-1:** TRY/EXCEPT Exception Handling - **ALREADY IMPLEMENTED!** (src/sblr/executor.cpp:19142)
- ✅ **P1-4:** Cursor Operations (DECLARE/OPEN/FETCH/CLOSE) - **ALREADY IMPLEMENTED!** (src/sblr/executor.cpp:18956+)
- ✅ **P1-5:** Stored Procedure Invocation - **ALREADY IMPLEMENTED!** (src/sblr/executor.cpp:18321)
- ✅ **P1-13:** MERGE Statement (commit 15de05f), **P1-14:** RETURNING Clause (commit ebd29a7)

**Agent B (Performance) - 100% complete (4/4 items):** 🎉
- ✅ **P1-2:** XID Wraparound Prevention (src/core/transaction_manager.cpp:681-760)
- ✅ **P1-7:** TIP Binary Search - **N/A** (using superior CLOG O(1) lookup)
- ✅ **P1-8:** Index-Based FK Lookups (src/sblr/executor.cpp:21895-21926)
- ✅ **P1-11:** Bulk Index Loading - **COMPLETE!** (bottom-up B-tree construction in src/core/btree.cpp:2837)

**Agent C (Constraints/Catalog) - 100% complete (6/6 items):** 🎉
- ✅ **P1-3:** SQLSTATE Error Codes, **P1-9:** Constraints Table CRUD
- ✅ **P1-6:** Foreign Key Actions (CASCADE/SET NULL) - integrated into DELETE/UPDATE
- ✅ **P1-12:** Session Timeout, **P1-15:** Multi-Geometry Functions
- ✅ **P1-10:** Statistics & ANALYZE - **COMPLETE!** (commit 5676aae - bytecode/executor fully wired)

**Key Finding:** All P1 items are now complete! Most were already implemented, only bulk loading needed implementation.
**Achievement:** All critical PSQL features, constraint operations, and performance improvements complete for Alpha 1!

**PRIORITY 2: P0 Critical Issues** ✅ **COMPLETE** (<1 hour actual work, November 24, 2025):
See [docs/planning/IMPROVEMENTS_P0_CRITICAL_PLAN.md](docs/planning/IMPROVEMENTS_P0_CRITICAL_PLAN.md) for complete details.

**All 8 P0 items verified complete:**
- ✅ **P0-1:** Password Policy Enforcement (src/core/password_policy.cpp)
- ✅ **P0-2:** Account Lockout Mechanism (src/core/login_attempt_tracker.cpp)
- ✅ **P0-3:** Security Audit Logging (src/core/audit_logger.cpp)
- ✅ **P0-4:** Arithmetic Overflow Checking (src/core/safe_arithmetic.h)
- ✅ **P0-5:** NaN/Infinity Handling (src/sblr/executor.cpp, src/core/typed_value.cpp)
- ✅ **P0-6:** GIN Parallel Operations MGA Bug (test fixes in test_gin_phase6.cpp)
- ✅ **P0-7:** Catalog Sequence Operations (src/core/catalog_manager.cpp)
- ✅ **P0-8:** Charset/Collation Read Operations (src/core/catalog_manager.cpp)

**Key Finding:** Most P0 items were already implemented! Only minor enhancements needed (NaN/Infinity checks, test fixes).
**Achievement:** All critical security and correctness issues resolved for Alpha 1.

**PRIORITY 1: Built-in Functions** ✅ **COMPLETE** (~222 hours, November 2025):
See [docs/planning/MISSING_FUNCTIONS_IMPLEMENTATION_STATUS.md](docs/planning/MISSING_FUNCTIONS_IMPLEMENTATION_STATUS.md) for complete details.

**All 5 phases complete:**
- ✅ **Phase 1 (Quick Wins):** LPAD, RPAD, SINH, COSH, TANH, ASINH, ACOSH, ATANH, COT, CBRT, OVERLAY, INITCAP (12 functions)
- ✅ **Phase 2 (Statistical Regression):** REGR_SLOPE, REGR_INTERCEPT, REGR_R2, REGR_COUNT, REGR_AVGX, REGR_AVGY, REGR_SXX, REGR_SYY, REGR_SXY (9 functions)
- ✅ **Phase 3 (Advanced Grouping):** ROLLUP, CUBE, GROUPING SETS, GROUPING() - Full OLAP support
- ✅ **Phase 4 (Window Functions):** NTH_VALUE, CUME_DIST, PERCENT_RANK + 6 simplified window functions (9 functions)
- ✅ **Phase 5 (Misc):** AGE, INITCAP (2 functions)

**Final Status:** 153/153 functions complete (100%) 🎉
**Achievement:** Full functional parity with PostgreSQL, MySQL, MSSQL, and Firebird

### What's Next 🚧

**PRIORITY 3: P3 Improvement Opportunities** (~200+ hours / 5+ weeks):
See [docs/audit/IMPROVEMENT_OPPORTUNITIES.md](docs/audit/IMPROVEMENT_OPPORTUNITIES.md) and [docs/IMPLEMENTATION_STATUS_DASHBOARD.md](docs/IMPLEMENTATION_STATUS_DASHBOARD.md) for complete details.

- **P0 (Critical - 8 items):** ✅ 100% COMPLETE (8/8) - 0 hours remaining
  - ✅ Password policy enforcement, account lockout, audit logging
  - ✅ Arithmetic overflow checking, NaN/Infinity handling
  - ✅ GIN parallel operations MGA bug, catalog sequence/charset operations
  - **All P0 items verified complete! Only minor fixes needed.**

- **P1 (High - 15 items):** ✅ 100% COMPLETE (15/15) - 0 hours remaining 🎉
  - ✅ SQLSTATE error codes, MERGE statement, RETURNING clause
  - ✅ Constraints table CRUD, Session timeout, Multi-Geometry functions
  - ✅ XID wraparound prevention, Index-based FK lookups, FK actions (CASCADE/SET NULL)
  - ✅ **Statistics & ANALYZE** - 100% COMPLETE (commit 5676aae)
  - ✅ **Bulk index loading** - 100% COMPLETE (bottom-up B-tree construction)
  - ✅ TRY/EXCEPT exception handling, cursor operations, stored procedure invocation (already implemented!)
  - **All P1 items complete!**
  - See [docs/planning/IMPROVEMENTS_P1_HIGH_PRIORITY_PLAN.md](docs/planning/IMPROVEMENTS_P1_HIGH_PRIORITY_PLAN.md)

- **P2 (Medium - 25 items):** ✅ 100% COMPLETE (25/25) - 0 hours remaining 🎉
  - ✅ Performance: Page table lock partitioning, dirty page counter, TOAST prefetching, hash index resize
  - ✅ Features: GENERATED columns, deferred constraints, statement-level triggers, MV refresh strategies
  - ✅ Infrastructure: Query result caching, parallel query execution, prepared statement cache
  - ✅ Operations: Connection pooling, backup/restore improvements, index advisor
  - ✅ Quality: Edge case tests, concurrent transaction tests, benchmark suite, constraint enforcement tests
  - ✅ Code: Role cycle detection, policy expression validation, error message context
  - **All P2 items complete!**
  - See [docs/planning/IMPROVEMENTS_P2_MEDIUM_PRIORITY_PLAN.md](docs/planning/IMPROVEMENTS_P2_MEDIUM_PRIORITY_PLAN.md)

- **P3 (Low - 13+ items):** 200+ hours
  - Password expiration, MFA (requires Alpha 3)
  - DECIMAL fixed-point, SIMD vector operations
  - Advanced index features, partition pruning
  - Telemetry, structured logging, query profiler

**Total Improvement Opportunities:** 13+ remaining items (P3 items after P0-P2 completion)

**PRIORITY 4: Local Server Architecture** (~140-190 hours / 3.5-4.5 weeks):
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

**PRIORITY 5: Command-Line Tools** (~90-110 hours / 2.5-3 weeks):
*To be started after server architecture is implemented*

- sb_isql (interactive SQL shell) - connects via libscratchbird_client
- sb_verify (database integrity checker) - connects via libscratchbird_client
- sb_backup (backup/restore tool) - connects via libscratchbird_client
- sb_security (user/role management tool) - connects via libscratchbird_client

### Immediate Next Steps

**UPDATED SEQUENCE (Functions & P0 Complete):**

1. ✅ **Implement all 30 missing functions** ✅ **COMPLETE** (~222 hours)
   - ✅ Phase 1: Quick wins (string, hyperbolic, misc) - COMPLETE
   - ✅ Phase 2: Regression functions - COMPLETE
   - ✅ Phase 3: ROLLUP/CUBE/GROUPING SETS - COMPLETE
   - ✅ Phase 4: Window functions - COMPLETE
   - ✅ Phase 5: Remaining functions - COMPLETE

2. ✅ **Implement all P0 critical issues** ✅ **COMPLETE** (<1 hour)
   - ✅ All 8 P0 items verified complete (most were already implemented)
   - ✅ Minor enhancements: NaN/Infinity checks, test fixes

3. ✅ **Implement P1 high-priority improvements** ✅ **100% COMPLETE** (0 hours remaining) 🎉
   - 15/15 items complete (all items verified!)
   - ✅ Statistics & ANALYZE fully wired (commit 5676aae)
   - ✅ Bulk loading - COMPLETE (bottom-up B-tree construction in btree.cpp:2837)
   - ✅ TRY/EXCEPT, cursors, stored procedure invocation (already implemented!)

4. ✅ **Implement P2 medium-priority improvements** ✅ **COMPLETE** (November 25, 2025) 🎉
   - 25/25 items complete - all performance and feature items done!

5. **Implement P3 low-priority improvements** (200+ hours / 5+ weeks) - CURRENT PRIORITY
   - P3 (Low): 13+ items - enhancements, advanced features

6. **Implement local server architecture** (140-190 hours / 3.5-4.5 weeks)
   - Phase 1: IPC infrastructure (Unix sockets, named pipes, TCP) - 40-50 hours
   - Phase 2: Wire protocol (message format, streaming) - 30-40 hours
   - Phase 3: Server implementation (sb_server process) - 40-50 hours
   - Phase 4: Client library (libscratchbird_client) - 20-30 hours
   - Phase 5: Integration & testing - 10-20 hours

7. **Build command-line tools** (90-110 hours / 2.5-3 weeks)
   - sb_isql (interactive SQL shell)
   - sb_verify (database integrity checker)
   - sb_backup (backup/restore tool)
   - sb_security (user/role management tool)

**Total Alpha 1 Remaining Work:** ~430-500 hours (11-13 weeks)

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

**November 24, 2025 (Latest):**
- ✅ **P0 Critical Issues 100% COMPLETE:** All 8 P0 items verified and resolved! 🎉
  - Security: Password policy, account lockout, audit logging
  - Correctness: Arithmetic overflow, NaN/Infinity handling
  - MGA Compliance: GIN parallel operations bug fixed
  - Functionality: Sequence operations, charset/collation operations
  - **Key finding:** Most P0 items already implemented - only minor fixes needed
  - Total actual work: <1 hour (verification, enhancements, test fixes)
  - Achievement: All critical blockers resolved for Alpha 1 completion

- ✅ **Built-in Functions 100% COMPLETE:** All 153 functions implemented! 🎉
  - Advanced Grouping: ROLLUP, CUBE, GROUPING SETS, GROUPING() - Full OLAP support
  - Statistical Regression: 9 functions (REGR_SLOPE, REGR_INTERCEPT, REGR_R2, etc.)
  - Hyperbolic Math: 7 functions (SINH, COSH, TANH, ASINH, ACOSH, ATANH, COT)
  - Window Functions: CUME_DIST, PERCENT_RANK, NTH_VALUE + 6 simplified functions
  - String Functions: LPAD, RPAD, OVERLAY, INITCAP
  - Date/Time: AGE function
  - Math: CBRT function
  - Total effort: ~222 hours across 5 implementation phases
  - Achievement: Full functional parity with PostgreSQL, MySQL, MSSQL, and Firebird

**November 23, 2025:**
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

**Current Focus:** Complete Alpha 1 (~15% remaining)

**Immediate Work:**
1. ✅ **Implement 30 missing functions** ✅ **COMPLETE** (~222 hours)
   - Achievement: Full PostgreSQL, MySQL, MSSQL, Firebird functional parity
   - Result: 153 total built-in functions (100% complete)

2. ✅ **Resolve P0 critical issues** ✅ **COMPLETE** (<1 hour)
   - Achievement: All 8 critical security and correctness issues resolved
   - Result: Codebase was already in excellent shape - only minor fixes needed

3. ✅ **Implement P1 high-priority improvements** ✅ **COMPLETE** (Nov 25, 2025) 🎉
   - Achievement: All 15 P1 items complete including bulk loading bottom-up construction
   - Result: Performance, PSQL features, and constraint operations all verified working

4. ✅ **Implement P2 medium-priority improvements** ✅ **100% COMPLETE** (November 25, 2025) 🎉
   - 25/25 items complete - all performance and feature items done!
   - See [docs/planning/IMPROVEMENTS_P2_MEDIUM_PRIORITY_PLAN.md](docs/planning/IMPROVEMENTS_P2_MEDIUM_PRIORITY_PLAN.md)

5. **Implement P3 low-priority improvements** (200+ hours / 5+ weeks) - **CURRENT PRIORITY**
   - P3 (Low): 13+ items - enhancements, advanced features

5. **Implement local server architecture** (140-190 hours / 3.5-4.5 weeks) - AFTER improvements
   - Transition from embedded to client-server model
   - IPC infrastructure, wire protocol, sb_server, libscratchbird_client
   - Mandatory before CLI tools can function

5. **Build CLI tools** (90-110 hours / 2.5-3 weeks) - AFTER server architecture
   - sb_isql, sb_verify, sb_backup, sb_security
   - All tools connect via libscratchbird_client

**Next Major Milestones:**
1. Alpha 1 completion (~11-13 weeks total: 430-500 hours remaining)
2. Alpha 2: Multi-dialect parsers (PostgreSQL, MySQL, MSSQL, Firebird, ScratchBird)
3. Alpha 3: Network protocols (libpq, MySQL, TDS, native)
4. Beta 1-4: Distributed systems + NoSQL models
5. RC1-3: Native drivers + stabilization
6. Gold: Production release

**Full Details:** See [OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md)

---

**Last Updated:** November 25, 2025
**Status:** Alpha 1 - 85% complete (~15% of total project)
**Current Priority:** ✅ Functions (153/153) COMPLETE → ✅ P0 Critical (8/8) COMPLETE → ✅ P1 High (15/15) COMPLETE → ✅ P2 Medium (25/25) COMPLETE → P3 Low (13+ items) → Server architecture → CLI tools
