# Remote Database UDR - SQL Syntax

> Reference-only: Canonical UDR and live-migration behavior now lives in
> `ScratchBird/docs/specifications/Alpha Phase 2/11-Remote-Database-UDR-Specification.md`
> and `ScratchBird/docs/specifications/Alpha Phase 2/11h-Live-Migration-Emulated-Listener.md`.

**Scope Note:** MSSQL/TDS adapter support is post-gold; MSSQL references are forward-looking.

## 1. Foreign Server Management

### 1.1 CREATE SERVER

```sql
CREATE SERVER server_name
    [TYPE 'database_type']
    FOREIGN DATA WRAPPER wrapper_name
    OPTIONS (option_name 'value' [, ...]);
```

**Parameters:**
- `server_name`: Unique identifier for the foreign server
- `TYPE`: Optional explicit type (`postgresql`, `mysql`, `mssql`, `firebird`, `scratchbird`)
- `wrapper_name`: FDW type (`postgresql_fdw`, `mysql_fdw`, `mssql_fdw`, `firebird_fdw`, `scratchbird_fdw`)

**Required Options:**
| Option | Description |
|--------|-------------|
| `host` | Hostname or IP address |
| `port` | Port number |
| `dbname` | Database name |

**Optional Options:**
| Option | Default | Description |
|--------|---------|-------------|
| `min_connections` | 1 | Minimum pool size |
| `max_connections` | 10 | Maximum pool size |
| `connect_timeout` | 5000 | Connection timeout (ms) |
| `query_timeout` | 30000 | Query timeout (ms) |
| `idle_timeout` | 300000 | Idle connection timeout (ms) |
| `use_ssl` | false | Enable TLS |
| `ssl_mode` | prefer | disable/allow/prefer/require/verify-ca/verify-full |
| `ssl_ca_cert` | - | CA certificate path |
| `ssl_client_cert` | - | Client certificate path |
| `ssl_client_key` | - | Client key path |

**Examples:**

```sql
-- PostgreSQL server
CREATE SERVER legacy_pg
    FOREIGN DATA WRAPPER postgresql_fdw
    OPTIONS (
        host 'legacy-db.company.com',
        port '5432',
        dbname 'production',
        use_ssl 'true',
        ssl_mode 'verify-full',
        max_connections '20'
    );

-- MySQL server
CREATE SERVER prod_mysql
    FOREIGN DATA WRAPPER mysql_fdw
    OPTIONS (
        host 'mysql.company.com',
        port '3306',
        dbname 'orders',
        connect_timeout '10000'
    );

-- SQL Server (post-gold)
CREATE SERVER analytics_mssql
    FOREIGN DATA WRAPPER mssql_fdw
    OPTIONS (
        host 'sqlserver.company.com',
        port '1433',
        dbname 'analytics',
        use_ssl 'true'
    );

-- Firebird server
CREATE SERVER legacy_firebird
    FOREIGN DATA WRAPPER firebird_fdw
    OPTIONS (
        host 'fb.company.com',
        port '3050',
        dbname '/var/firebird/legacy.fdb'
    );

-- Another ScratchBird instance (federation)
CREATE SERVER reporting_db
    FOREIGN DATA WRAPPER scratchbird_fdw
    OPTIONS (
        host 'reporting.company.com',
        port '3092',
        dbname 'reports',
        use_cluster_pki 'true'
    );
```

### 1.2 ALTER SERVER

```sql
ALTER SERVER server_name
    [VERSION 'new_version']
    [OPTIONS (SET option_name 'value' [, ...])]
    [OPTIONS (ADD option_name 'value' [, ...])]
    [OPTIONS (DROP option_name [, ...])];
```

**Examples:**

```sql
-- Change connection pool settings
ALTER SERVER legacy_pg
    OPTIONS (SET max_connections '30', SET idle_timeout '600000');

-- Add SSL configuration
ALTER SERVER legacy_pg
    OPTIONS (ADD ssl_ca_cert '/etc/ssl/certs/company-ca.pem');

-- Remove option
ALTER SERVER legacy_pg
    OPTIONS (DROP connect_timeout);
```

### 1.3 DROP SERVER

```sql
DROP SERVER [IF EXISTS] server_name [CASCADE | RESTRICT];
```

- `CASCADE`: Drop dependent user mappings and foreign tables
- `RESTRICT`: Error if dependencies exist (default)

---

## 2. User Mapping

### 2.1 CREATE USER MAPPING

```sql
CREATE USER MAPPING FOR { user_name | PUBLIC | CURRENT_USER }
    SERVER server_name
    OPTIONS (option_name 'value' [, ...]);
```

