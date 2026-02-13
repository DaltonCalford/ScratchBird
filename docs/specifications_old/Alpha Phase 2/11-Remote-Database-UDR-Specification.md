# Remote Database UDR Specification

## Overview

The Remote Database UDR (User Defined Routine) plugin enables your database engine to connect to external databases using native wire protocol client implementations. This transforms your existing wire protocol expertise into a powerful migration and integration tool.

**Scope Note:** MSSQL/TDS adapter support is in Beta scope. All listed connectors
ship as separate UDR packages.

**Strategic Benefits:**
- **Bidirectional Protocol Support**: Your PostgreSQL/MySQL/Firebird/MSSQL server
  code becomes client code (wire-protocol clients).
- **Zero-Downtime Migration**: Migrate data from legacy systems incrementally
- **Foreign Data Wrapper Pattern**: Query remote data transparently
- **Hybrid Query Execution**: JOIN local and remote tables seamlessly
- **Schema Auto-Discovery**: Automatically introspect remote databases

**Implementation Scope:**
- Core specification: This document
- Connection pooling: [11a-Connection-Pool-Implementation.md](11a-Connection-Pool-Implementation.md)
- PostgreSQL client: [11b-PostgreSQL-Client-Implementation.md](11b-PostgreSQL-Client-Implementation.md)
- MySQL client: [11c-MySQL-Client-Implementation.md](11c-MySQL-Client-Implementation.md)
- MSSQL client: [11d-MSSQL-Client-Implementation.md](11d-MSSQL-Client-Implementation.md)
- Firebird client: [11e-Firebird-Client-Implementation.md](11e-Firebird-Client-Implementation.md)
- ODBC connector: [11f-ODBC-Client-Implementation.md](11f-ODBC-Client-Implementation.md)
- JDBC connector: [11g-JDBC-Client-Implementation.md](11g-JDBC-Client-Implementation.md)
- Live migration with emulated listener: [11h-Live-Migration-Emulated-Listener.md](11h-Live-Migration-Emulated-Listener.md)
- ScratchBird connector (untrusted): [11i-ScratchBird-Client-Implementation.md](11i-ScratchBird-Client-Implementation.md)

---

## Connector Packaging and Isolation

- Each protocol connector ships as a **separate UDR module**.
- PostgreSQL/MySQL/Firebird/MSSQL/ScratchBird connectors implement the **native
  wire protocols directly** (no external client libraries).
- ODBC/JDBC connectors include a **bundled driver manager + drivers** inside
  the UDR package to avoid system-level client library installs.
- ScratchBird connector is **untrusted** (non-cluster) and must not negotiate
  `FEATURE_FEDERATION` or use cluster PKI.
- All connectors are subject to the UDR signing/verification policy and
  per‑server allowlists.

## Compatibility Contract (Canonical)
Remote Database UDRs are passthrough connectors. They MUST implement full
protocol flows for the target database so that remote SQL executes with native
semantics on the remote server. ScratchBird does not rewrite or emulate the
remote SQL dialect; it only:
- Establishes authenticated connections
- Sends SQL/parameters and receives results
- Maps remote types to ScratchBird types
- Enforces allow_ddl/allow_dml/allow_psql/allow_passthrough

If a remote feature depends on a specific wire-protocol capability (prepared
statements, cursors, COPY/LOAD, cancellation), the connector must implement
that capability to claim full compatibility.

## Required Protocol Capabilities (per connector)
All connectors must implement, at minimum:
- Startup + authentication (including TLS where supported)
- Simple query execution
- Prepared statements with parameter binding
- Cursor/paging (portal for PostgreSQL, COM_STMT_FETCH for MySQL)
- Cancellation semantics
- Schema introspection
- Error mapping to ScratchBird SQLSTATE

Optional (but recommended for full fidelity):
- COPY/streaming (PostgreSQL)
- Batch execution where supported (Firebird protocol 17+)
- Server notifications/events (PostgreSQL LISTEN/NOTIFY, Firebird events)

