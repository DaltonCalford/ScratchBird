# Driver Comparison

**Status:** Complete
**Last Updated:** 2026-01-30

---

## Overview

This guide compares all available drivers and connection methods for ScratchBird, helping you choose the right approach for your application.

**Quick Recommendation:**
- **ScratchBird native drivers (SBWP v1.1)**: Preferred for full feature access on port 3092.
- **Web applications**: Use native drivers when possible; fall back to PostgreSQL drivers for ecosystem tools.
- **Enterprise/Desktop**: Use native JDBC/.NET/Pascal drivers; ODBC for BI tools.
- **Migrating from Firebird/MySQL**: Use those protocols for compatibility during cutover.

---

## Protocol Support Matrix

ScratchBird supports multiple wire protocols, allowing connections from various client ecosystems:

| Protocol | Port | Best For | Compatibility |
|----------|------|----------|---------------|
| **Native (SBWP v1.1)** | 3092 | Full ScratchBird feature set | ScratchBird native drivers |
| **PostgreSQL** | 5432 | Ecosystem compatibility | PostgreSQL client ecosystem |
| **MySQL** | 3306 | MySQL migrations, MySQL-native apps | MySQL client ecosystem |
| **Firebird** | 3050 | Firebird migrations, legacy apps | Firebird client ecosystem |

---

## Driver Overview by Language

### Quick Selection Guide

| Language | Recommended Driver | Protocol | Use Case |
|----------|-------------------|----------|----------|
| Python | scratchbird (native) | Native (3092) | Full ScratchBird feature set |
| Node.js | scratchbird (native) | Native (3092) | Full ScratchBird feature set |
| Java | ScratchBird JDBC | Native (3092) | Enterprise, Spring Boot |
| C# / .NET | ScratchBird.Data | Native (3092) | ASP.NET, desktop apps |
| Go | scratchbird-go | Native (3092) | Microservices, CLI tools |
| PHP | ScratchBird PDO | Native (3092) | Web apps, Laravel/Symfony |
| Delphi | ScratchBird Pascal/Delphi | Native (3092) | Desktop apps, RAD |
| Ruby | scratchbird | Native (3092) | Scripting, web apps |
| Rust | scratchbird | Native (3092) | Async services, CLIs |
| R | scratchbird | Native (3092) | Analytics, notebooks |
| Any | ODBC (psqlODBC) | PostgreSQL (5432) | BI tools, Excel, Access |

**Note:** The tables below compare **emulation drivers** (PostgreSQL/MySQL/Firebird). Use native
ScratchBird drivers for full SBWP feature coverage (binary prepare/bind, wrapper types, and
driver-level parity).

---

## Detailed Comparison Tables

### Connection & Authentication

| Driver | Pooling | SSL/TLS | SCRAM-SHA-256 | Kerberos | Certificate Auth |
|--------|---------|---------|---------------|----------|------------------|
| **Python (psycopg2)** | External (pool) | Yes | Yes | Yes | Yes |
| **Python (asyncpg)** | Built-in | Yes | Yes | No | Yes |
| **Node.js (pg)** | Built-in | Yes | Yes | No | Yes |
| **Java (JDBC)** | External (HikariCP) | Yes | Yes | Yes | Yes |
| **C# (Npgsql)** | Built-in | Yes | Yes | Yes | Yes |
| **Go (pgx)** | Built-in (pgxpool) | Yes | Yes | No | Yes |
| **PHP (PDO)** | Persistent | Yes | Yes | No | Yes |
| **Delphi (FireDAC)** | Built-in | Yes | Yes | No | Limited |
| **ODBC (psqlODBC)** | OS-level | Yes | Yes | Yes | Yes |

### Query Features

| Driver | Prepared Statements | Batch Operations | COPY Protocol | Async/Streaming | Transactions |
|--------|---------------------|------------------|---------------|-----------------|--------------|
| **Python (psycopg2)** | Yes | executemany | Yes | Async via psycopg3 | Full |
| **Python (asyncpg)** | Yes | executemany | Yes | Native async | Full |
| **Node.js (pg)** | Yes | Multi-query | Yes | Streams | Full |
| **Java (JDBC)** | Yes | addBatch | Yes (CopyManager) | ResultSet streaming | Full |
| **C# (Npgsql)** | Yes | Batch command | Yes | Async/await | Full |
| **Go (pgx)** | Yes | Batch | CopyFrom | Context-based | Full |
| **PHP (PDO)** | Yes | Loop execute | pgsqlCopyFrom | No | Full |
| **Delphi (FireDAC)** | Yes | Array DML | No | No | Full |
| **ODBC** | Yes | Parameter arrays | No | No | Full |

