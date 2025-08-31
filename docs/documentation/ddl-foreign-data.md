### DDL: Foreign Data

**What it is**

Foreign Data Wrappers (FDW) enable ScratchBird to access and query data stored in external systems as if they were local tables. This includes other databases, file systems, web services, and custom data sources. The FDW infrastructure consists of foreign servers (connection definitions), user mappings (authentication), and foreign tables (schema mappings).

**Why it matters**

- **Data Federation**: Query multiple data sources with single SQL statements
- **ETL Simplification**: Direct access eliminates intermediate staging
- **Real-time Integration**: Access live external data without replication
- **Storage Flexibility**: Keep data in optimal locations
- **Migration Support**: Gradual migration without downtime

**How to use it**

Install the appropriate FDW extension, create a foreign server with connection parameters, set up user mappings for authentication, then create foreign tables that map to external data structures. Query foreign tables like regular tables, with the FDW handling data retrieval and pushdown optimization.

## Foreign Servers

### CREATE SERVER

```sql
-- Basic foreign server
CREATE SERVER foreign_server_name
    FOREIGN DATA WRAPPER wrapper_name;

-- PostgreSQL server
CREATE SERVER pg_remote
    FOREIGN DATA WRAPPER postgres_fdw
    OPTIONS (
        host 'remote.example.com',
        port '5432',
        dbname 'remote_db'
    );

-- MySQL server
CREATE SERVER mysql_remote
    FOREIGN DATA WRAPPER mysql_fdw
    OPTIONS (
        host '192.168.1.100',
        port '3306'
    );

-- File server (CSV/JSON)
CREATE SERVER file_server
    FOREIGN DATA WRAPPER file_fdw
    OPTIONS (
        directory '/data/import'
    );

-- MongoDB server
CREATE SERVER mongo_remote
    FOREIGN DATA WRAPPER mongo_fdw
    OPTIONS (
        address 'mongodb://mongo.example.com:27017',
        database 'app_db'
    );

-- REST API server
CREATE SERVER api_server
    FOREIGN DATA WRAPPER rest_fdw
    OPTIONS (
        base_url 'https://api.example.com/v1',
        timeout '30'
    );
```

### ALTER SERVER

```sql
-- Change server options
ALTER SERVER pg_remote
    OPTIONS (SET host 'new-remote.example.com');

-- Add new option
ALTER SERVER pg_remote
    OPTIONS (ADD sslmode 'require');

-- Remove option
ALTER SERVER pg_remote
    OPTIONS (DROP port);

-- Change owner
ALTER SERVER pg_remote OWNER TO fdw_admin;

-- Rename server
ALTER SERVER old_name RENAME TO new_name;
```

### DROP SERVER

```sql
-- Drop server
DROP SERVER foreign_server;

-- Drop if exists
DROP SERVER IF EXISTS obsolete_server;

-- Drop with cascade (drops dependent objects)
DROP SERVER pg_remote CASCADE;
```

## User Mappings

### CREATE USER MAPPING

```sql
-- Basic user mapping
CREATE USER MAPPING FOR local_user
    SERVER foreign_server
    OPTIONS (user 'remote_user', password 'secret');

-- Mapping for current user
CREATE USER MAPPING FOR CURRENT_USER
    SERVER pg_remote
    OPTIONS (user 'app_user', password 'SecurePass123');

-- Mapping for PUBLIC (all users)
CREATE USER MAPPING FOR PUBLIC
    SERVER file_server
    OPTIONS (user 'reader');

-- Certificate authentication
CREATE USER MAPPING FOR admin
    SERVER secure_server
    OPTIONS (
        sslcert '/path/to/client.crt',
        sslkey '/path/to/client.key'
    );

-- OAuth authentication
CREATE USER MAPPING FOR api_user
    SERVER api_server
    OPTIONS (
        token 'Bearer eyJhbGciOiJIUzI1NiIs...',
        refresh_token 'refresh_token_value'
    );
```

### ALTER USER MAPPING

```sql
-- Change password
ALTER USER MAPPING FOR local_user
    SERVER pg_remote
    OPTIONS (SET password 'NewPassword456');

-- Add option
ALTER USER MAPPING FOR local_user
    SERVER pg_remote
    OPTIONS (ADD sslmode 'require');

-- Drop option
ALTER USER MAPPING FOR local_user
    SERVER pg_remote
    OPTIONS (DROP sslmode);
```