## Canonical Implementation Specs
- 11a-Connection-Pool-Implementation.md
- 11b-PostgreSQL-Client-Implementation.md
- 11c-MySQL-Client-Implementation.md
- 11d-MSSQL-Client-Implementation.md
- 11e-Firebird-Client-Implementation.md
- 11f-ODBC-Client-Implementation.md
- 11g-JDBC-Client-Implementation.md
- 11i-ScratchBird-Client-Implementation.md

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Your Database Engine                                   │
│                                                          │
│  ┌────────────────────────────────────────────────┐   │
│  │  Query Planner                                  │   │
│  │  - Recognize foreign tables                     │   │
│  │  - Cost estimation (local vs remote)            │   │
│  │  - Query pushdown analysis                      │   │
│  └────────────┬───────────────────────────────────┘   │
│               │                                         │
│  ┌────────────▼───────────────────────────────────┐   │
│  │  Execution Engine                               │   │
│  │  ┌──────────┐  ┌──────────────────────┐       │   │
│  │  │Local Scan│  │Foreign Scan (UDR)    │       │   │
│  │  └──────────┘  └──────────┬───────────┘       │   │
│  └───────────────────────────┼────────────────────┘   │
└────────────────────────────────┼────────────────────────┘
                                 │ UDR Interface
┌────────────────────────────────▼────────────────────────┐
│  Remote Database UDR Plugin                             │
│  ┌────────────────────────────────────────────────┐    │
│  │  Connection Pool Registry                       │    │
│  │  {                                              │    │
│  │    "legacy_pg": PostgreSQL pool,                │    │
│  │    "old_mysql": MySQL pool,                     │    │
│  │    "archive_fb": Firebird pool                  │    │
│  │  }                                              │    │
│  └────────────┬───────────────────────────────────┘    │
│               │                                         │
│  ┌────────────▼───────────────────────────────────┐    │
│  │  Protocol Adapters                              │    │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐       │    │
│  │  │PostgreSQL│ │  MySQL   │ │ MSSQL   │       │    │
│  │  └──────────┘ └──────────┘ └──────────┘       │    │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐       │    │
│  │  │ Firebird │ │  ODBC    │ │  JDBC    │       │    │
│  │  └──────────┘ └──────────┘ └──────────┘       │    │
│  │  ┌──────────┐                                   │    │
│  │  │ScratchBird│                                  │    │
│  │  └──────────┘                                   │    │
│  └────────────┬───────────────────────────────────┘    │
└───────────────┼──────────────────────────────────────────┘
                │ Wire Protocols
┌───────────────▼──────────────────────────────────────────┐
│  Remote/Legacy Databases                                │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │
│  │PostgreSQL│ │  MySQL   │ │ MSSQL   │ │ Firebird │  │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘  │
│  ┌──────────┐                                       │
│  │ScratchBird│                                       │
│  └──────────┘                                       │
└──────────────────────────────────────────────────────────┘
```

---

## Use Cases

### 1. Data Migration

Migrate from legacy database to new engine without downtime:

```sql
-- Step 1: Register legacy database
REGISTER REMOTE DATABASE legacy_postgres WITH (
    protocol = 'postgresql',
    host = 'old-db.internal',
    port = 5432,
    database = 'production'
);

-- Step 2: Create foreign table
CREATE FOREIGN TABLE users_old
SERVER legacy_postgres
OPTIONS (schema 'public', table 'users');

-- Step 3: Validate data
SELECT COUNT(*) FROM users_old;    -- 1,234,567
SELECT COUNT(*) FROM users_new;    -- 0

-- Step 4: Migrate incrementally
INSERT INTO users_new
SELECT * FROM users_old
WHERE created_at >= '2024-01-01';

-- Step 5: Verify
SELECT 
    COUNT(*) as old_count 
FROM users_old 
WHERE created_at >= '2024-01-01'
UNION ALL
SELECT 
    COUNT(*) as new_count 
FROM users_new;

-- Step 6: Cutover (after full migration)
DROP FOREIGN TABLE users_old;
```

---

## End-to-End Workflow (UDR → Server → User Mapping → Use)

Short lifecycle for connecting and using a remote database:

1. **Install + verify UDR** (admin) — makes the connector available.
2. **Create FDW** — registers handler/validator.
3. **Create SERVER** — defines remote endpoint + policy (allow_ddl/dml/psql/passthrough).
4. **Create USER MAPPING** — multiple credentials per local user/role.
5. **Use** — pass-through queries, foreign tables, or migration.

### Diagram (End-to-End)

```
┌─────────────────────────┐
│ Admin: UDR install       │
│ sb_admin udr install     │
└──────────────┬──────────┘
               │
               ▼
┌─────────────────────────┐
│ SQL: CREATE FDW         │
│ + CREATE SERVER         │
└──────────────┬──────────┘
               │
               ▼
┌─────────────────────────┐
│ SQL: CREATE USER MAPPING│
│ (per local user/role)   │
└──────────────┬──────────┘
               │
               ▼
┌─────────────────────────┐
│ Use cases:              │
│ - sys.remote_*          │
│ - FOREIGN TABLE         │
│ - migration workflow    │
└─────────────────────────┘
```

### Multi-User Mapping Example

```sql
CREATE USER MAPPING FOR analyst
    SERVER legacy_pg
    OPTIONS (user 'readonly_user', password '***');