**Options:**
| Option | Description |
|--------|-------------|
| `user` | Remote username |
| `password` | Remote password |
| `auth_type` | Authentication type (password, kerberos, certificate) |
| `kerberos_principal` | Kerberos principal name |
| `client_certificate` | Path to client certificate |

**Examples:**

```sql
-- Basic password authentication
CREATE USER MAPPING FOR current_user
    SERVER legacy_pg
    OPTIONS (
        user 'migration_user',
        password 'secure_password'
    );

-- Default mapping for all users
CREATE USER MAPPING FOR PUBLIC
    SERVER legacy_pg
    OPTIONS (
        user 'readonly_user',
        password 'read_only_pw'
    );

-- Kerberos authentication
CREATE USER MAPPING FOR admin_user
    SERVER prod_mysql
    OPTIONS (
        auth_type 'kerberos',
        kerberos_principal 'admin@COMPANY.COM'
    );
```

### 2.2 ALTER USER MAPPING

```sql
ALTER USER MAPPING FOR { user_name | PUBLIC | CURRENT_USER }
    SERVER server_name
    OPTIONS (SET option_name 'value' [, ...]);
```

### 2.3 DROP USER MAPPING

```sql
DROP USER MAPPING [IF EXISTS] FOR { user_name | PUBLIC | CURRENT_USER }
    SERVER server_name;
```

---

## 3. Foreign Tables

### 3.1 CREATE FOREIGN TABLE

```sql
CREATE FOREIGN TABLE [IF NOT EXISTS] table_name (
    column_name data_type [OPTIONS (option 'value' [, ...])] [column_constraint [, ...]]
    [, ...]
)
    SERVER server_name
    OPTIONS (option_name 'value' [, ...]);
```

**Table Options:**
| Option | Description |
|--------|-------------|
| `schema_name` | Remote schema name |
| `table_name` | Remote table name |
| `updatable` | Allow INSERT/UPDATE/DELETE (true/false) |
| `estimated_rows` | Estimated row count for planner |
| `startup_cost` | Cost estimate for connection overhead |
| `fetch_size` | Rows to fetch per round-trip |

**Column Options:**
| Option | Description |
|--------|-------------|
| `column_name` | Remote column name (if different) |
| `type_name` | Remote type hint |

**Examples:**

```sql
-- Basic foreign table
CREATE FOREIGN TABLE legacy_users (
    id INTEGER NOT NULL,
    username VARCHAR(255) NOT NULL,
    email VARCHAR(255),
    created_at TIMESTAMP NOT NULL,
    status VARCHAR(20)
)
SERVER legacy_pg
OPTIONS (
    schema_name 'public',
    table_name 'users'
);

-- With column name mapping
CREATE FOREIGN TABLE customer_orders (
    order_id BIGINT OPTIONS (column_name 'OrderID'),
    customer_id INTEGER OPTIONS (column_name 'CustomerID'),
    order_total DECIMAL(10,2) OPTIONS (column_name 'TotalAmount'),
    order_date DATE OPTIONS (column_name 'OrderDate')
)
SERVER analytics_mssql
OPTIONS (
    schema_name 'dbo',
    table_name 'Orders',
    updatable 'false',
    fetch_size '5000'
);

-- Updatable foreign table
CREATE FOREIGN TABLE remote_products (
    product_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    price DECIMAL(10,2),
    stock_qty INTEGER DEFAULT 0
)
SERVER prod_mysql
OPTIONS (
    schema_name 'inventory',
    table_name 'products',
    updatable 'true'
);
```

### 3.2 ALTER FOREIGN TABLE

```sql
-- Add column
ALTER FOREIGN TABLE table_name
    ADD COLUMN column_name data_type [OPTIONS (...)];

-- Drop column
ALTER FOREIGN TABLE table_name
    DROP COLUMN column_name;

-- Change options
ALTER FOREIGN TABLE table_name
    OPTIONS (SET option_name 'value');

-- Rename
ALTER FOREIGN TABLE table_name
    RENAME TO new_name;
```

### 3.3 DROP FOREIGN TABLE

```sql
DROP FOREIGN TABLE [IF EXISTS] table_name [CASCADE | RESTRICT];
```

---

## 4. Schema Import

### 4.1 IMPORT FOREIGN SCHEMA

```sql
IMPORT FOREIGN SCHEMA remote_schema
    [ { LIMIT TO | EXCEPT } ( table_name [, ...] ) ]
    FROM SERVER server_name
    INTO local_schema
    [ OPTIONS (option_name 'value' [, ...]) ];
```