### DROP USER MAPPING

```sql
-- Drop mapping
DROP USER MAPPING FOR local_user SERVER pg_remote;

-- Drop if exists
DROP USER MAPPING IF EXISTS FOR obsolete_user SERVER old_server;
```

## Foreign Tables

### CREATE FOREIGN TABLE

```sql
-- Basic foreign table
CREATE FOREIGN TABLE foreign_customers (
    id INTEGER,
    name VARCHAR(100),
    email VARCHAR(100),
    created_at TIMESTAMP
) SERVER pg_remote
OPTIONS (schema_name 'public', table_name 'customers');

-- With column options
CREATE FOREIGN TABLE remote_orders (
    order_id INTEGER OPTIONS (column_name 'id'),
    customer_id INTEGER,
    order_date DATE OPTIONS (column_name 'created'),
    total DECIMAL(10,2),
    status VARCHAR(20)
) SERVER pg_remote
OPTIONS (schema_name 'sales', table_name 'orders');

-- CSV file table
CREATE FOREIGN TABLE csv_import (
    product_code VARCHAR(20),
    description TEXT,
    price DECIMAL(10,2),
    quantity INTEGER
) SERVER file_server
OPTIONS (
    filename 'products.csv',
    format 'csv',
    header 'true',
    delimiter ',',
    quote '"',
    escape '"',
    null ''
);

-- JSON file table
CREATE FOREIGN TABLE json_data (
    doc JSONB
) SERVER file_server
OPTIONS (
    filename 'data.json',
    format 'json'
);

-- MongoDB collection
CREATE FOREIGN TABLE mongo_events (
    _id VARCHAR(24),
    event_type VARCHAR(50),
    user_id INTEGER,
    timestamp TIMESTAMP,
    data JSONB
) SERVER mongo_remote
OPTIONS (collection 'events');

-- REST API endpoint
CREATE FOREIGN TABLE api_users (
    id INTEGER,
    username VARCHAR(50),
    email VARCHAR(100),
    active BOOLEAN
) SERVER api_server
OPTIONS (
    endpoint '/users',
    method 'GET',
    result_path '$.data[*]'
);
```

### ALTER FOREIGN TABLE

```sql
-- Add column
ALTER FOREIGN TABLE foreign_customers
    ADD COLUMN phone VARCHAR(20);

-- Drop column
ALTER FOREIGN TABLE foreign_customers
    DROP COLUMN phone;

-- Change column type
ALTER FOREIGN TABLE foreign_customers
    ALTER COLUMN email TYPE VARCHAR(200);

-- Set column options
ALTER FOREIGN TABLE foreign_customers
    ALTER COLUMN email OPTIONS (column_name 'email_address');

-- Change table options
ALTER FOREIGN TABLE foreign_customers
    OPTIONS (SET table_name 'customer_master');

-- Add constraint (informational only)
ALTER FOREIGN TABLE foreign_customers
    ADD CONSTRAINT customers_email_unique UNIQUE (email);
```

### DROP FOREIGN TABLE

```sql
-- Drop foreign table
DROP FOREIGN TABLE foreign_customers;

-- Drop if exists
DROP FOREIGN TABLE IF EXISTS old_import;

-- Drop with cascade
DROP FOREIGN TABLE remote_orders CASCADE;
```

## Import Foreign Schema

```sql
-- Import entire schema
IMPORT FOREIGN SCHEMA public
    FROM SERVER pg_remote
    INTO local_schema;

-- Import specific tables
IMPORT FOREIGN SCHEMA public
    LIMIT TO (customers, orders, products)
    FROM SERVER pg_remote
    INTO local_schema;

-- Import except specific tables
IMPORT FOREIGN SCHEMA public
    EXCEPT (audit_log, temp_data)
    FROM SERVER pg_remote
    INTO local_schema;

-- Import with options
IMPORT FOREIGN SCHEMA remote_schema
    FROM SERVER pg_remote
    INTO local_schema
    OPTIONS (import_collate 'false', import_default 'false');
```

## Querying Foreign Data

### Basic Queries