CREATE USER MAPPING FOR migrator
    SERVER legacy_pg
    OPTIONS (user 'migration_user', password '***');
```

### 2. Hybrid Queries

Query across old and new systems during migration:

```sql
-- Join new orders with legacy customer data
SELECT 
    c.customer_name,
    c.email,
    o.order_total,
    o.order_date
FROM customers_legacy c
JOIN orders_new o ON o.customer_id = c.id
WHERE o.order_date > CURRENT_DATE - INTERVAL '7 days';
```

### 3. Cross-Database Analytics

Combine data from multiple sources:

```sql
-- Register multiple remote databases
REGISTER REMOTE DATABASE warehouse_mysql WITH (
    protocol = 'mysql',
    host = 'warehouse.internal',
    database = 'analytics'
);

REGISTER REMOTE DATABASE archive_firebird WITH (
    protocol = 'firebird',
    host = 'archive.internal',
    database = '/data/historical.fdb'
);

-- Create foreign tables
CREATE FOREIGN TABLE recent_sales
SERVER warehouse_mysql
OPTIONS (table 'sales_2024');

CREATE FOREIGN TABLE archived_sales
SERVER archive_firebird
OPTIONS (table 'SALES_ARCHIVE');

-- Unified query across systems
SELECT 
    'recent' as source,
    SUM(amount) as total_sales
FROM recent_sales
WHERE sale_date >= '2024-01-01'
UNION ALL
SELECT 
    'archive' as source,
    SUM(amount) as total_sales
FROM archived_sales
WHERE sale_date < '2024-01-01';
```

### 4. Continuous Replication

Set up ongoing replication from remote database:

```sql
-- Create replication stream
CREATE REPLICATION STREAM user_sync
FROM legacy_postgres.public.users
TO users_new
WITH (
    mode = 'continuous',
    poll_interval_seconds = 5,
    conflict_resolution = 'last_write_wins',
    batch_size = 1000
);

-- Monitor replication
SELECT * FROM replication_status('user_sync');
```

---

## Core Concepts

### Foreign Table

A **foreign table** is a local table definition that maps to a table in a remote database:

```sql
CREATE FOREIGN TABLE remote_products (
    product_id INTEGER,
    name VARCHAR(200),
    price DECIMAL(10,2)
)
SERVER legacy_mysql
OPTIONS (
    schema_name 'inventory',
    table_name 'products'
);
```

- Local definition can differ from remote (subset of columns, renamed, etc.)
- Metadata is cached locally for performance
- Queries to foreign tables are translated to remote SQL

### Remote Database Registration

Before creating foreign tables, register the remote database:

```sql
REGISTER REMOTE DATABASE <name> WITH (
    protocol = 'postgresql' | 'mysql' | 'mssql' | 'firebird' | 'odbc' | 'jdbc' | 'scratchbird',
    host = 'hostname',
    port = port_number,
    database = 'database_name',
    username = 'user',
    password = 'password',  -- or use password_file
    
    -- Connection pool settings
    pool_min_size = 2,
    pool_max_size = 10,
    connection_timeout_ms = 30000,
    idle_timeout_ms = 300000,
    
    -- SSL/TLS
    use_ssl = true,
    ssl_ca_cert = '/path/to/ca.pem',
    
    -- Query behavior
    enable_query_pushdown = true,
    query_timeout_ms = 60000
);
```

### Query Pushdown

The query planner analyzes queries on foreign tables and determines what can be executed remotely:

**Example 1: Simple pushdown**
```sql
-- User query
SELECT * FROM users_remote WHERE age > 25 ORDER BY name LIMIT 10;

-- Pushed to remote database (efficient)
SELECT * FROM users WHERE age > 25 ORDER BY name LIMIT 10;
```

**Example 2: Partial pushdown**
```sql
-- User query
SELECT * FROM users_remote WHERE age > 25 AND random() < 0.1;

-- Pushed to remote (partial)
SELECT * FROM users WHERE age > 25;

-- Applied locally (random() not on remote)
Filter: random() < 0.1
```

**Example 3: No pushdown**
```sql
-- User query
SELECT 
    u.name,
    COUNT(*) as order_count
FROM users_remote u
JOIN orders_local o ON o.user_id = u.id
GROUP BY u.name;

-- Fetch all from remote
SELECT id, name FROM users;