**Options:**
| Option | Default | Description |
|--------|---------|-------------|
| `import_views` | true | Import views as foreign tables |
| `import_collation` | false | Import column collations |
| `import_not_null` | true | Import NOT NULL constraints |
| `import_default` | false | Import default values |
| `table_prefix` | - | Prefix for local table names |
| `table_suffix` | - | Suffix for local table names |
| `updatable` | true | Allow DML on imported tables |

**Examples:**

```sql
-- Import all tables from a schema
IMPORT FOREIGN SCHEMA public
    FROM SERVER legacy_pg
    INTO legacy_schema;

-- Import specific tables
IMPORT FOREIGN SCHEMA public
    LIMIT TO (users, orders, products)
    FROM SERVER legacy_pg
    INTO legacy_schema;

-- Exclude certain tables
IMPORT FOREIGN SCHEMA public
    EXCEPT (audit_log, temp_tables)
    FROM SERVER legacy_pg
    INTO legacy_schema;

-- With options
IMPORT FOREIGN SCHEMA inventory
    FROM SERVER prod_mysql
    INTO local_inventory
    OPTIONS (
        import_views 'true',
        table_prefix 'mysql_',
        updatable 'false'
    );
```

---

## 5. Querying Foreign Tables

### 5.1 Basic SELECT

```sql
-- Simple query (WHERE, ORDER BY, LIMIT pushed to remote)
SELECT * FROM legacy_users
WHERE status = 'active'
ORDER BY created_at DESC
LIMIT 100;

-- Aggregate (pushed to remote if supported)
SELECT COUNT(*) AS total, status
FROM legacy_users
GROUP BY status;

-- Join with local table
SELECT l.name, l.email, o.total
FROM local_customers l
JOIN legacy_orders o ON l.legacy_id = o.customer_id;

-- Join between foreign tables (same server - pushed remotely)
SELECT u.username, o.total
FROM legacy_users u
JOIN legacy_orders o ON u.id = o.user_id
WHERE o.order_date > '2024-01-01';

-- Cross-server join (executed locally)
SELECT pg.username, my.order_total
FROM legacy_pg_users pg
JOIN mysql_orders my ON pg.id = my.customer_id;
```

### 5.2 DML Operations

```sql
-- INSERT
INSERT INTO remote_products (product_id, name, price, stock_qty)
VALUES (1001, 'New Product', 29.99, 100);

-- Bulk INSERT
INSERT INTO remote_products (product_id, name, price, stock_qty)
SELECT id, name, price, quantity
FROM local_new_products
WHERE approved = true;

-- UPDATE
UPDATE remote_products
SET price = price * 1.10
WHERE category = 'electronics';

-- DELETE
DELETE FROM remote_products
WHERE stock_qty = 0 AND discontinued = true;
```

### 5.3 Query Hints

```sql
-- Force local execution (no pushdown)
SELECT /*+ NO_PUSHDOWN */ * FROM legacy_users WHERE id = 1;

-- Force specific fetch size
SELECT /*+ FETCH_SIZE(10000) */ * FROM large_remote_table;

-- Use cursor for very large results
SELECT /*+ USE_CURSOR */ * FROM very_large_table;

-- Set query timeout
SELECT /*+ TIMEOUT(60000) */ * FROM slow_remote_query;
```

### 5.4 Pass-through Execution (sys.*)

Pass-through routines execute raw SQL or stored routines on the remote server.
These require `allow_passthrough = true` on the server definition.

```sql
-- Execute DDL/DML directly on the remote server
CALL sys.remote_exec('legacy_pg', 'CREATE TABLE tmp(id int)');
CALL sys.remote_exec('legacy_pg', 'INSERT INTO tmp(id) VALUES (1)');

-- Query remote data directly
SELECT * FROM sys.remote_query('legacy_pg', 'SELECT id, name FROM users')
AS t(id INT, name TEXT);

-- Call a remote stored routine
CALL sys.remote_call('legacy_pg', 'refresh_materialized_view', '{"name":"mv"}');
```

---

## 6. Administrative Commands

### 6.1 Server Status

```sql
-- List all foreign servers
SELECT * FROM sys.foreign_servers;

-- Check server health
SELECT * FROM sys.remote_server_status;

-- View pool statistics
SELECT * FROM sys.remote_pool_stats;

-- View active connections
SELECT * FROM sys.remote_connections
WHERE server_name = 'legacy_pg';
```

### 6.2 Connection Pool Management

