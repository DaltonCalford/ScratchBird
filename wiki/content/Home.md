![ScratchBird Logo](../images/logos/TransparentScratchBirdLogoHeader.png)

# Welcome to ScratchBird Wiki

**Version:** Alpha
**Last Updated:** 2026-01-30

> 🎯 **ScratchBird** is a high-performance database that implements **Firebird's Multi-Generational Architecture (MGA)** transaction model with modern features including vector search, advanced indexing, and comprehensive SQL support.

---

## Quick Links

### 📘 Introduction
- [How this all started](How-this-all-started.md) - Why ScratchBird exists and how it evolved
- [What makes ScratchBird different](What-makes-ScratchBird-different.md) - The design choices that set it apart

### 🚀 Getting Started
- [**Getting Started Guide**](Getting-Started.md) - Quick start in < 5 minutes
- [Installation Guides](installation/README.md) - Platform-specific installation
- [Docker Quick Start](installation/Docker.md) - Fastest way to try ScratchBird

### 📚 Documentation
- [Developers Guide](developer-guide/README.md) - Architecture and specs map
- [Command Line Tools](cli-tools/README.md) - CLI reference and commands
- [Language Guides](language-guides/README.md) - Dialect-specific SQL guides
- [Feature Comparison](Feature-Comparison.md) - ScratchBird vs other engines and SQL standard
- [User Guides](user-guides/README.md) - Feature documentation
- [Tutorials](tutorials/README.md) - Step-by-step learning