-- Join and aggregate locally
```

**Pushdown Optimization Rules:**

| Operation | Pushdown? | Notes |
|-----------|-----------|-------|
| WHERE (simple predicates) | ✅ Yes | `col = value`, `col > value`, etc. |
| WHERE (complex functions) | ❌ No | Custom functions not on remote |
| ORDER BY | ✅ Yes | If result set is small |
| LIMIT/OFFSET | ✅ Yes | Always beneficial |
| GROUP BY/Aggregation | ✅ Yes | If no local joins |
| JOIN (remote + remote) | ✅ Yes | Can push entire join |
| JOIN (remote + local) | ❌ No | Must fetch remote data |
| DISTINCT | ✅ Yes | If no other operations prevent it |

### Type Mapping

Each protocol adapter maps remote types to internal types:

**PostgreSQL → Internal:**
```
INTEGER    → INT32
BIGINT     → INT64
DOUBLE     → DOUBLE
VARCHAR(n) → STRING
TIMESTAMP  → TIMESTAMP
BYTEA      → BYTES
```

**MySQL → Internal:**
```
INT        → INT32
BIGINT     → INT64
DOUBLE     → DOUBLE
VARCHAR(n) → STRING
DATETIME   → TIMESTAMP
BLOB       → BYTES
```

**MSSQL → Internal:**
```
INT        → INT32
BIGINT     → INT64
FLOAT      → DOUBLE
NVARCHAR(n)→ STRING
DATETIME2  → TIMESTAMP
VARBINARY  → BYTES
```

**Firebird → Internal:**
```
INTEGER    → INT32
BIGINT     → INT64
DOUBLE     → DOUBLE
VARCHAR(n) → STRING
TIMESTAMP  → TIMESTAMP
BLOB       → BYTES
```

**ODBC/JDBC → Internal:**
```
Use driver metadata (SQL type codes / java.sql.Types) with per-driver overrides.
```

---

## SQL Syntax Reference

### REGISTER REMOTE DATABASE

```sql
REGISTER REMOTE DATABASE remote_db_name WITH (
    -- Required
    protocol = 'postgresql' | 'mysql' | 'mssql' | 'firebird' | 'odbc' | 'jdbc' | 'scratchbird',
    host = 'hostname',
    port = port_number,
    database = 'database_name',
    username = 'username',
    password = 'password',  -- or password_file = '/path/to/file'
    
    -- Optional: Connection Pool
    pool_min_size = 2,
    pool_max_size = 10,
    connection_timeout_ms = 30000,
    idle_timeout_ms = 300000,
    max_lifetime_seconds = 3600,
    
    -- Optional: SSL/TLS
    use_ssl = true,
    ssl_ca_cert = '/path/to/ca.pem',
    ssl_client_cert = '/path/to/client.pem',
    ssl_client_key = '/path/to/client.key',
    ssl_verify_server = true,
    
    -- Optional: Query Behavior
    enable_query_pushdown = true,
    enable_result_cache = true,
    cache_ttl_seconds = 300,
    query_timeout_ms = 60000,
    
    -- Optional: Health Check
    health_check_interval_seconds = 30,
    health_check_query = 'SELECT 1'
);
```

### UNREGISTER REMOTE DATABASE

```sql
UNREGISTER REMOTE DATABASE remote_db_name;
```

### CREATE FOREIGN TABLE

```sql
CREATE FOREIGN TABLE local_table_name (
    col1 TYPE,
    col2 TYPE,
    ...
)
SERVER remote_db_name
OPTIONS (
    schema_name 'remote_schema',  -- Optional, default 'public'
    table_name 'remote_table'     -- Required
);
```

### DROP FOREIGN TABLE

```sql
DROP FOREIGN TABLE local_table_name;
```

### ALTER FOREIGN TABLE

```sql
-- Change remote mapping
ALTER FOREIGN TABLE local_table_name
OPTIONS (SET table_name 'new_remote_table');

-- Add column
ALTER FOREIGN TABLE local_table_name
ADD COLUMN new_col TYPE;
```

### SHOW REMOTE DATABASES

```sql
-- List all registered remote databases
SHOW REMOTE DATABASES;

-- Output:
-- name             | protocol    | host              | database
-- -----------------|-------------|-------------------|------------
-- legacy_postgres  | postgresql  | old-db.internal   | production
-- archive_mysql    | mysql       | archive.internal  | archive_db
```

### SHOW FOREIGN TABLES

```sql
-- List all foreign tables
SHOW FOREIGN TABLES;

