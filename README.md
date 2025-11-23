# ScratchBird Universal Database Engine

**A universal multi-model database platform** featuring Firebird MGA (Multi-Generational Architecture), capable of replacing 9+ specialized databases in a single deployment with full protocol compatibility and unified ACID transactions.

## Vision: The Universal Data Platform

ScratchBird aims to be a **universal database engine** that can emulate PostgreSQL, MySQL, MSSQL, FirebirdSQL, Neo4j, MongoDB, Redis, Cassandra, Elasticsearch, and more—all in one platform with native client compatibility and unified transaction semantics.

**See [OFFICIAL_ROADMAP.md](OFFICIAL_ROADMAP.md) for complete project scope and development phases.**

## Current Status

**Phase:** Alpha 1 - Engine Functionality (Local Operations)
**Progress:** ~99% of Alpha 1 complete (~11% of total project)
**Started:** June 2025 (5 months of evening/weekend development)
**Project Type:** Educational/Research (no time constraints)
**Last Updated:** November 23, 2025

### What's Working ✅

#### Core Engine (100%)
- **MGA Architecture** - Firebird-style Multi-Generational Architecture with TIP-based visibility
- **Buffer Pool & Pages** - LRU caching, heap pages with back-versioning
- **TOAST** - Large object storage with MGA compliance
- **Transactions** - 4 isolation levels, deadlock detection, O(1) state lookups
- **Tablespaces** - Multi-file support with GPID addressing

#### Indexes (11/11 = 100%) 🎉
**Production-ready with MGA compliance:**
- B-Tree, Hash, R-Tree, GIN, Bitmap
- GiST, HNSW (vector), SP-GiST, BRIN
- LSM-Tree, Columnstore

#### Data Types (86/86 = 100%) 🎉
- Numeric, String, Temporal, Binary, Spatial
- JSON/JSONB, XML, UUID, ARRAY, RANGE, VECTOR
- Network types (INET, CIDR, MACADDR)
- Text search types (TSVECTOR, TSQUERY)

#### Built-in Functions (123/123 = 100%) 🎉
- String (11), Aggregate (6), Window (8)
- JSON (13), Array (12), Date/Time (6)
- Mathematical (29), Bit Manipulation (14)
- Cryptographic (4), Statistical (7), XML (9)
- Spatial (40+), Regex (4), Conditional (3)

#### Security System (100%) 🎉
- User/role/group management with transitive membership
- Table-level, column-level, and row-level permissions
- Row-Level Security (RLS) with policy-based filtering
- SQL SECURITY DEFINER/INVOKER
- Password hashing (BCrypt)
- Permission cache with LRU eviction

#### Catalog System (40 tables)
- 18-level schema hierarchy
- UUIDv7 identifiers (RFC 9562)
- 32 object types
- Full CRUD for security tables (8/8)
- Core tables (10/10 structures defined)

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

**Current Work (Alpha 1 - ~1% remaining):**
- ❌ Command-line tools (sb_isql, sb_verify, sb_backup, sb_security)
- ⧗ Views (80% - materialized view physical implementation pending)

**After Alpha 1 (~89% of project remaining):**

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

```bash
# Build
mkdir build && cd build
cmake .. && make -j$(nproc)

# Test
ctest --output-on-failure
```

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

## The End Goal

A universal database platform that can:
- **Replace** 9+ specialized databases in a single deployment
- **Emulate** PostgreSQL, MySQL, MSSQL, FirebirdSQL, Neo4j, MongoDB, Redis, Cassandra, Elasticsearch
- **Integrate** with existing databases in heterogeneous clusters
- **Scale** from embedded use to massive distributed systems
- **Support** legacy applications with full wire protocol compatibility
- **Enable** modern applications with NoSQL, streaming, and AI capabilities
- **Unify** all data models under a single ACID transaction engine
- **Provide** a stable educational platform for database research

**This is not just a database—it's a database platform demonstrating what's possible when you combine the best ideas from relational, NoSQL, and distributed systems into a cohesive MGA architecture.**

## License

See [LICENSE](LICENSE) file.
