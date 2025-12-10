# ScratchBird Project Context

**Last Updated:** December 10, 2025
**Current Phase:** Alpha Phase 2 - Parser Separation ✅ **COMPLETE**
**Progress:** 100% of Alpha 2 complete, ready for Alpha 3
**Test Suite:** 1255/1255 = 100% pass rate
**Project Type:** Educational/Research (no time constraints)
**Detailed Status:** [IMPLEMENTATION_STATUS_DASHBOARD.md](docs/IMPLEMENTATION_STATUS_DASHBOARD.md)

> **MANDATORY:** Read [/MGA_RULES.md](/MGA_RULES.md) before ANY transaction or index work.
> **COMPLETE ROADMAP:** See [/OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md) for full project scope.

---

## Alpha 2 Status: ✅ **COMPLETE** (December 10, 2025)

### Parser Separation - All Goals Achieved

| Component | Status | Tests |
|-----------|--------|-------|
| **Parser V2** | ✅ Complete | 171 tests (DDL, DML, Session, State) |
| **Firebird Parser** | ✅ Complete | 52 tests |
| **MySQL Parser** | ✅ Complete | 30 tests |
| **PostgreSQL Parser** | ✅ Complete | 52 tests |
| **Total Parser Tests** | ✅ **293 tests** | 100% passing |

### Key Accomplishments

1. **Parser v2.0 Architecture**
   - Context-sensitive "Smart Parser, Dumb Lexer" design
   - ~35 Gatekeeper keywords globally reserved
   - Contextual keyword recognition (TABLE, INDEX, VIEW, etc.)
   - Hierarchical schema path navigation (`.name`, `..name`, qualified paths)
   - UUID-based object resolution

2. **Multi-Dialect SQL Support**
   - **Firebird SQL:** Full dialect 1/2/3 support, EXECUTE BLOCK, generators/sequences
   - **MySQL 8.0:** Backtick identifiers, AUTO_INCREMENT, ON DUPLICATE KEY
   - **PostgreSQL 16:** Dollar-quoting, `::` casts, RETURNING, ON CONFLICT

3. **Query Compilers**
   - `FirebirdQueryCompiler` - Firebird SQL → SBLR bytecode
   - `PostgreSQLQueryCompiler` - PostgreSQL SQL → SBLR bytecode
   - Shared semantic analyzer and bytecode generator

### Key Documents
- **Implementation Plan:** [/docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md](/docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md)
- **Grammar Specification:** [/docs/specifications/ScratchBird Master Grammar Specification v2.0.md](/docs/specifications/ScratchBird%20Master%20Grammar%20Specification%20v2.0.md)
- **Firebird Parser Spec:** [/docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md](/docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md)
- **PostgreSQL Parser Spec:** [/docs/specifications/POSTGRESQL_PARSER_SPECIFICATION.md](/docs/specifications/POSTGRESQL_PARSER_SPECIFICATION.md)
- **MySQL Parser Spec:** [/docs/specifications/MYSQL_PARSER_SPECIFICATION.md](/docs/specifications/MYSQL_PARSER_SPECIFICATION.md)

---

## Next Phase: Alpha 3 - Network Listeners 🚀

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

## Summary

**Current Focus:** Alpha 2 ✅ **COMPLETE** - Ready for Alpha 3

**All Alpha 2 Work Complete!**
- Parser v2.0 with context-sensitive architecture
- Firebird, MySQL, and PostgreSQL dialect parsers
- 293 parser tests (100% passing)
- 1255 total tests (100% passing)

**Next Major Milestones:**

1. ✅ Alpha 1: Core engine - **COMPLETE**
2. ✅ Alpha 2: Multi-dialect parsers - **COMPLETE** (December 10, 2025)
3. 🔜 Alpha 3: Network protocols (libpq, MySQL, TDS, native)
4. Beta 1-4: Distributed systems + NoSQL models
5. RC1-3: Native drivers + stabilization
6. Gold: Production release

**Full Details:** See [OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md)
