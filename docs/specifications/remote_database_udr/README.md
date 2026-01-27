# Remote Database UDR - Comprehensive Implementation Specification

> Reference-only: Canonical UDR and live-migration behavior now lives in
> `ScratchBird/docs/specifications/Alpha Phase 2/11-Remote-Database-UDR-Specification.md`
> and `ScratchBird/docs/specifications/Alpha Phase 2/11h-Live-Migration-Emulated-Listener.md`.

## Executive Summary

The Remote Database UDR (User Defined Routine) plugin transforms ScratchBird into a migration powerhouse by leveraging existing wire protocol implementations. This plugin enables:

- **Bidirectional Protocol Support**: PostgreSQL/MySQL/MSSQL/Firebird server implementations become client implementations
- **Zero-Downtime Migration**: Connect to legacy databases and migrate incrementally
- **Foreign Data Wrapper Pattern**: Query remote data as if it were local
- **Hybrid Queries**: JOIN local and remote tables transparently
- **Schema Discovery**: Automatically introspect remote database structure

**Scope Note:** MSSQL/TDS adapter support is post-gold; references to MSSQL are forward-looking.

**Implementation Time**: 8-12 weeks | **LOC**: ~15,000

---

## Document Structure

This specification is split into multiple files for maintainability:

| Document | Description | Size |
|----------|-------------|------|
| [01-CORE_TYPES.md](01-CORE_TYPES.md) | Core type definitions, enums, structures | ~400 lines |
| [02-CONNECTION_POOL.md](02-CONNECTION_POOL.md) | Remote connection pooling implementation | ~500 lines |
| [03-POSTGRESQL_ADAPTER.md](03-POSTGRESQL_ADAPTER.md) | PostgreSQL wire protocol client adapter | ~600 lines |
| [04-MYSQL_ADAPTER.md](04-MYSQL_ADAPTER.md) | MySQL wire protocol client adapter | ~600 lines |
| [05-MSSQL_FIREBIRD_ADAPTERS.md](05-MSSQL_FIREBIRD_ADAPTERS.md) | MSSQL (TDS, post-gold) and Firebird adapters | ~700 lines |
| [06-QUERY_EXECUTION.md](06-QUERY_EXECUTION.md) | Query execution layer and pushdown | ~500 lines |
| [07-SCHEMA_INTROSPECTION.md](07-SCHEMA_INTROSPECTION.md) | Remote schema discovery | ~400 lines |
| [08-SQL_SYNTAX.md](08-SQL_SYNTAX.md) | SQL syntax, DDL, and examples | ~500 lines |
| [09-MIGRATION_WORKFLOWS.md](09-MIGRATION_WORKFLOWS.md) | Migration patterns and workflows | ~400 lines |

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────┐
│  ScratchBird Database Engine                                  │
│                                                               │
│  ┌─────────────────────────────────────────────────────┐     │
│  │  SQL Query Layer                                     │     │
│  │  "SELECT * FROM legacy_users WHERE id > 1000"       │     │
│  └─────────────────┬───────────────────────────────────┘     │
│                    │                                          │
│  ┌─────────────────▼───────────────────────────────────┐     │
│  │  Query Planner/Optimizer                             │     │
│  │  - Recognizes 'legacy_users' is FOREIGN TABLE        │     │
│  │  - Determines pushdown opportunities                 │     │
│  │  - Cost estimation (local vs remote)                 │     │
│  └─────────────────┬───────────────────────────────────┘     │
│                    │                                          │
│  ┌─────────────────▼───────────────────────────────────┐     │
│  │  Execution Engine                                    │     │
│  │  ┌──────────────┐  ┌──────────────────────────┐     │     │
│  │  │ Local Scan   │  │ Foreign Scan             │     │     │
│  │  │ (MGA/MVCC)   │  │ (Remote DB UDR)          │     │     │
│  │  └──────────────┘  └──────────┬───────────────┘     │     │
│  └───────────────────────────────┼─────────────────────┘     │
└──────────────────────────────────┼─────────────────────────────┘
                                   │ UDR Call Interface
┌──────────────────────────────────▼─────────────────────────────┐
│  Remote Database UDR Plugin (libscratchbird_remote_db.so)      │
│                                                                 │
│  ┌───────────────────────────────────────────────────────┐    │
│  │  Connection Pool Registry                              │    │
│  │  - Multiple remote databases                           │    │
│  │  - Per-database connection pooling                     │    │
│  │  - Health monitoring                                   │    │
│  │  - Statistics collection                               │    │
│  └─────────────────┬─────────────────────────────────────┘    │
│                    │                                           │
│  ┌─────────────────▼─────────────────────────────────────┐    │
│  │  Protocol Adapters (Wire Protocol Clients)             │    │
│  │                                                         │    │
│  │  ┌─────────────┐  ┌─────────────┐  ┌────────────┐     │    │
│  │  │ PostgreSQL  │  │   MySQL     │  │   MSSQL    │     │    │
│  │  │  Adapter    │  │   Adapter   │  │  Adapter   │     │    │
│  │  │  (native)   │  │  (native)   │  │ TDS (post-gold) │ │    │
│  │  └─────────────┘  └─────────────┘  └────────────┘     │    │
│  │                                                         │    │
│  │  ┌─────────────┐  ┌─────────────┐                      │    │
│  │  │  Firebird   │  │ ScratchBird │                      │    │
│  │  │  Adapter    │  │  Adapter    │                      │    │
│  │  │  (native)   │  │  (native)   │                      │    │
│  │  └─────────────┘  └─────────────┘                      │    │
│  └────────────────┬──────────────────────────────────────┘    │
└───────────────────┼────────────────────────────────────────────┘
                    │ Native Wire Protocols
                    │ (TCP sockets + protocol framing)