```sql
-- Simple select
SELECT * FROM foreign_customers WHERE country = 'USA';

-- Join with local table
SELECT 
    l.order_id,
    l.order_date,
    f.name AS customer_name,
    f.email
FROM local_orders l
JOIN foreign_customers f ON l.customer_id = f.id;

-- Aggregation
SELECT 
    country,
    COUNT(*) AS customer_count,
    AVG(total_purchases) AS avg_purchases
FROM foreign_customers
GROUP BY country;
```

### Push-down Optimization

```sql
-- WHERE clause push-down
EXPLAIN (VERBOSE)
SELECT * FROM foreign_orders
WHERE order_date >= '2024-01-01';
-- Remote SQL: SELECT * FROM orders WHERE created >= '2024-01-01'

-- Aggregate push-down
EXPLAIN (VERBOSE)
SELECT country, COUNT(*)
FROM foreign_customers
GROUP BY country;
-- Remote SQL: SELECT country, COUNT(*) FROM customers GROUP BY country

-- Join push-down (if both tables on same server)
EXPLAIN (VERBOSE)
SELECT *
FROM foreign_orders o
JOIN foreign_customers c ON o.customer_id = c.id
WHERE c.country = 'USA';
-- Remote SQL: SELECT ... FROM orders o JOIN customers c ON ... WHERE ...
```

### Data Modification

```sql
-- Insert into foreign table
INSERT INTO foreign_customers (name, email, created_at)
VALUES ('New Customer', 'new@example.com', CURRENT_TIMESTAMP);

-- Update foreign data
UPDATE foreign_customers
SET email = 'updated@example.com'
WHERE id = 123;

-- Delete from foreign table
DELETE FROM foreign_customers
WHERE status = 'inactive' AND created_at < '2020-01-01';

-- Insert with returning
INSERT INTO foreign_orders (customer_id, total, status)
VALUES (456, 299.99, 'pending')
RETURNING order_id;
```

## Common FDW Patterns

### Data Lake Integration

```sql
-- S3 data lake
CREATE SERVER s3_lake
    FOREIGN DATA WRAPPER s3_fdw
    OPTIONS (
        region 'us-east-1',
        bucket 'company-data-lake'
    );

CREATE FOREIGN TABLE lake_events (
    event_time TIMESTAMP,
    event_type VARCHAR(50),
    user_id BIGINT,
    properties JSONB
) SERVER s3_lake
OPTIONS (
    prefix 'events/year=2024/month=01/',
    format 'parquet'
);

-- Query partitioned data
SELECT 
    date_trunc('day', event_time) AS day,
    event_type,
    COUNT(*) AS event_count
FROM lake_events
WHERE event_time >= '2024-01-01'
GROUP BY 1, 2;
```

### Multi-Database Sharding

```sql
-- Shard servers
CREATE SERVER shard_1 FOREIGN DATA WRAPPER postgres_fdw
    OPTIONS (host 'shard1.example.com', dbname 'app');

CREATE SERVER shard_2 FOREIGN DATA WRAPPER postgres_fdw
    OPTIONS (host 'shard2.example.com', dbname 'app');

-- Shard tables
CREATE FOREIGN TABLE orders_shard_1 (LIKE orders)
    SERVER shard_1 OPTIONS (table_name 'orders');

CREATE FOREIGN TABLE orders_shard_2 (LIKE orders)
    SERVER shard_2 OPTIONS (table_name 'orders');

-- Partitioned view
CREATE VIEW orders_all AS
    SELECT * FROM orders_shard_1
    UNION ALL
    SELECT * FROM orders_shard_2;

-- Query across shards
SELECT customer_id, SUM(total) AS total_spent
FROM orders_all
GROUP BY customer_id;
```

### Real-time Data Streaming

```sql
-- Kafka streaming
CREATE SERVER kafka_server
    FOREIGN DATA WRAPPER kafka_fdw
    OPTIONS (
        brokers 'kafka1:9092,kafka2:9092',
        topic 'user-events'
    );

CREATE FOREIGN TABLE event_stream (
    timestamp TIMESTAMP,
    user_id INTEGER,
    action VARCHAR(50),
    payload JSONB
) SERVER kafka_server
OPTIONS (
    format 'json',
    offset 'earliest'
);

-- Materialize streaming data
CREATE MATERIALIZED VIEW user_activity AS
SELECT 
    user_id,
    date_trunc('hour', timestamp) AS hour,
    COUNT(*) AS action_count,
    array_agg(DISTINCT action) AS actions
FROM event_stream
WHERE timestamp > CURRENT_TIMESTAMP - INTERVAL '24 hours'
GROUP BY user_id, date_trunc('hour', timestamp);
```

