![ScratchBird Logo](../images/logos/TransparentScratchBirdLogoHeader.png)

# Welcome to ScratchBird Wiki

**Version:** Alpha (documentation in progress)
**Last Updated:** 2026-01-18

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
- **Vector Search** - Native pgvector-compatible vector operations
- **Advanced Indexing** - B-tree, Hash, GiST, GIN, Bloom filters, LSM
- **TOAST/LOB Storage** - Efficient large object handling
- **Zone Maps** - Fast data pruning for analytical queries

### 🌐 Broad Compatibility
- **SQL-92/SQL-99** compliance
- **PostgreSQL wire protocol** compatibility
- **MySQL dialect** support
- **Firebird migration** path

---

## Key Features

### Transaction Management
- **Firebird MGA** - Multi-generational architecture
- **ACID Compliance** - Full transaction guarantees
- **Isolation Levels** - READ COMMITTED, REPEATABLE READ, SERIALIZABLE
- **Savepoints** - Partial rollback support
- **Two-Phase Commit** - Distributed transaction support

### Data Types
- **Numeric** - INTEGER, BIGINT, DECIMAL, NUMERIC, REAL, DOUBLE
- **String** - CHAR, VARCHAR, TEXT with UTF-8 support
- **Binary** - BYTEA, BLOB
- **Date/Time** - DATE, TIME, TIMESTAMP with timezone support
- **JSON** - JSONB for structured data
- **Vector** - Native vector type for AI/ML
- **Spatial** - PostGIS-compatible geometry types
- **Domain** - Custom types with validation

### Indexing
- **B-tree** - General-purpose indexing
- **Hash** - Equality lookups
- **GiST/GIN** - Full-text search, arrays, JSON
- **Bloom Filters** - Space-efficient membership testing
- **LSM Trees** - Write-optimized indexing
- **Vector Indexes** - HNSW for similarity search
- **Expression Indexes** - Index on computed columns

### Advanced Features
- **CTEs** - Common Table Expressions including RECURSIVE
- **Window Functions** - Analytics and aggregations
- **Views** - Regular and materialized views
- **Triggers** - BEFORE/AFTER row and statement level
- **Stored Procedures** - SBLR bytecode language
- **Row-Level Security** - Fine-grained access control
- **Domains** - Custom types with constraints

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
| **Python** | [scratchbird](drivers/Python.md) | SQLAlchemy, Django | ✅ Beta |
| **Node.js** | [scratchbird](drivers/NodeJS-TypeScript.md) | Prisma, TypeORM | ✅ Beta |
| **Java** | [JDBC](drivers/Java-JDBC.md) | Hibernate, JPA | ✅ Beta |
| **C#/.NET** | [ADO.NET](drivers/CSharp-DotNet.md) | EF Core | ✅ Beta |
| **Go** | [database/sql](drivers/Go.md) | GORM, sqlx | ✅ Beta |
| **PHP** | [PDO](drivers/PHP.md) | Laravel, WordPress | ✅ Beta |
| **Pascal/Delphi** | [FireDAC](drivers/Pascal-Delphi.md) | IBX, Zeos | ✅ Beta |

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

### Beta (Current)
- ✅ Core database engine with MGA
- ✅ All 7 P0 language drivers
- ✅ Vector search support
- ✅ Advanced indexing (B-tree, Hash, GiST, GIN, LSM)
- ✅ PostgreSQL wire protocol
- 🚧 BI tool integration (ODBC, Tableau, Power BI)
- 🚧 Kubernetes operator
- 🚧 Streaming (Kafka, Spark connectors)

### 1.0 Release (Q2 2026)
- Production-ready stability
- Performance optimization
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

> ⚠️ **Beta Software**
>
> ScratchBird is currently in **Beta**. While functional and tested, the API may change before 1.0 release.
> Use in production at your own risk. We recommend thorough testing before production deployment.
>
> **Stability:** Beta quality, breaking changes possible
> **Support:** Community support via Discord and GitHub
> **Production Ready:** Not recommended for critical production workloads yet

---

**Welcome to ScratchBird!** Start with the [Getting Started Guide](Getting-Started.md) →

---

*Last updated: 2026-01-03 | Wiki version synced with codebase*
