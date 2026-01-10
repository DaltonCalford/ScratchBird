# Programming Language Drivers

**[← Back to Beta Requirements](../README.md)** | **[← Back to Specifications Index](../../README.md)**

This directory contains specifications for native database drivers across all major programming languages.

## Overview

ScratchBird provides native drivers for the most popular programming languages, ensuring broad developer adoption. Each driver is designed to feel idiomatic to its language while maintaining compatibility with existing database libraries and frameworks.

## Driver Specifications

### P0 - Critical (Beta Required)

**[python/](python/)** - Python Database Driver
- PEP 249 DB-API 2.0 compliance
- SQLAlchemy support
- Pandas/NumPy integration
- Async/await support
- **Market Share:** >50% of developers

**[nodejs-typescript/](nodejs-typescript/)** - Node.js/TypeScript Driver
- Promise-based async API
- Full TypeScript type definitions
- ES6+ pattern support
- Connection pooling
- **Market Share:** Most-used language

**[java-jdbc/](java-jdbc/)** - JDBC Driver for Java/JVM
- JDBC 4.2+ compliance
- Connection pooling
- Prepared statements
- Transaction management
- **Market Share:** ~30% of developers

**[dotnet-csharp/](dotnet-csharp/)** - .NET/C# Driver
- ADO.NET compliance
- IDbConnection interface
- Async/await support
- .NET 6.0+ compatibility
- **Market Share:** ~28% of developers

**[golang/](golang/)** - Go Driver
- database/sql.Driver interface
- Context support
- Connection pooling
- **Market Share:** Growing adoption

**[php/](php/)** - PHP Driver
- PDO driver compatibility
- mysqli compatibility
- WordPress support (43% of websites)
- Laravel framework support
- **Market Share:** ~18% of developers, 80% of websites

**[pascal-delphi/](pascal-delphi/)** - Pascal/Delphi/FreePascal Driver
- FireDAC driver (Delphi XE5+)
- IBX (InterBase Express) compatibility
- Zeos database components
- FreePascal/Lazarus SQLdb components
- Firebird migration toolkit
- **Strategic:** Firebird migration path

### P1 - High Priority

**[ruby/](ruby/)** - Ruby Driver
- ActiveRecord support
- Rails integration
- **Market Share:** ~5% of developers

**[rust/](rust/)** - Rust Driver
- tokio async runtime support
- sqlx compatibility
- **Market Share:** Growing adoption, systems programming

### P2 - Medium Priority

**[r/](r/)** - R Driver
- DBI interface compliance
- dplyr integration
- **Use Case:** Statistical analysis

**[cpp/](cpp/)** - C/C++ Driver
- Native C API
- C++ wrapper classes
- **Use Case:** High-performance applications

## Baseline Specification

**[DRIVER_BASELINE_SPEC.md](DRIVER_BASELINE_SPEC.md)** - Common requirements for all drivers

All drivers must support:
- Connection management (connect, disconnect, reconnect)
- Query execution (simple and prepared statements)
- Transaction control (BEGIN, COMMIT, ROLLBACK)
- Result set handling (fetch, iterate, metadata)
- Error handling and reporting
- Connection pooling (or integration with language-standard pooling)
- TLS/SSL encryption
- Authentication (multiple methods)

## Driver Features Matrix

| Language | DB-API/Standard | Async Support | Pooling | ORM Support | Status |
|----------|----------------|---------------|---------|-------------|--------|
| Python | PEP 249 | ✅ asyncio | ✅ | SQLAlchemy, Django | ✅ Specified |
| Node.js/TypeScript | N/A | ✅ Promise | ✅ | Sequelize, TypeORM, Prisma | ✅ Specified |
| Java | JDBC 4.2+ | ✅ | ✅ | Hibernate, JPA | ✅ Specified |
| C#/.NET | ADO.NET | ✅ async/await | ✅ | Entity Framework Core | ✅ Specified |
| Go | database/sql | ✅ Context | ✅ | GORM, sqlx | ✅ Specified |
| PHP | PDO/mysqli | ✅ | ✅ | Laravel, Doctrine | ✅ Specified |
| Pascal/Delphi | FireDAC/IBX | ✅ | ✅ | FireDAC, Zeos | ✅ Specified |
| Ruby | DBI | ✅ | ✅ | ActiveRecord | ✅ Specified |
| Rust | N/A | ✅ tokio | ✅ | sqlx, diesel | ✅ Specified |
| R | DBI | ❌ | ✅ | dplyr | ✅ Specified |
| C/C++ | Custom | ✅ | ✅ | N/A | ⏳ Specified |

## Implementation Priority

### Phase 1 (Critical for Beta)
1. Python (psycopg2-compatible)
2. Node.js/TypeScript (pg-compatible)
3. Java JDBC
4. C#/.NET (Npgsql-compatible)

### Phase 2 (Beta completion)
5. Go (pgx-compatible)
6. PHP (PDO PostgreSQL-compatible)
7. Pascal/Delphi (Firebird-compatible)

### Phase 3 (Post-Beta)
8. Ruby
9. Rust
10. R
11. C/C++

## Testing Requirements

Each driver must pass:

- **Unit tests** - Test driver internals
- **Integration tests** - Test against live ScratchBird server
- **Compatibility tests** - Test with popular libraries/ORMs
- **Performance tests** - Benchmark against native drivers
- **Stress tests** - Test under high load

## Related Specifications

- [ORM Support](../orms-frameworks/) - ORM and framework integrations
- [Connectivity](../connectivity/) - ODBC/JDBC specifications
- [Wire Protocols](../../wire_protocols/) - Wire protocol implementations
- [API](../../api/) - Client library API design

## Navigation

- **Parent Directory:** [Beta Requirements](../README.md)
- **Specifications Index:** [Specifications Index](../../README.md)
- **Project Root:** [ScratchBird Home](../../../../README.md)

---

**Last Updated:** January 2026
**Status:** ✅ All P0/P1 drivers specified
**Target:** Beta release
