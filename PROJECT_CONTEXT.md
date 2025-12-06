# ScratchBird Project Context

**Last Updated:** December 6, 2025
**Current Phase:** Alpha Phase 2 - Parser v2.0 Implementation
**Progress:** 100% of Alpha 1 complete, Alpha 2 Phase 2.2 in progress
**Test Suite:** 1123/1123 = 100% pass rate
**Project Type:** Educational/Research (no time constraints)
**Detailed Status:** [IMPLEMENTATION_STATUS_DASHBOARD.md](docs/IMPLEMENTATION_STATUS_DASHBOARD.md)

> **MANDATORY:** Read [/MGA_RULES.md](/MGA_RULES.md) before ANY transaction or index work.
> **COMPLETE ROADMAP:** See [/OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md) for full project scope.

---

## Current Work: Parser v2.0 Implementation 🚧

**Alpha 2 Phase 2.2:** ScratchBird Parser Extraction with "Smart Parser, Dumb Lexer" architecture.

### Parser v2.0 Design Goals
- **< 50 Reserved Keywords:** Only ~35 "Gatekeeper" words globally reserved
- **Context-Sensitive Parsing:** Keywords like TABLE, INDEX, VIEW recognized contextually
- **Hierarchical Schema Navigation:** Filesystem-like schema paths (`.name`, `..name`, `schema.table`)
- **UUID-Based Object Resolution:** Single name-to-UUID lookup; execution uses UUIDs only

### Key Documents
- **Implementation Plan:** [/docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md](/docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md)
- **Grammar Specification:** [/docs/specifications/ScratchBird Master Grammar Specification v2.0.md](/docs/specifications/ScratchBird%20Master%20Grammar%20Specification%20v2.0.md)
- **Current Parser Audit:** [/docs/planning/current_parser/](/docs/planning/current_parser/) (13 documents)

### Immediate Work
1. Implement ParserState class (mode stack: DDL, DML, SESSION, EXPRESSION, PSQL)
2. Create Gatekeeper keyword set (~35 words)
3. Build contextual keyword helper `expectContextual(string)`
4. Implement schema path parsing (`.`, `..`, qualified names)
5. Refactor lexer to emit IDENTIFIER for most current keywords

---

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

**Current Focus:** Alpha 1 ✅ **COMPLETE** - Ready for Alpha 2

**All Alpha 1 Work Complete!**

**Next Major Milestones:**

1. Alpha 1 completion (~0.5 weeks: 10-20 hours remaining)
2. Alpha 2: Multi-dialect parsers (PostgreSQL, MySQL, MSSQL, Firebird, ScratchBird)
3. Alpha 3: Network protocols (libpq, MySQL, TDS, native)
4. Beta 1-4: Distributed systems + NoSQL models
5. RC1-3: Native drivers + stabilization
6. Gold: Production release

**Full Details:** See [OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md)