### ORM & Framework Support

| Language | Native ORM | Popular Frameworks | Query Builders |
|----------|------------|-------------------|----------------|
| **Python** | SQLAlchemy | Django, FastAPI, Flask | SQLAlchemy Core |
| **Node.js** | Prisma, TypeORM | Express, NestJS, Fastify | Knex.js |
| **Java** | Hibernate/JPA | Spring Boot, Quarkus | jOOQ, QueryDSL |
| **C#** | Entity Framework Core | ASP.NET Core, Blazor | Dapper, LINQ |
| **Go** | GORM | Gin, Echo, Fiber | sqlx, squirrel |
| **PHP** | Doctrine, Eloquent | Laravel, Symfony | Query Builder |
| **Delphi** | FireDAC datasets | VCL, FMX | - |

### Data Type Support

| Driver | JSON/JSONB | Arrays | UUID | Vectors | Geometry | Custom Types |
|--------|------------|--------|------|---------|----------|--------------|
| **Python (psycopg2)** | Dict/Json | List | uuid.UUID | pgvector | PostGIS | register_type |
| **Python (asyncpg)** | Dict | List | uuid.UUID | pgvector | PostGIS | set_type_codec |
| **Node.js (pg)** | Object | Array | String | pgvector | postgis | setTypeParser |
| **Java (JDBC)** | String/PGobject | Array | UUID | Manual | JTS | PGobject |
| **C# (Npgsql)** | JsonDocument | Array | Guid | NpgsqlVector | NetTopology | TypeMapper |
| **Go (pgx)** | map/struct | Slice | [16]byte | pgvector-go | - | RegisterType |
| **PHP (PDO)** | String (json) | String (parse) | String | Manual | Manual | - |
| **Delphi (FireDAC)** | String | Variant | String | Manual | - | - |
| **ODBC** | String | String | String | - | - | - |

---

## Performance Characteristics

### Connection Overhead

| Driver | Connection Time | Memory per Connection | Recommended Pool Size |
|--------|----------------|----------------------|----------------------|
| **Python (psycopg2)** | ~50ms | ~2MB | 5-20 |
| **Python (asyncpg)** | ~30ms | ~1.5MB | 10-50 |
| **Node.js (pg)** | ~40ms | ~1MB | 10-20 |
| **Java (JDBC)** | ~60ms | ~3MB | 10-50 |
| **C# (Npgsql)** | ~40ms | ~2MB | 10-100 |
| **Go (pgx)** | ~30ms | ~1MB | 10-50 |
| **PHP (PDO)** | ~50ms | ~2MB | Persistent |
| **Delphi (FireDAC)** | ~50ms | ~3MB | 5-25 |
| **ODBC** | ~80ms | ~2MB | OS-managed |

*Note: Times are approximate and depend on network latency, SSL negotiation, and authentication method.*

### Throughput (Relative Performance)

| Driver | Simple Queries | Bulk Inserts | COPY Performance | Async Advantage |
|--------|---------------|--------------|------------------|-----------------|
| **Python (asyncpg)** | Excellent | Excellent | Excellent | High |
| **Go (pgx)** | Excellent | Excellent | Excellent | High |
| **C# (Npgsql)** | Excellent | Very Good | Excellent | High |
| **Java (JDBC)** | Very Good | Very Good | Very Good | Medium |
| **Node.js (pg)** | Very Good | Good | Very Good | High |
| **Python (psycopg2)** | Good | Good | Very Good | Low |
| **PHP (PDO)** | Good | Good | Good | None |
| **Delphi (FireDAC)** | Good | Very Good | N/A | None |
| **ODBC** | Moderate | Moderate | N/A | None |

---

## Feature Availability by Protocol

### PostgreSQL Protocol (Port 5432) - Recommended

| Feature | Support Level | Notes |
|---------|---------------|-------|
| Prepared statements | Full | Server-side preparation |
| COPY protocol | Full | Bulk load/export |
| LISTEN/NOTIFY | Full | Real-time notifications |
| Large objects | Full | Binary large objects |
| Two-phase commit | Full | Distributed transactions |
| SSL/TLS | Full | All modes supported |
| SCRAM authentication | Full | SHA-256 based |

### MySQL Protocol (Port 3306)