### 💻 Language Drivers
- [Python](drivers/Python.md) - PEP 249 DB-API, SQLAlchemy, Pandas
- [Node.js/TypeScript](drivers/NodeJS-TypeScript.md) - Promise-based, Prisma
- [Java JDBC](drivers/Java-JDBC.md) - JDBC 4.2+, Hibernate, Spring
- [C#/.NET](drivers/CSharp-DotNet.md) - ADO.NET, Entity Framework Core
- [Go](drivers/Go.md) - database/sql, GORM, sqlx
- [PHP](drivers/PHP.md) - PDO, mysqli, WordPress
- [Pascal/Delphi](drivers/Pascal-Delphi.md) - FireDAC, IBX, Zeos, Lazarus
- [Ruby](drivers/Ruby.md) - Sequel, ActiveRecord
- [Rust](drivers/Rust.md) - Tokio, async services
- [R](drivers/R.md) - DBI, analytics

### 🔄 Migration Guides
- [From Firebird](migration/From-Firebird.md) - **Critical for MGA users**
- [From PostgreSQL](migration/From-PostgreSQL.md)
- [From MySQL](migration/From-MySQL.md)
- [Migration Checklist](migration/Migration-Checklist.md)

---

## What is ScratchBird?

ScratchBird is a **modern relational database** that combines:

### 🔥 Firebird's MGA Transaction Model
- **Multi-Generational Architecture** - True MVCC with record versioning
- **Snapshot Isolation** - Readers never block writers
- **No Rollback Segment** - Clean, elegant transaction design
- **Automatic Garbage Collection** - Reclaim space from old record versions

### ⚡ Modern Performance Features
- **Vector Search** - Native pgvector-compatible vector operations with HNSW and IVF indexing
- **Advanced Indexing** - B-tree, Hash, GiST, GIN, SP-GiST, BRIN, R-tree, Bitmap, LSM-Tree, HNSW, IVF, Columnstore, Full-text, Inverted, Zone Map
- **Bloom Filters** - Attachable to B-tree, Hash, and GIN indexes for accelerated lookups
- **TOAST/LOB Storage** - Efficient large object handling with visibility tracking
- **Zone Maps (BRIN)** - Fast data pruning for analytical queries
- **Columnstore** - Columnar storage for analytical workloads

### 🌐 Broad Compatibility
- **SQL-92/SQL-99** compliance
- **PostgreSQL wire protocol** compatibility
- **Firebird SQL dialect** support via emulated parser
- **MySQL dialect** support via emulated parser
- **Native ScratchBird SQL** with SBLR bytecode compilation

---

## Key Features

### Transaction Management
- **Firebird MGA** - Multi-generational architecture with record versioning
- **ACID Compliance** - Full transaction guarantees
- **Isolation Levels** - READ COMMITTED, REPEATABLE READ, SERIALIZABLE
- **Savepoints** - Partial rollback support
- **Two-Phase Commit** - Distributed transaction support
- **Commit Log (CLOG)** - Transaction state tracking
- **TIP Compaction** - Transaction Inventory Page management
- **Sweep Manager** - Automatic garbage collection of old record versions
- **Long Transaction Monitor** - Detection and management of long-running transactions
- **ProcArray** - Active transaction tracking for snapshot isolation

### Authentication and Security
- **SCRAM-SHA-256** - PostgreSQL-compatible password authentication
- **Kerberos/GSSAPI** - Enterprise single sign-on
- **LDAP** - Directory service authentication
- **OAuth 2.0** - Token-based authentication
- **SAML** - Federated identity
- **MFA** - Multi-factor authentication
- **Certificate** - TLS client certificate authentication
- **Password Policy** - Configurable password strength and expiration rules
- **Login Attempt Tracking** - Brute-force protection
- **Security Quorum** - Multi-party authorization for sensitive operations

### Data Types
- **Numeric** - INT8/16/32/64/128, UINT8/16/32/64/128, FLOAT32, FLOAT64, DECIMAL, MONEY
- **String** - CHAR, VARCHAR, TEXT with UTF-8 support
- **Binary** - BINARY, VARBINARY, BLOB, BYTEA
- **Date/Time** - DATE, TIME, TIMESTAMP (with/without timezone), INTERVAL
- **Boolean** - Native boolean type
- **JSON** - JSON (validated text) and JSONB (binary optimized)
- **XML** - Native XML document type
- **UUID** - RFC 4122 UUID (UUIDv7 generation supported)
- **Vector** - Variable-dimension vector embeddings for AI/ML similarity search
- **Spatial** - POINT, LINESTRING, POLYGON, MULTIPOINT, MULTILINESTRING, MULTIPOLYGON, GEOMETRYCOLLECTION with SRID support
- **Text Search** - TSVECTOR and TSQUERY for full-text search
- **Range** - INT4RANGE, INT8RANGE, NUMRANGE, TSRANGE, TSTZRANGE, DATERANGE
- **Network** - INET, CIDR, MACADDR, MACADDR8
- **Composite** - ARRAY (homogeneous), COMPOSITE (heterogeneous record types)
- **Domain** - Custom types with constraints and validation
- **VARIANT** - Tagged union for polymorphic columns

### Indexing
- **B-tree** - General-purpose ordered indexing with optional Bloom filter
- **Hash** - Equality lookups with optional Bloom filter
- **GiST** - Generalized Search Trees for spatial, range, and custom types
- **GIN** - Generalized Inverted Index for full-text search, arrays, JSONB with optional Bloom filter and compression
- **SP-GiST** - Space-partitioned GiST for quad-trees, radix trees, text operations
- **BRIN** - Block Range Indexes for large sequential datasets
- **R-tree** - Spatial indexing with node-level operations
- **Bitmap** - Bitmap indexes for low-cardinality columns
- **LSM-Tree** - Write-optimized indexing with compaction, block cache, and Bloom filters
- **HNSW** - Hierarchical Navigable Small World graphs for vector similarity search
- **IVF** - Inverted File index for memory-efficient vector search
- **Zone Map** - Min/max metadata per block for fast data skipping
- **Columnstore** - Columnar indexes for analytical queries
- **Full-text** - Dedicated full-text search index with tsvector/tsquery
- **Inverted** - General-purpose inverted indexes
- **Expression Indexes** - Index on computed columns

### Advanced Features
- **CTEs** - Common Table Expressions including RECURSIVE
- **Window Functions** - Analytics and aggregations
- **Views** - Regular and materialized views
- **Triggers** - BEFORE/AFTER row and statement level
- **Stored Procedures** - SBLR bytecode language
- **Row-Level Security** - Fine-grained access control
- **Domains** - Custom types with constraints and validation
- **Data Encryption** - Transparent data encryption with key management
- **Data Masking** - Column-level data masking
- **Audit Logging** - Structured audit trail
- **Parallel Execution** - Parallel query executor
- **Job Scheduler** - Background job scheduling

---

## Installation

### Quick Start with Docker

```bash
# Pull the official image
docker pull scratchbird/scratchbird:latest

# Run ScratchBird
docker run -d \
  --name scratchbird \
  -p 3092:3092 \
  -p 5432:5432 \
  -e SCRATCHBIRD_PASSWORD=mypassword \
  -v scratchbird_data:/var/lib/scratchbird/data \
  scratchbird/scratchbird:latest

# Connect
sb_isql -H localhost -p 3092 -U scratchbird -d mydb
# Or use PostgreSQL protocol:
psql -h localhost -p 5432 -U scratchbird -d mydb
```

### Platform-Specific Installation

- [**Linux**](installation/Linux.md) - DEB, RPM, AppImage
- [**Windows**](installation/Windows.md) - MSI installer
- [**macOS**](installation/macOS.md) - Homebrew, DMG
- [**Docker**](installation/Docker.md) - Container deployment
- [**Kubernetes**](installation/Kubernetes.md) - Cloud-native deployment

---

## First Steps

### 1. Install ScratchBird
Choose your platform from the [installation guides](installation/README.md).

### 2. Connect to Database
Use a native ScratchBird client or a dialect client:

```bash
# Command-line
sb_isql -H localhost -p 3092 -U scratchbird -d mydb
psql -h localhost -p 5432 -U scratchbird -d mydb

# Or use your preferred GUI
# DBeaver, pgAdmin, DataGrip, etc.
```

### 3. Run Your First Query

```sql
-- Create a table
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    email VARCHAR(100) UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert data
INSERT INTO users (name, email)
VALUES ('Alice', 'alice@example.com'),
       ('Bob', 'bob@example.com');

-- Query data
SELECT * FROM users;
```

### 4. Explore Features

- [**Transactions**](user-guides/Transactions.md) - Learn MGA transaction model
- [**Indexes**](user-guides/Indexes.md) - Optimize query performance
- [**Vector Search**](user-guides/Vector-Search.md) - Implement semantic search
- [**Security**](user-guides/Security.md) - Set up authentication and RLS

---

## Language Drivers

ScratchBird supports all major programming languages:

| Language | Driver | ORM Support | Status |
|----------|--------|-------------|--------|
| **Python** | [scratchbird](drivers/Python.md) | SQLAlchemy, Django | ✅ Native (SBWP v1.1) |
| **Node.js** | [scratchbird](drivers/NodeJS-TypeScript.md) | Prisma, TypeORM | ✅ Native (SBWP v1.1) |
| **Java** | [JDBC](drivers/Java-JDBC.md) | Hibernate, JPA | ✅ Native (SBWP v1.1) |
| **C#/.NET** | [ADO.NET](drivers/CSharp-DotNet.md) | EF Core | ✅ Native (SBWP v1.1) |
| **Go** | [database/sql](drivers/Go.md) | GORM, sqlx | ✅ Native (SBWP v1.1) |
| **PHP** | [PDO](drivers/PHP.md) | Laravel, WordPress | ✅ Native (SBWP v1.1) |
| **Pascal/Delphi** | [FireDAC](drivers/Pascal-Delphi.md) | IBX, Zeos | ✅ Native (SBWP v1.1) |
| **Ruby** | [scratchbird](drivers/Ruby.md) | Sequel, ActiveRecord | ✅ Native (SBWP v1.1) |
| **Rust** | [scratchbird](drivers/Rust.md) | Tokio, SeaORM | ✅ Native (SBWP v1.1) |
| **R** | [scratchbird](drivers/R.md) | DBI | ✅ Native (SBWP v1.1) |

---

## Migration

### Migrating to ScratchBird

**From Firebird:** ScratchBird implements the same MGA transaction model, making migration straightforward.
- [Firebird Migration Guide](migration/From-Firebird.md) ⭐
- [FireDAC/IBX Migration](drivers/Pascal-Delphi.md)

**From PostgreSQL:** Use PostgreSQL wire protocol compatibility.
- [PostgreSQL Migration Guide](migration/From-PostgreSQL.md)
- [pg_dump/restore workflow](migration/From-PostgreSQL.md#data-migration)

**From MySQL:** SQL dialect compatibility layer.
- [MySQL Migration Guide](migration/From-MySQL.md)
- [WordPress Migration](applications/WordPress.md)

---

## Community and Support

### Get Help

- **Discord:** Join our community server → [discord.gg/scratchbird](#)
- **Stack Overflow:** Tag your questions with `scratchbird`
- **GitHub Issues:** Report bugs and request features
- **Documentation:** Browse this wiki for answers

### Contribute

- **Report Bugs:** [GitHub Issues](#)
- **Suggest Features:** [Feature Requests](#)
- **Improve Docs:** [Edit this wiki](#)
- **Write Code:** [Contributing Guide](Contributing.md)

### Resources

- **Website:** https://scratchbird.dev
- **GitHub:** https://github.com/scratchbird/scratchbird
- **API Docs:** https://scratchbird.dev/api/ (auto-generated)
- **Blog:** https://scratchbird.dev/blog/

---

## Roadmap

### Alpha (Current)
- ✅ Core database engine with Firebird MGA transaction model
- ✅ Comprehensive indexing (B-tree, Hash, GiST, GIN, SP-GiST, BRIN, R-tree, Bitmap, LSM-Tree, HNSW, Columnstore, Full-text, Inverted)
- ✅ Vector search support with HNSW indexing
- ✅ PostgreSQL wire protocol
- ✅ V2 parser with native SQL dialect
- ✅ Emulated parsers for Firebird, PostgreSQL, and MySQL dialects
- ✅ SBLR bytecode compilation and execution
- ✅ Authentication (SCRAM, Kerberos, LDAP, OAuth, SAML, MFA, Certificate)
- ✅ Data encryption, data masking, audit logging
- ✅ Backup and restore management
- ✅ Full-text search with tsvector/tsquery
- ✅ Spatial types with GEOS and PROJ integration
- ✅ TOAST large object storage with visibility
- ✅ Garbage collection and sweep management
- ✅ Buffer pool, page management, compressed pages
- ✅ Job scheduler for background tasks
- ✅ Language drivers (Python, Node.js, Java, C#, Go, PHP, Pascal)
- 🚧 BI tool integration (ODBC, Tableau, Power BI)
- 🚧 Kubernetes operator

### Beta
- Production stability hardening
- App-specific drivers (Metabase, Superset) and BI tooling
- Streaming connectors (Kafka, Spark)
- Performance optimization

### 1.0 Release
- Production-ready stability
- Complete documentation
- Enterprise features
- Support and training

### Future
- Distributed clustering
- Read replicas
- Cloud-managed service
- Advanced analytics
- Time-series optimizations

---

## License

ScratchBird is open-source software licensed under [LICENSE_TYPE].

---

## Status

> ⚠️ **Alpha Software**
>
> ScratchBird is currently in **Alpha**. The core engine, indexing, transaction model, and parser infrastructure are implemented and tested, but the API may change before 1.0 release.
> Not recommended for production use. We recommend thorough testing and expect breaking changes.
>
> **Stability:** Alpha quality, breaking changes expected
> **Support:** Community support via Discord and GitHub
> **Production Ready:** Not recommended for production workloads

---

**Welcome to ScratchBird!** Start with the [Getting Started Guide](Getting-Started.md) →

---

*Last updated: 2026-01-30 | Wiki version synced with codebase*