```sql
-- Warm up connection pool
CALL sys.warmup_pool('legacy_pg', 5);

-- Evict idle connections
CALL sys.evict_idle_connections('legacy_pg');

-- Close all connections to a server
CALL sys.close_all_connections('legacy_pg');

-- Reset a specific connection
CALL sys.reset_connection('legacy_pg', connection_id);
```

### 6.3 Schema Refresh

```sql
-- Refresh schema metadata (check for remote changes)
SELECT * FROM sys.refresh_foreign_schema('legacy_schema', 'legacy_pg');

-- The result shows:
-- | change_type | object_name | details                    |
-- |-------------|-------------|----------------------------|
-- | NEW_TABLE   | orders      | New table detected         |
-- | NEW_COLUMN  | users.phone | New column added           |
-- | TYPE_CHANGE | users.age   | SMALLINT -> INTEGER        |
-- | DROPPED     | temp_data   | Table no longer exists     |
```

---

## 7. System Views

### 7.1 sys.foreign_servers

| Column | Type | Description |
|--------|------|-------------|
| server_id | BIGINT | Unique server ID |
| server_name | VARCHAR | Server name |
| server_type | VARCHAR | Database type |
| wrapper_name | VARCHAR | FDW name |
| host | VARCHAR | Remote host |
| port | INTEGER | Remote port |
| dbname | VARCHAR | Remote database |
| owner | VARCHAR | Server owner |
| options | JSON | Server options |

### 7.2 sys.foreign_tables

| Column | Type | Description |
|--------|------|-------------|
| table_id | BIGINT | Unique table ID |
| schema_name | VARCHAR | Local schema |
| table_name | VARCHAR | Local table name |
| server_name | VARCHAR | Foreign server |
| remote_schema | VARCHAR | Remote schema |
| remote_table | VARCHAR | Remote table name |
| updatable | BOOLEAN | DML allowed |
| columns | JSON | Column definitions |

### 7.3 sys.user_mappings

| Column | Type | Description |
|--------|------|-------------|
| mapping_id | BIGINT | Unique mapping ID |
| local_user | VARCHAR | Local username |
| server_name | VARCHAR | Foreign server |
| remote_user | VARCHAR | Remote username |
| auth_type | VARCHAR | Authentication method |

### 7.4 sys.remote_pool_stats

| Column | Type | Description |
|--------|------|-------------|
| server_name | VARCHAR | Server name |
| total_connections | INTEGER | Total pool size |
| active_connections | INTEGER | In-use connections |
| idle_connections | INTEGER | Available connections |
| waiting_requests | INTEGER | Pending acquires |
| total_acquires | BIGINT | Lifetime acquires |
| acquire_timeouts | BIGINT | Timeout count |
| avg_acquire_time_ms | DOUBLE | Average wait time |

### 7.5 sys.remote_server_status

| Column | Type | Description |
|--------|------|-------------|
| server_name | VARCHAR | Server name |
| is_healthy | BOOLEAN | Health status |
| last_check | TIMESTAMP | Last health check |
| consecutive_failures | INTEGER | Failure count |
| error_message | VARCHAR | Last error |
| queries_executed | BIGINT | Total queries |
| avg_query_time_ms | DOUBLE | Average query time |

---

## 8. Error Messages

### 8.1 Common Errors

| SQLSTATE | Message | Description |
|----------|---------|-------------|
| RD001 | Connection failed | Cannot connect to remote server |
| RD002 | Authentication failed | Invalid credentials |
| RD003 | SSL error | TLS handshake failed |
| RD004 | Timeout | Connection or query timeout |
| RD005 | Pool exhausted | No available connections |
| RD010 | Remote syntax error | Invalid SQL on remote |
| RD011 | Permission denied | Access denied on remote |
| RD012 | Object not found | Table/column doesn't exist |
| RD030 | Server not found | Unknown foreign server |
| RD031 | No user mapping | Missing user mapping |

### 8.2 Error Handling Example

```sql
DO $$
BEGIN
    INSERT INTO remote_products (id, name, price)
    VALUES (1, 'Test', 9.99);
EXCEPTION
    WHEN SQLSTATE 'RD004' THEN
        RAISE NOTICE 'Remote server timeout - retrying...';
        -- Retry logic here
    WHEN SQLSTATE 'RD005' THEN
        RAISE NOTICE 'Connection pool exhausted - waiting...';
        PERFORM pg_sleep(1);
        -- Retry logic here
    WHEN OTHERS THEN
        RAISE EXCEPTION 'Remote operation failed: %', SQLERRM;
END;
$$;
```