| Feature | Support Level | Notes |
|---------|---------------|-------|
| Prepared statements | Full | Server-side preparation |
| LOAD DATA | Partial | Via COPY translation |
| Multi-statements | Full | Batch execution |
| SSL/TLS | Full | All modes supported |
| Native authentication | Full | MySQL auth plugins |

### Firebird Protocol (Port 3050)

| Feature | Support Level | Notes |
|---------|---------------|-------|
| Prepared statements | Full | Server-side preparation |
| EXECUTE BLOCK | Full | Anonymous procedures |
| Generators | Full | Sequence support |
| SSL/TLS | Full | All modes supported |
| SYSDBA authentication | Full | Traditional auth |

---

## Language-Specific Details

### Python

**Recommended stack:**
```
psycopg2 or asyncpg → SQLAlchemy → FastAPI/Django
```

| Library | Best For | Async | Pool |
|---------|----------|-------|------|
| psycopg2 | Synchronous apps, Django | No | External |
| psycopg3 | Modern sync/async apps | Yes | Built-in |
| asyncpg | High-performance async | Yes | Built-in |
| SQLAlchemy | ORM, query building | Yes (2.0) | Built-in |

**Guide:** [Python Driver Guide](Python.md)

---

### Node.js / TypeScript

**Recommended stack:**
```
pg → Prisma/TypeORM → Express/NestJS
```

| Library | Best For | TypeScript | Pool |
|---------|----------|------------|------|
| pg | Direct SQL, low-level | @types/pg | Built-in |
| pg-promise | Query templating | Native | Built-in |
| Prisma | Type-safe ORM | Native | Built-in |
| TypeORM | Decorators, Active Record | Native | Built-in |
| Knex.js | Query builder | @types/knex | Built-in |

**Guide:** [Node.js/TypeScript Driver Guide](NodeJS-TypeScript.md)

---

### Java

**Recommended stack:**
```
PostgreSQL JDBC → HikariCP → Spring Boot/JPA
```

| Library | Best For | Spring Integration | Pool |
|---------|----------|-------------------|------|
| PostgreSQL JDBC | Direct JDBC | Yes | External |
| HikariCP | Connection pooling | Yes | Native |
| Spring Data JPA | Repository pattern | Native | Inherited |
| jOOQ | Type-safe SQL | Yes | Inherited |

**Guide:** [Java/JDBC Driver Guide](Java-JDBC.md)

---

### C# / .NET

**Recommended stack:**
```
Npgsql → Dapper/EF Core → ASP.NET Core
```

| Library | Best For | Async | Pool |
|---------|----------|-------|------|
| Npgsql | ADO.NET provider | Yes | Built-in |
| Dapper | Micro-ORM, performance | Yes | Inherited |
| Entity Framework Core | Full ORM | Yes | Inherited |
| LINQ to DB | LINQ queries | Yes | Inherited |