-- Output:
-- table_name    | server          | remote_schema | remote_table
-- --------------|-----------------|---------------|-------------
-- users_remote  | legacy_postgres | public        | users
-- orders_remote | archive_mysql   | sales         | orders
```

---

## Required Metadata Coverage (Introspection)

Each connector must be able to analyze the remote database and return the
minimum metadata needed to mount schemas and generate compatible local
definitions. The following categories are required for all connectors (where
the remote engine supports them):

- Databases/catalogs, schemas, and tables
- Columns (type, nullability, default, identity/sequence info)
- Primary keys and unique constraints
- Foreign keys
- Indexes and index expressions
- Views (including definitions, when available)
- Stored procedures/functions (names, params, return types)
- Triggers (name, timing, event)
- Sequences/identity sources

When a concept does not exist on the remote engine, report it as `not_supported`
and continue discovery.

---

## Schema Mounting and Namespace Mapping

Remote schemas are mounted into a dedicated namespace so local objects are not
polluted and permissions can be scoped cleanly.

Default mount rules:
- `legacy_<server>`: imported foreign schemas (read-only by default)
- `emulated_<server>`: translated local copies for migration/cutover

Optional overrides (server options):
- `mount_root`: override `legacy_<server>` schema name
- `emulated_root`: override `emulated_<server>` schema name
- `schema_prefix`: prefix applied to imported schemas

Engine-specific mapping rules:
- PostgreSQL: database -> server, schema -> schema.
- MySQL/MariaDB: database -> schema (no separate schema layer).
- MSSQL: database -> catalog, schema -> schema (default `dbo`).
- Firebird: no schema layer; map all objects under a single schema (default
  `public`) inside `legacy_<server>`.

---

## Migration Workflow

Live migration is optional and must be explicitly enabled per remote server.
If disabled, the connector operates in read-only discovery or pass-through
mode only.

### Complete Migration Process

```sql
-- ============================================================
-- Phase 1: Setup and Validation
-- ============================================================

-- 1.1: Register legacy database
REGISTER REMOTE DATABASE legacy_db WITH (
    protocol = 'postgresql',
    host = 'legacy.internal',
    port = 5432,
    database = 'production',
    username = 'migration_user',
    password = 'secure_password',
    pool_max_size = 10
);

-- 1.2: Create foreign tables for all tables to migrate
CREATE FOREIGN TABLE users_legacy
SERVER legacy_db
OPTIONS (schema 'public', table 'users');

CREATE FOREIGN TABLE orders_legacy
SERVER legacy_db
OPTIONS (schema 'public', table 'orders');

CREATE FOREIGN TABLE products_legacy
SERVER legacy_db
OPTIONS (schema 'public', table 'products');

-- 1.3: Create corresponding local tables
CREATE TABLE users_new (
    id INTEGER PRIMARY KEY,
    email VARCHAR(255) NOT NULL,
    name VARCHAR(200),
    created_at TIMESTAMP
);

CREATE TABLE orders_new (
    id INTEGER PRIMARY KEY,
    user_id INTEGER REFERENCES users_new(id),
    total DECIMAL(10,2),
    order_date TIMESTAMP
);

-- 1.4: Validate schemas match
SELECT column_name, data_type 
FROM information_schema.columns 
WHERE table_name = 'users_legacy';

-- 1.5: Get row counts
SELECT 
    'users_legacy' as table_name,
    COUNT(*) as row_count 
FROM users_legacy
UNION ALL
SELECT 
    'orders_legacy',
    COUNT(*) 
FROM orders_legacy;

-- ============================================================
-- Phase 2: Initial Data Migration
-- ============================================================

-- 2.1: Migrate users (all historical data)
INSERT INTO users_new
SELECT * FROM users_legacy;

-- Verify
SELECT 
    (SELECT COUNT(*) FROM users_legacy) as legacy_count,
    (SELECT COUNT(*) FROM users_new) as new_count;

-- 2.2: Migrate orders (in batches for large tables)
INSERT INTO orders_new
SELECT * FROM orders_legacy
WHERE order_date < '2024-01-01';  -- Historical batch 1

INSERT INTO orders_new
SELECT * FROM orders_legacy
WHERE order_date >= '2024-01-01' 
  AND order_date < '2024-06-01';  -- Historical batch 2

INSERT INTO orders_new
SELECT * FROM orders_legacy
WHERE order_date >= '2024-06-01';  -- Recent data

-- ============================================================
-- Phase 3: Delta Sync (catch up recent changes)
-- ============================================================

-- 3.1: Set up replication for ongoing changes
CREATE REPLICATION STREAM users_sync
FROM legacy_db.public.users
TO users_new
WITH (
    mode = 'continuous',
    poll_interval_seconds = 5,
    watermark_column = 'updated_at',
    conflict_resolution = 'last_write_wins'
);

