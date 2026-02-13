# ORM & Framework Support

**[← Back to Beta Requirements](../README.md)** | **[← Back to Specifications Index](../../README.md)**

This directory contains specifications for ORM (Object-Relational Mapping) and framework integrations.

## Overview

ScratchBird provides native support for the most popular ORMs and frameworks across all major programming ecosystems, ensuring seamless integration with existing applications.

## ORM Specifications

### P0 - Critical (Beta Required)

**[sqlalchemy/](sqlalchemy/)** - SQLAlchemy (Python)
- Dialect implementation
- Core and ORM support
- Async support
- **Market Share:** Dominant Python ORM

**[sequelize/](sequelize/)** - Sequelize (Node.js)
- Dialect implementation
- Migrations support
- TypeScript types
- **Market Share:** Most popular Node.js ORM

**[hibernate-jpa/](hibernate-jpa/)** - Hibernate/JPA (Java)
- JPA 2.2+ compliance
- Hibernate dialect
- Spring Data JPA support
- **Market Share:** Dominant Java ORM

**[entity-framework-core/](entity-framework-core/)** - Entity Framework Core (.NET)
- Database provider implementation
- Migrations support
- LINQ support
- **Market Share:** Dominant .NET ORM

### P1 - High Priority

**[typeorm/](typeorm/)** - TypeORM (TypeScript)
- Full TypeScript support
- Decorators and metadata
- Migrations

**[prisma/](prisma/)** - Prisma (Node.js/TypeScript)
- Prisma Client generation
- Prisma Migrate support
- Schema definition language

**[rails-active-record/](rails-active-record/)** - Rails ActiveRecord (Ruby)
- Adapter implementation
- Migrations support
- Rails integration

**[laravel-eloquent/](laravel-eloquent/)** - Laravel Eloquent (PHP)
- Database driver integration
- Query builder support
- Migrations

**[dapper/](dapper/)** - Dapper (.NET)
- Micro-ORM support
- Performance optimization

### P2 - Medium Priority

**[django-orm/](django-orm/)** - Django ORM (Python)
- Database backend implementation
- Migrations support
- Admin interface support

### Specialized Frameworks

**[cypher-opencypher/](cypher-opencypher/)** - Cypher/OpenCypher
- Graph query language support
- Neo4j compatibility layer

**[gremlin-tinkerpop/](gremlin-tinkerpop/)** - Gremlin/TinkerPop
- Graph traversal support
- Apache TinkerPop integration

## ORM Features Matrix

| ORM | Language | Type | Schema Mgmt | Async | Status |
|-----|----------|------|-------------|-------|--------|
| SQLAlchemy | Python | Full ORM | ✅ Alembic | ✅ | ✅ Specified |
| Sequelize | Node.js | Full ORM | ✅ Migrations | ✅ | ✅ Specified |
| Hibernate/JPA | Java | Full ORM | ✅ Flyway/Liquibase | ❌ | ✅ Specified |
| Entity Framework Core | .NET | Full ORM | ✅ Migrations | ✅ | ✅ Specified |
| TypeORM | TypeScript | Full ORM | ✅ Migrations | ✅ | ✅ Specified |
| Prisma | Node.js/TS | Full ORM | ✅ Prisma Migrate | ✅ | ✅ Specified |
| ActiveRecord | Ruby | Full ORM | ✅ Migrations | ❌ | ✅ Specified |
| Eloquent | PHP | Full ORM | ✅ Migrations | ❌ | ✅ Specified |
| Dapper | .NET | Micro-ORM | ❌ | ✅ | ✅ Specified |
| Django ORM | Python | Full ORM | ✅ Migrations | ✅ | ✅ Specified |

## Testing Requirements

Each ORM integration must pass:

- **CRUD operations** - Create, Read, Update, Delete
- **Relations** - One-to-one, one-to-many, many-to-many
- **Queries** - Complex queries, joins, aggregations
- **Transactions** - Transaction management
- **Migrations** - Schema migrations (if supported)
- **Performance** - Benchmark against native databases

## Related Specifications

- [Drivers](../drivers/) - Native database drivers
- [Connectivity](../connectivity/) - ODBC/JDBC
- [Applications](../applications/) - Application integrations

## Navigation

- **Parent Directory:** [Beta Requirements](../README.md)
- **Specifications Index:** [Specifications Index](../../README.md)
- **Project Root:** [ScratchBird Home](../../../../README.md)

---

**Last Updated:** January 2026