### API Gateway

```sql
-- REST API integration
CREATE SERVER api_gateway
    FOREIGN DATA WRAPPER rest_fdw
    OPTIONS (
        base_url 'https://api.internal.com',
        auth_type 'bearer',
        auth_token '${API_TOKEN}'
    );

-- Customer API
CREATE FOREIGN TABLE api_customers (
    id INTEGER,
    name VARCHAR(100),
    subscription_tier VARCHAR(20),
    monthly_revenue DECIMAL(10,2)
) SERVER api_gateway
OPTIONS (
    endpoint '/customers',
    method 'GET',
    page_size '100'
);

-- Writable API endpoint
CREATE FOREIGN TABLE api_orders (
    order_id INTEGER,
    customer_id INTEGER,
    items JSONB,
    total DECIMAL(10,2)
) SERVER api_gateway
OPTIONS (
    endpoint '/orders',
    method 'POST' FOR INSERT,
    method 'PUT' FOR UPDATE,
    method 'DELETE' FOR DELETE
);
```

## Performance Considerations

### Connection Pooling

```sql
-- Configure connection pooling
ALTER SERVER pg_remote
    OPTIONS (
        ADD connection_pool 'true',
        ADD pool_size '10',
        ADD pool_timeout '30'
    );
```

### Caching

```sql
-- Enable result caching
ALTER FOREIGN TABLE foreign_customers
    OPTIONS (
        ADD cache_ttl '300',  -- 5 minutes
        ADD cache_size '100MB'
    );
```

### Batch Operations

```sql
-- Configure batch size for modifications
ALTER FOREIGN TABLE foreign_orders
    OPTIONS (
        ADD batch_size '1000'
    );

-- Bulk insert
INSERT INTO foreign_orders
SELECT * FROM local_staging_orders
WHERE processed = false;
```

## Monitoring and Troubleshooting

```sql
-- View foreign servers
SELECT 
    srvname AS server_name,
    srvowner::regrole AS owner,
    fdwname AS wrapper,
    srvoptions AS options
FROM pg_foreign_server
JOIN pg_foreign_data_wrapper ON srvfdw = fdw.oid;

-- View foreign tables
SELECT 
    n.nspname AS schema,
    c.relname AS table_name,
    s.srvname AS server,
    ftoptions AS options
FROM pg_foreign_table ft
JOIN pg_class c ON ft.ftrelid = c.oid
JOIN pg_namespace n ON c.relnamespace = n.oid
JOIN pg_foreign_server s ON ft.ftserver = s.oid;

-- Check FDW statistics
SELECT 
    schemaname,
    tablename,
    n_tup_ins AS inserts,
    n_tup_upd AS updates,
    n_tup_del AS deletes,
    n_tup_hot_upd AS hot_updates
FROM pg_stat_user_tables
WHERE schemaname = 'foreign_schema';
```

## Implementation Details

**Parser** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_foreign_server`: CREATE/ALTER/DROP SERVER
- `parse_ddl_user_mapping`: User mapping management
- `parse_ddl_foreign_table`: Foreign table operations
- `parse_ddl_import_foreign_schema`: Schema import

**FDW Interface** (`include/scratchbird/engine/fdw.h`):
- Foreign data wrapper API
- Iterator interface
- Push-down capabilities

**Code Anchors**:
- Foreign server parser: `src/engine/parser_ddl.cpp` (parse_ddl_foreign_server)
- User mapping parser: `src/engine/parser_ddl.cpp` (parse_ddl_user_mapping)
- Foreign table parser: `src/engine/parser_ddl.cpp` (parse_ddl_foreign_table)
- Import schema parser: `src/engine/parser_ddl.cpp` (parse_ddl_import_foreign_schema)
- FDW interface: `include/scratchbird/engine/fdw.h`

## See also

- [Tables](./ddl-tables.md) - Local table management
- [Views](./ddl-views.md) - Creating views over foreign tables
- [Materialized Views](./ddl-materialized-views.md) - Caching foreign data
- [Database Links](./ddl-database-links.md) - Direct database connections
- [Performance](./explain-analyze.md) - Analyzing foreign table queries