-- 3.2: Monitor replication lag
SELECT 
    stream_name,
    last_sync_time,
    records_synced,
    lag_seconds
FROM replication_status;

-- ============================================================
-- Phase 4: Validation
-- ============================================================

-- 4.1: Row count validation
SELECT 
    'users' as table_name,
    (SELECT COUNT(*) FROM users_legacy) as legacy,
    (SELECT COUNT(*) FROM users_new) as new,
    (SELECT COUNT(*) FROM users_legacy) - 
    (SELECT COUNT(*) FROM users_new) as difference;

-- 4.2: Checksum validation (sample)
SELECT 
    'users_legacy' as source,
    COUNT(*) as count,
    SUM(CAST(id AS BIGINT)) as id_sum,
    MD5(STRING_AGG(email, ',' ORDER BY id)) as email_hash
FROM users_legacy
UNION ALL
SELECT 
    'users_new' as source,
    COUNT(*),
    SUM(CAST(id AS BIGINT)),
    MD5(STRING_AGG(email, ',' ORDER BY id))
FROM users_new;

-- 4.3: Test queries on new system
SELECT * FROM users_new WHERE email LIKE '%@example.com%';
SELECT COUNT(*) FROM orders_new WHERE order_date > CURRENT_DATE - 30;

-- ============================================================
-- Phase 5: Cutover
-- ============================================================

-- 5.1: Stop writes to legacy database (application maintenance window)

-- 5.2: Final sync
SYNC REPLICATION STREAM users_sync;
-- Wait for lag_seconds = 0

-- 5.3: Stop replication
DROP REPLICATION STREAM users_sync;

-- 5.4: Final validation
-- (Repeat row count and checksum checks)

-- 5.5: Switch application to new database
-- (Update application configuration)

-- 5.6: Drop foreign tables
DROP FOREIGN TABLE users_legacy;
DROP FOREIGN TABLE orders_legacy;

-- 5.7: Rename tables (optional)
ALTER TABLE users_new RENAME TO users;
ALTER TABLE orders_new RENAME TO orders;

-- 5.8: Keep remote database registered temporarily for rollback
-- UNREGISTER REMOTE DATABASE legacy_db;  -- Do this after 1-2 weeks

-- ============================================================
-- Phase 6: Monitoring and Rollback Plan
-- ============================================================

-- 6.1: Monitor new system performance
SELECT 
    query_type,
    AVG(execution_time_ms) as avg_time,
    COUNT(*) as query_count
FROM query_statistics
WHERE timestamp > CURRENT_TIMESTAMP - INTERVAL '1 hour'
GROUP BY query_type;

-- 6.2: If rollback needed
-- Re-register remote database
-- Switch application back to legacy
-- Investigate issues
-- Plan re-migration
```

---

## UDR Plugin Implementation

The Remote Database UDR is implemented as a standard UDR plugin. See [10-UDR-System-Specification.md](10-UDR-System-Specification.md) for the UDR framework.

### Plugin Structure

Each connector is a separate UDR module, with shared code compiled into a
common library or copied as needed.

```
udr_connectors/
├── common/
│   ├── connection_pool.c         # Connection pooling (see 11a)
│   ├── query_executor.c          # Query execution layer
│   ├── schema_introspection.c    # Schema discovery
│   └── type_mapping.c            # Type conversion
├── postgresql_udr/
│   ├── plugin.c                  # UDR entry point
│   └── protocol_client.c         # PostgreSQL client (see 11b)
├── mysql_udr/
│   ├── plugin.c
│   └── protocol_client.c         # MySQL client (see 11c)
├── mssql_udr/
│   ├── plugin.c
│   └── protocol_client.c         # MSSQL client (see 11d)
├── firebird_udr/
│   ├── plugin.c
│   └── protocol_client.c         # Firebird client (see 11e)
├── odbc_udr/
│   ├── plugin.c
│   └── odbc_manager.c            # Embedded ODBC stack (see 11f)
└── jdbc_udr/
    ├── plugin.c
    └── jdbc_runtime.c            # Embedded JDBC stack (see 11g)
