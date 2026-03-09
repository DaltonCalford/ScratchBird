# Foreign Data Wrappers (FDW)

[External and Integration README](../README.md) | [DDL README](../../README.md)

## Synopsis

Foreign Data Wrappers (FDW) allow ScratchBird to access data in external databases or systems as if they were local tables.

## Architecture

```
ScratchBird                    External System
┌─────────────┐               ┌───────────────┐
│  Query      │ ──SQL/SBLR──► │  PostgreSQL   │
│  Planner    │               │  MySQL        │
│             │ ◄──Results──  │  MongoDB      │
│  FDW        │               │  REST API     │
│  Interface  │               │  Files        │
└─────────────┘               └───────────────┘
```

## Creating FDW

### 1. Create Foreign Data Wrapper

```sql
-- Create FDW (usually pre-installed)
CREATE FOREIGN DATA WRAPPER postgres_fdw
    HANDLER postgres_fdw_handler
    VALIDATOR postgres_fdw_validator;
```

### 2. Create Server

```sql
-- Define external server
CREATE SERVER foreign_pg
    FOREIGN DATA WRAPPER postgres_fdw
    OPTIONS (
        host 'external-db.example.com',
        port '5432',
        dbname 'production'
    );
```

### 3. Create User Mapping

```sql
-- Map local user to remote user
CREATE USER MAPPING FOR current_user
    SERVER foreign_pg
    OPTIONS (
        user 'remote_user',
        password 'remote_password'
    );
```

### 4. Create Foreign Table

```sql
-- Define foreign table structure
CREATE FOREIGN TABLE remote_users (
    id INTEGER,
    name TEXT,
    email TEXT
)
SERVER foreign_pg
OPTIONS (
    schema_name 'public',
    table_name 'users'
);
```

## Available FDWs

| FDW | External System | Use Case |
|-----|-----------------|----------|
| `postgres_fdw` | PostgreSQL | Cross-database queries |
| `mysql_fdw` | MySQL | MySQL integration |
| `mongo_fdw` | MongoDB | Document store access |
| `file_fdw` | CSV/JSON files | File-based data |
| `http_fdw` | REST APIs | Web service integration |
| `redis_fdw` | Redis | Cache access |
| `jdbc_fdw` | Any JDBC | Generic database access |

## Examples

### PostgreSQL Integration

```sql
-- Setup
CREATE EXTENSION postgres_fdw;

CREATE SERVER prod_pg
    FOREIGN DATA WRAPPER postgres_fdw
    OPTIONS (host 'prod.db.internal', port '5432', dbname 'analytics');

CREATE USER MAPPING FOR app_user
    SERVER prod_pg
    OPTIONS (user 'readonly', password 'secret');

-- Import all tables from schema
IMPORT FOREIGN SCHEMA public
    LIMIT TO (users, orders)
    FROM SERVER prod_pg
    INTO local_schema;

-- Query remote data
SELECT * FROM local_schema.users WHERE created_at > '2024-01-01';

-- Join local and remote
SELECT 
    l.name,
    r.order_count
FROM local_users l
JOIN local_schema.users r ON l.email = r.email;
```

### MySQL Integration

```sql
-- Setup MySQL FDW
CREATE EXTENSION mysql_fdw;

CREATE SERVER mysql_prod
    FOREIGN DATA WRAPPER mysql_fdw
    OPTIONS (host 'mysql.internal', port '3306');

CREATE USER MAPPING FOR current_user
    SERVER mysql_prod
    OPTIONS (username 'app', password 'secret');

-- Create foreign table
CREATE FOREIGN TABLE mysql_customers (
    id INT,
    name VARCHAR(255),
    created_at TIMESTAMP
)
SERVER mysql_prod
OPTIONS (database 'production', table_name 'customers');
```

### File-based Data

```sql
-- CSV file access
CREATE EXTENSION file_fdw;

CREATE SERVER csv_server
    FOREIGN DATA WRAPPER file_fdw;

CREATE FOREIGN TABLE csv_import (
    id INTEGER,
    name TEXT,
    amount NUMERIC
)
SERVER csv_server
OPTIONS (
    filename '/data/import.csv',
    format 'csv',
    header 'true'
);

-- Query CSV like a table
SELECT * FROM csv_import WHERE amount > 1000;
```

### REST API Access

```sql
-- HTTP FDW for API integration
CREATE EXTENSION http_fdw;

CREATE SERVER api_server
    FOREIGN DATA WRAPPER http_fdw
    OPTIONS (uri 'https://api.example.com/v1');

CREATE FOREIGN TABLE api_users (
    id INTEGER,
    name TEXT,
    status TEXT
)
SERVER api_server
OPTIONS (endpoint '/users');
```

## Pushdown Optimization

FDWs can push down operations to remote systems:

```sql
-- WHERE pushdown
SELECT * FROM remote_users WHERE id > 1000;
-- -> Sent to remote: SELECT * FROM users WHERE id > 1000

-- Aggregate pushdown
SELECT department, COUNT(*) FROM remote_users GROUP BY department;
-- -> Remote aggregation performed

-- Join pushdown (if both tables on same server)
SELECT * FROM remote_users u JOIN remote_orders o ON u.id = o.user_id;
-- -> Join performed remotely
```

## Performance Considerations

### 1. Limit Data Transfer

```sql
-- Good: Filter at source
SELECT * FROM remote_users WHERE created_at > '2024-01-01';

-- Bad: Pull all, filter locally
SELECT * FROM remote_users;  -- Then filter in application
```

### 2. Use Materialized Views

```sql
-- Cache remote data locally
CREATE MATERIALIZED VIEW cached_remote_users AS
SELECT * FROM remote_users;

-- Refresh periodically
REFRESH MATERIALIZED VIEW cached_remote_users;
```

### 3. Connection Pooling

```ini
# In scratchbird.conf
fdw.connection_pool_size = 10
fdw.connection_timeout = 30
```

## Security

### Row-Level Security with FDW

```sql
-- Apply RLS to foreign table
ALTER FOREIGN TABLE remote_users ENABLE ROW LEVEL SECURITY;

CREATE POLICY tenant_isolation ON remote_users
    USING (tenant_id = current_setting('app.tenant_id'));
```

### Encrypted Connections

```sql
-- Force SSL
ALTER SERVER foreign_pg OPTIONS (ADD sslmode 'require');
```

## Monitoring

```sql
-- Check FDW statistics
SELECT * FROM pg_stat_user_tables WHERE relkind = 'f';

-- View FDW queries
SELECT * FROM pg_stat_activity WHERE query LIKE '%remote_%';
```

## See Also

- [CREATE SERVER](02_create_server.md)
- [CREATE USER MAPPING](03_create_user_mapping.md)
- [IMPORT FOREIGN SCHEMA](04_import_foreign_schema.md)