**Guide:** [C#/.NET Driver Guide](CSharp-DotNet.md)

---

### Go

**Recommended stack:**
```
pgx → sqlx/GORM → Gin/Echo
```

| Library | Best For | database/sql | Pool |
|---------|----------|--------------|------|
| pgx | Native driver, performance | Optional | pgxpool |
| lib/pq | Legacy, database/sql | Required | sql.DB |
| sqlx | Extended sql, scanning | Required | sql.DB |
| GORM | Full ORM | No | Built-in |

**Guide:** [Go Driver Guide](Go.md)

---

### PHP

**Recommended stack:**
```
PDO (pgsql) → Doctrine/Eloquent → Laravel/Symfony
```

| Library | Best For | Framework | Pool |
|---------|----------|-----------|------|
| PDO pgsql | Direct SQL | Any | Persistent |
| Doctrine DBAL | Abstraction layer | Symfony | External |
| Eloquent | Active Record ORM | Laravel | External |

**Guide:** [PHP Driver Guide](PHP.md)

---

### Delphi / Pascal

**Recommended stack:**
```
FireDAC → VCL/FMX components
```

| Library | Best For | IDE | Pool |
|---------|----------|-----|------|
| FireDAC | RAD Studio apps | Delphi | Built-in |
| IBX | Legacy Firebird | Delphi | No |
| Zeos | Cross-platform, FPC | Any | No |
| SQLdb | Free Pascal | Lazarus | No |

**Guide:** [Pascal/Delphi Driver Guide](Pascal-Delphi.md)

---

### ODBC

**Recommended for:**
- Microsoft Excel, Access, Power BI
- Crystal Reports, SSRS
- Legacy applications
- Cross-platform tooling

| Driver | Protocol | Platform | Best For |
|--------|----------|----------|----------|
| psqlODBC | PostgreSQL | All | Most applications |
| MySQL ODBC | MySQL | All | MySQL compatibility |
| Firebird ODBC | Firebird | All | Firebird migration |

**Guide:** [ODBC Driver Guide](ODBC.md)

---

## Decision Matrix

### By Application Type

| Application Type | Recommended Driver | Reason |
|------------------|-------------------|--------|
| REST API (Python) | asyncpg + FastAPI | High throughput, async |
| REST API (Node.js) | pg + Express | Event-driven, streaming |
| REST API (Java) | JDBC + Spring Boot | Enterprise patterns |
| REST API (C#) | Npgsql + ASP.NET | .NET ecosystem |
| REST API (Go) | pgx + Gin | Performance, simplicity |
| Web App (PHP) | PDO + Laravel | Full-stack framework |
| Desktop App | FireDAC / Npgsql | Rich data binding |
| Data Science | psycopg2 + pandas | DataFrame integration |
| BI/Reporting | ODBC | Universal connectivity |
| Microservices | pgx / asyncpg | Lightweight, fast |

### By Migration Source

| Source Database | Recommended Protocol | Driver Strategy |
|-----------------|---------------------|-----------------|
| PostgreSQL | PostgreSQL (5432) | Keep existing drivers |
| MySQL | MySQL (3306) initially | Migrate to PostgreSQL protocol |
| Firebird | Firebird (3050) | Keep for compatibility |
| SQL Server | PostgreSQL (5432) | Use PostgreSQL drivers |
| Oracle | PostgreSQL (5432) | Use PostgreSQL drivers |

### By Performance Priority

| Priority | Driver Choice | Configuration |
|----------|---------------|---------------|
| Latency | pgx, asyncpg | Prepared statements, pooling |
| Throughput | Any + COPY | Bulk operations, batching |
| Concurrency | asyncpg, pgx | Async patterns, connection pools |
| Memory | pgx, pg | Streaming results, cursors |

---

## Migration Considerations

### Switching Protocols

If migrating from MySQL or Firebird protocols to PostgreSQL:

1. **SQL Syntax**: Most SELECT/INSERT/UPDATE/DELETE works unchanged
2. **Parameter markers**: MySQL uses `?`, PostgreSQL uses `$1, $2`
3. **Functions**: Some function names differ (see language guides)
4. **Data types**: Type mappings documented in migration guides

### Switching Drivers

When switching drivers within the same language:

| From | To | Considerations |
|------|----|----------------|
| psycopg2 | asyncpg | Async/await patterns, connection strings |
| lib/pq | pgx | Context usage, type handling |
| mysql2 | pg | Parameter placeholders, result format |
| mysqli | PDO | Prepared statement syntax |

---

## Quick Start Examples

### Minimal Connection by Language

**Python:**
```python
import psycopg2
conn = psycopg2.connect("postgresql://user:pass@localhost:5432/db")
```

**Node.js:**
```javascript
const { Pool } = require('pg');
const pool = new Pool({ connectionString: 'postgresql://user:pass@localhost:5432/db' });
```

**Java:**
```java
Connection conn = DriverManager.getConnection("jdbc:postgresql://localhost:5432/db", "user", "pass");
```

**C#:**
```csharp
await using var conn = new NpgsqlConnection("Host=localhost;Database=db;Username=user;Password=pass");
await conn.OpenAsync();
```

**Go:**
```go
conn, err := pgx.Connect(context.Background(), "postgresql://user:pass@localhost:5432/db")
```

**PHP:**
```php
$pdo = new PDO("pgsql:host=localhost;port=5432;dbname=db", "user", "pass");
```

**Delphi:**
```pascal
FDConnection1.Params.Values['Server'] := 'localhost';
FDConnection1.Params.Values['Database'] := 'db';
FDConnection1.Connected := True;
```

---

## See Also

- [Python Driver Guide](Python.md)
- [Node.js/TypeScript Driver Guide](NodeJS-TypeScript.md)
- [Java/JDBC Driver Guide](Java-JDBC.md)
- [C#/.NET Driver Guide](CSharp-DotNet.md)
- [Go Driver Guide](Go.md)
- [PHP Driver Guide](PHP.md)
- [Pascal/Delphi Driver Guide](Pascal-Delphi.md)
- [ODBC Driver Guide](ODBC.md)
- [Connection Guide](../getting-started/first-connection.md)
- [Performance Tuning](../user-guides/Performance-Tuning.md)