```

### UDR Functions Exposed

**REMOTE_EXECUTE_QUERY**
```sql
SELECT REMOTE_EXECUTE_QUERY(
    'legacy_postgres',  -- remote database name
    'SELECT * FROM users WHERE id > $1',
    1000  -- parameter
);
```

**REMOTE_LIST_TABLES**
```sql
SELECT * FROM REMOTE_LIST_TABLES('legacy_postgres', 'public');
-- Returns: (schema, table_name, row_count, size_bytes)
```

**REMOTE_GET_TABLE_SCHEMA**
```sql
SELECT * FROM REMOTE_GET_TABLE_SCHEMA(
    'legacy_postgres',
    'public',
    'users'
);
-- Returns: (column_name, data_type, nullable, is_primary_key)
```

---

## Performance Considerations

### Connection Pooling

- Maintain pool of connections to each remote database
- Reuse connections across queries
- Health checks to remove stale connections
- Configurable min/max pool size

**Recommended Settings:**
- Min connections: 2-5 (always warm)
- Max connections: 10-20 (prevent overwhelming remote DB)
- Connection timeout: 30 seconds
- Idle timeout: 5 minutes
- Max lifetime: 1 hour (recreate periodically)

### Result Caching

Cache frequently accessed remote data locally:

```sql
REGISTER REMOTE DATABASE legacy_db WITH (
    ...
    enable_result_cache = true,
    cache_ttl_seconds = 300  -- 5 minutes
);
```

**Cache Behavior:**
- LRU eviction policy
- Per-query cache keys (SQL text + parameters)
- Automatic invalidation after TTL
- Disabled for non-SELECT queries

### Query Pushdown Optimization

The query planner estimates costs:

```
Cost_local_execution = 
    Cost_fetch_all_rows + 
    Cost_filter_locally + 
    Cost_sort_locally

Cost_remote_execution = 
    Cost_network_round_trip + 
    Cost_remote_query_execution

Choose: MIN(Cost_local_execution, Cost_remote_execution)
```

**Factors Considered:**
- Network latency to remote database
- Selectivity of WHERE clauses (estimated row reduction)
- Remote database capabilities (indexes, query optimizer)
- Data volume (rows × row size)

### Batch Operations

For large data migrations, use batching:

```sql
-- Bad: Single large insert (OOM risk)
INSERT INTO users_new SELECT * FROM users_legacy;

-- Good: Batched migration
INSERT INTO users_new
SELECT * FROM users_legacy
WHERE id >= 0 AND id < 100000;

INSERT INTO users_new
SELECT * FROM users_legacy
WHERE id >= 100000 AND id < 200000;

-- ... continue in batches
```

---

## Security

### Credential Management

**Never hardcode passwords:**

```sql
-- Bad
REGISTER REMOTE DATABASE legacy_db WITH (
    password = 'plaintext_password'  -- BAD!
);

-- Good: Use password file
REGISTER REMOTE DATABASE legacy_db WITH (
    password_file = '/secure/passwords/legacy_db.txt'
);
```

**Password file format:**
```
# /secure/passwords/legacy_db.txt
# Permissions: 0600, owner: dbengine
MySecurePassword123!
```

**Or use environment variable:**
```sql
REGISTER REMOTE DATABASE legacy_db WITH (
    password = '$LEGACY_DB_PASSWORD'  -- Expanded from environment
);
```

### SSL/TLS Encryption

Always use encrypted connections for production:

```sql
REGISTER REMOTE DATABASE legacy_db WITH (
    protocol = 'postgresql',
    host = 'legacy.internal',
    port = 5432,
    
    -- SSL/TLS
    use_ssl = true,
    ssl_ca_cert = '/etc/ssl/certs/ca-bundle.crt',
    ssl_client_cert = '/etc/ssl/certs/client.crt',
    ssl_client_key = '/etc/ssl/private/client.key',
    ssl_verify_server = true  -- Verify server certificate
);
```

### Least Privilege

Grant minimum necessary permissions on remote database:

```sql
-- On remote PostgreSQL database
CREATE USER migration_user WITH PASSWORD 'secure_password';
GRANT CONNECT ON DATABASE production TO migration_user;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO migration_user;
-- No INSERT/UPDATE/DELETE during read-only migration
```

### Network Security

- Use VPN or private network between databases
- Firewall rules limiting access to specific IP addresses
- Monitor connection attempts and query patterns
- Set query timeout to prevent runaway queries

---

## Monitoring and Diagnostics

### Connection Pool Metrics

```sql
-- View pool statistics
SELECT 
    pool_name,
    active_connections,
    idle_connections,
    total_connections_created,
    total_acquires,
    total_timeouts,
    total_errors
FROM remote_database_pool_stats;
```

### Query Performance

```sql
-- Track remote query performance
SELECT 
    remote_database,
    query_hash,
    query_text,
    avg_execution_time_ms,
    total_executions,
    cache_hit_ratio
FROM remote_query_stats
ORDER BY avg_execution_time_ms DESC
LIMIT 20;
```

### Error Tracking

```sql
-- Recent remote database errors
SELECT 
    timestamp,
    remote_database,
    error_code,
    error_message,
    query_text