┌───────────────────▼────────────────────────────────────────────┐
│  Remote/Legacy Databases                                        │
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐         │
│  │ PostgreSQL   │  │ MySQL 5.7/8  │  │ MS SQL (post-gold) │    │
│  │ 9.6 - 17.x   │  │ MariaDB 10+  │  │ 2016-2022    │         │
│  └──────────────┘  └──────────────┘  └──────────────┘         │
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐                            │
│  │ Firebird     │  │ ScratchBird  │                            │
│  │ 2.5 - 5.0    │  │ (Federated)  │                            │
│  └──────────────┘  └──────────────┘                            │
└─────────────────────────────────────────────────────────────────┘
```

---

## Design Principles

1. **Reuse Wire Protocol Code**: Server implementations contain all protocol knowledge - reuse it for client mode
2. **Connection Pooling**: Maintain pools of connections to remote databases with health monitoring
3. **Query Pushdown**: Send computation to remote database when efficient (WHERE, ORDER BY, LIMIT, aggregates)
4. **Type Mapping**: Convert remote types to ScratchBird's internal type system (86 types)
5. **Error Handling**: Graceful degradation, retries, connection failover
6. **Security**: TLS support, credential management, connection encryption

---

## Integration with UDR System

This plugin implements the UDR interfaces defined in [10-UDR-System-Specification.md](../udr/10-UDR-System-Specification.md):

```cpp
// Plugin module entry point
extern "C" IPluginModule* pluginEntryPoint() {
    return new RemoteDatabasePluginModule();
}

// Plugin implements these UDR interfaces:
class RemoteDatabasePluginModule : public IPluginModule { ... };
class RemoteDatabasePluginFactory : public IPluginFactory { ... };
class ForeignTableScanner : public IExternalProcedure { ... };
class RemoteQueryExecutor : public IExternalFunction { ... };
```

---

## Quick Start Example

```sql
-- 1. Create a foreign server connection
CREATE SERVER legacy_pg
    FOREIGN DATA WRAPPER postgresql_fdw
    OPTIONS (
        host 'legacy-db.company.com',
        port '5432',
        dbname 'production'
    );

-- 2. Create user mapping for authentication
CREATE USER MAPPING FOR current_user
    SERVER legacy_pg
    OPTIONS (
        user 'migration_user',
        password 'secure_password'
    );

-- 3. Import remote schema (auto-discovers tables)
IMPORT FOREIGN SCHEMA public
    FROM SERVER legacy_pg
    INTO legacy_schema;

-- 4. Query remote data as local
SELECT * FROM legacy_schema.users WHERE created_at > '2024-01-01';

-- 5. Join local and remote tables
SELECT u.*, o.total
FROM local_users u
JOIN legacy_schema.orders o ON u.legacy_id = o.user_id;

-- 6. Migrate data incrementally
INSERT INTO local_users (name, email, created_at)
SELECT name, email, created_at
FROM legacy_schema.users
WHERE migrated = false
LIMIT 10000;
```

---

## Related Specifications

- [10-UDR-System-Specification.md](../udr/10-UDR-System-Specification.md) - UDR plugin interfaces
- [CONNECTION_POOLING_SPECIFICATION.md](../api/CONNECTION_POOLING_SPECIFICATION.md) - Local connection pooling (reference architecture)
- [wire_protocols/](../wire_protocols/) - Wire protocol specifications for each database type
- [00_SECURITY_SPEC_INDEX.md](../Security%20Design%20Specification/00_SECURITY_SPEC_INDEX.md) - Security and authentication

---

## Implementation Status

| Component | Status | Priority |
|-----------|--------|----------|
| Core Types & Interfaces | Planned | P0 |
| Connection Pool | Planned | P0 |
| PostgreSQL Adapter | Planned | P0 |
| MySQL Adapter | Planned | P1 |
| MSSQL Adapter | Planned | P2 |
| Firebird Adapter | Planned | P1 |
| ScratchBird Adapter | Planned | P1 |
| Query Pushdown | Planned | P1 |
| Schema Introspection | Planned | P0 |
| Migration Tools | Planned | P2 |