FROM remote_database_errors
WHERE timestamp > CURRENT_TIMESTAMP - INTERVAL '1 hour'
ORDER BY timestamp DESC;
```

---

## Troubleshooting

### Common Issues

**1. Connection Timeout**
```
Error: Failed to acquire connection from pool 'legacy_db' (timeout after 30000ms)
```

**Solution:**
- Increase pool_max_size
- Increase connection_timeout_ms
- Check network connectivity
- Verify remote database is responsive

**2. Query Timeout**
```
Error: Remote query timeout after 60000ms
```

**Solution:**
- Increase query_timeout_ms
- Add indexes on remote database
- Use query pushdown (EXPLAIN to verify)
- Batch large queries

**3. Type Mismatch**
```
Error: Cannot convert remote type 'DECIMAL(20,4)' to internal type 'DOUBLE'
```

**Solution:**
- Use explicit CAST in foreign table definition
- Check type mapping in protocol adapter
- File bug report if unsupported type

**4. SSL Certificate Error**
```
Error: SSL certificate verification failed
```

**Solution:**
- Verify ssl_ca_cert path is correct
- Ensure server certificate is valid and not expired
- Set ssl_verify_server = false for testing (not production!)

---

## Testing

### Unit Tests

Test each protocol adapter independently:

```bash
# Run PostgreSQL adapter tests
./test_postgresql_adapter

# Run MySQL adapter tests
./test_mysql_adapter

# Run connection pool tests
./test_connection_pool
```

### Integration Tests

Test end-to-end scenarios:

```sql
-- Test basic query
SELECT * FROM foreign_table LIMIT 10;

-- Test pushdown
EXPLAIN SELECT * FROM foreign_table WHERE id > 1000;
-- Verify "Remote scan with pushdown: WHERE id > 1000"

-- Test join
SELECT * FROM foreign_table f
JOIN local_table l ON l.id = f.id;

-- Test aggregation
SELECT COUNT(*), AVG(price) FROM foreign_table;
```

### Performance Benchmarks

```bash
# Benchmark query execution
./benchmark_remote_query \
    --database legacy_postgres \
    --query "SELECT * FROM large_table WHERE date > '2024-01-01'" \
    --iterations 100

# Benchmark data migration
./benchmark_migration \
    --source legacy_postgres.users \
    --dest users_new \
    --batch-size 10000
```

---

## Future Enhancements

1. **Write Support**: Currently read-only; add INSERT/UPDATE/DELETE support
2. **Two-Phase Commit**: Distributed transactions across local and remote databases
3. **Asynchronous Queries**: Non-blocking remote query execution
4. **Query Result Streaming**: Stream large result sets instead of buffering
5. **Smart Cache Invalidation**: Invalidate cache on remote data changes
6. **Connection Multiplexing**: Share connections for multiple concurrent queries
7. **Oracle Support**: Add Oracle client adapter
8. **DB2 Support**: Add DB2 client adapter

---

## Implementation Checklist

- [ ] Implement connection pool (see 11a)
- [ ] Implement PostgreSQL adapter (see 11b)
- [ ] Implement MySQL adapter (see 11c)
- [ ] Implement MSSQL adapter (Beta; see 11d)
- [ ] Implement Firebird adapter (see 11e)
- [ ] Implement ODBC connector (see 11f)
- [ ] Implement JDBC connector (see 11g)
- [ ] Implement query executor
- [ ] Implement schema introspection
- [ ] Implement type mapping
- [ ] Implement UDR plugin entry point
- [ ] Write unit tests for each component
- [ ] Write integration tests
- [ ] Write performance benchmarks
- [ ] Write documentation and examples
- [ ] Security audit
- [ ] Performance optimization

---

## References

- **UDR Framework**: [10-UDR-System-Specification.md](10-UDR-System-Specification.md)
- **Connection Pooling**: [11a-Connection-Pool-Implementation.md](11a-Connection-Pool-Implementation.md)
- **PostgreSQL Client**: [11b-PostgreSQL-Client-Implementation.md](11b-PostgreSQL-Client-Implementation.md)
- **MySQL Client**: [11c-MySQL-Client-Implementation.md](11c-MySQL-Client-Implementation.md)
- **MSSQL Client (Beta)**: [11d-MSSQL-Client-Implementation.md](11d-MSSQL-Client-Implementation.md)
- **Firebird Client**: [11e-Firebird-Client-Implementation.md](11e-Firebird-Client-Implementation.md)

---

**Document Status**: Core Specification Complete  
**Last Updated**: November 2025  
**Next Steps**: Implement protocol adapters per referenced documents
