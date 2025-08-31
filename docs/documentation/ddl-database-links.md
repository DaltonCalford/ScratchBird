### DDL: Database Links

**What it is**

Database links (dblinks) provide direct connections between databases, allowing queries to access remote database objects as if they were local. Unlike foreign data wrappers which provide a generic interface, database links offer native, optimized connections between ScratchBird instances or compatible databases. They support transparent remote execution, distributed transactions, and remote procedure calls.

**Why it matters**

- **Distributed Queries**: Join data across multiple databases seamlessly
- **Data Integration**: Access remote data without replication
- **Microservices**: Enable database-per-service architecture
- **Legacy Integration**: Connect to existing database systems
- **Performance**: Optimized for database-to-database communication

**How to use it**

Create a database link with connection credentials, then reference remote objects using the @dblink syntax. Database links support SELECT, INSERT, UPDATE, DELETE operations and can participate in local transactions. Use them for real-time data access, distributed queries, and cross-database operations.

## CREATE DATABASE LINK

### Basic Syntax

```sql
-- Simple database link
CREATE DATABASE LINK remote_db
CONNECT TO remote_user IDENTIFIED BY 'password'
USING 'host=remote.server.com port=5439 dbname=target_db';

-- Database link with connection options
CREATE DATABASE LINK analytics_link
CONNECT TO analyst_user IDENTIFIED BY 'SecurePass123!'
USING '(DESCRIPTION=
    (ADDRESS=(PROTOCOL=TCP)(HOST=analytics.db.com)(PORT=5439))
    (CONNECT_DATA=(SERVICE_NAME=analytics))
    (SECURITY=(SSL_SERVER_CERT_DN="CN=analytics.db.com")))';

-- Public database link (available to all users)
CREATE PUBLIC DATABASE LINK shared_link
CONNECT TO public_user IDENTIFIED BY 'password'
USING 'host=shared.db.com dbname=shared';

-- Database link with current user credentials
CREATE DATABASE LINK user_link
CONNECT TO CURRENT_USER
USING 'host=remote.db.com dbname=app';

-- Database link with connection pooling
CREATE DATABASE LINK pooled_link
CONNECT TO app_user IDENTIFIED BY 'password'
USING 'host=app.db.com dbname=application'
WITH (
    min_pool_size = 5,
    max_pool_size = 20,
    connection_timeout = 30
);
```

### Authentication Methods

```sql
-- Password authentication
CREATE DATABASE LINK pwd_link
CONNECT TO dbuser IDENTIFIED BY 'password'
USING 'host=server.com dbname=db';

-- Certificate authentication
CREATE DATABASE LINK cert_link
CONNECT TO cert_user IDENTIFIED BY CERTIFICATE '/path/to/client.crt'
USING 'host=secure.db.com dbname=secure sslmode=require';

-- Kerberos authentication
CREATE DATABASE LINK krb_link
CONNECT TO krb_user AUTHENTICATED BY KERBEROS
USING 'host=krb.db.com dbname=protected';

-- OAuth authentication
CREATE DATABASE LINK oauth_link
CONNECT TO oauth_user IDENTIFIED BY TOKEN 'oauth_token_here'
USING 'host=api.db.com dbname=api';
```

## Using Database Links

### Remote Queries

```sql
-- Simple SELECT
SELECT * FROM employees@remote_db;

-- Specific columns
SELECT emp_id, name, salary 
FROM employees@remote_db
WHERE department = 'Sales';

-- Join remote and local tables
SELECT l.order_id, l.order_date, r.customer_name
FROM local_orders l
JOIN customers@remote_db r ON l.customer_id = r.id;

-- Multiple remote sources
SELECT 
    o.order_id,
    c.customer_name,
    p.product_name
FROM orders@sales_db o
JOIN customers@crm_db c ON o.customer_id = c.id
JOIN products@inventory_db p ON o.product_id = p.id;
```

### Remote DML Operations

```sql
-- INSERT into remote table
INSERT INTO audit_log@remote_db (event, user_id, timestamp)
VALUES ('login', 123, CURRENT_TIMESTAMP);

-- UPDATE remote data
UPDATE inventory@warehouse_db
SET quantity = quantity - 10
WHERE product_id = 'ABC123';

-- DELETE from remote table
DELETE FROM temp_data@remote_db
WHERE created_at < CURRENT_DATE - INTERVAL '30 days';

-- MERGE with remote table
MERGE INTO customers@crm_db target
USING local_customer_updates source
ON (target.id = source.id)
WHEN MATCHED THEN
    UPDATE SET email = source.email, phone = source.phone
WHEN NOT MATCHED THEN
    INSERT (id, name, email, phone)
    VALUES (source.id, source.name, source.email, source.phone);
```

### Remote Procedure Calls

```sql
-- Call remote function
SELECT calculate_discount@sales_db(customer_id, order_total);

-- Execute remote procedure
EXECUTE process_payment@payment_db(order_id, amount, 'credit_card');

-- Remote function in expression
SELECT 
    order_id,
    total,
    get_tax_rate@tax_db(state) * total AS tax
FROM orders;

-- Remote package procedure
EXECUTE billing_pkg.generate_invoice@accounting_db(invoice_id);
```

## ALTER DATABASE LINK

```sql
-- Change connection string
ALTER DATABASE LINK remote_db
USING 'host=new-server.com port=5439 dbname=target_db';

-- Change credentials
ALTER DATABASE LINK remote_db
CONNECT TO new_user IDENTIFIED BY 'new_password';

-- Add connection options
ALTER DATABASE LINK remote_db
SET max_connections = 50,
    connection_timeout = 60,
    query_timeout = 300;

-- Enable/disable link
ALTER DATABASE LINK remote_db DISABLE;
ALTER DATABASE LINK remote_db ENABLE;

-- Rename database link
ALTER DATABASE LINK old_link RENAME TO new_link;
```

## DROP DATABASE LINK

```sql
-- Drop private database link
DROP DATABASE LINK remote_db;

-- Drop public database link
DROP PUBLIC DATABASE LINK shared_link;

-- Drop if exists
DROP DATABASE LINK IF EXISTS old_link;

-- Force drop (closes active connections)
DROP DATABASE LINK active_link FORCE;
```

## Distributed Transactions

### Two-Phase Commit

```sql
-- Begin distributed transaction
BEGIN;

-- Local operation
UPDATE local_accounts SET balance = balance - 100 WHERE id = 1;

-- Remote operation
UPDATE accounts@remote_db SET balance = balance + 100 WHERE id = 2;

-- Commit both or rollback both
COMMIT;  -- Two-phase commit protocol ensures consistency
```

### Distributed Queries

```sql
-- Aggregate across databases
SELECT 
    region,
    SUM(local_sales) + SUM(remote_sales) AS total_sales
FROM (
    SELECT region, amount AS local_sales, 0 AS remote_sales
    FROM sales
    UNION ALL
    SELECT region, 0 AS local_sales, amount AS remote_sales
    FROM sales@remote_db
) combined
GROUP BY region;

-- Distributed JOIN with pushdown
SELECT /*+ DRIVING_SITE(remote_db) */
    o.order_id,
    o.total,
    c.customer_name
FROM large_orders@remote_db o
JOIN small_customers c ON o.customer_id = c.id;
```

## Performance Optimization

### Query Pushdown

```sql
-- Entire query executed remotely
SELECT * FROM (
    SELECT * FROM large_table@remote_db
    WHERE status = 'active'
    ORDER BY created_at DESC
    LIMIT 100
);

-- Aggregate pushdown
SELECT region, SUM(amount), COUNT(*)
FROM sales@remote_db
GROUP BY region;

-- Join pushdown (both tables remote)
SELECT *
FROM orders@remote_db o
JOIN customers@remote_db c ON o.customer_id = c.id
WHERE o.order_date >= '2024-01-01';
```

### Connection Pooling

```sql
-- Configure connection pool
ALTER DATABASE LINK high_traffic_link
SET (
    min_pool_size = 10,
    max_pool_size = 50,
    idle_timeout = 300,
    max_lifetime = 3600
);

-- Monitor connection pool
SELECT 
    link_name,
    active_connections,
    idle_connections,
    total_connections,
    wait_queue_length
FROM pg_dblink_pools;
```

### Caching

```sql
-- Enable result caching
ALTER DATABASE LINK cached_link
SET (
    result_cache = true,
    cache_size = '100MB',
    cache_ttl = 300  -- 5 minutes
);

-- Hint for caching
SELECT /*+ RESULT_CACHE */ 
    product_id, price
FROM products@cached_link
WHERE category = 'Electronics';
```

## Security Considerations

### Encrypted Connections

```sql
-- SSL/TLS encrypted link
CREATE DATABASE LINK secure_link
CONNECT TO secure_user IDENTIFIED BY 'password'
USING 'host=secure.db.com dbname=sensitive sslmode=require 
       sslcert=/path/to/client.crt sslkey=/path/to/client.key';

-- Verify encryption
SELECT 
    link_name,
    encryption_method,
    cipher_suite,
    certificate_cn
FROM pg_dblink_security
WHERE link_name = 'secure_link';
```

### Access Control

```sql
-- Grant usage on database link
GRANT USAGE ON DATABASE LINK remote_db TO app_role;

-- Revoke access
REVOKE USAGE ON DATABASE LINK sensitive_link FROM public;

-- Audit database link usage
CREATE TRIGGER audit_dblink_access
AFTER SELECT OR INSERT OR UPDATE OR DELETE
ON pg_dblink_queries
FOR EACH ROW
EXECUTE FUNCTION log_dblink_access();
```

## Monitoring and Troubleshooting

### Link Status

```sql
-- View all database links
SELECT 
    link_name,
    owner,
    is_public,
    host,
    database,
    username,
    created_at,
    last_used
FROM pg_database_links
ORDER BY link_name;

-- Check link connectivity
SELECT test_database_link('remote_db');

-- Active connections
SELECT 
    link_name,
    pid,
    query,
    query_start,
    state,
    wait_event
FROM pg_stat_dblink_activity;
```

### Performance Metrics

```sql
-- Link performance statistics
SELECT 
    link_name,
    total_queries,
    total_time,
    mean_time,
    max_time,
    total_bytes_sent,
    total_bytes_received,
    error_count
FROM pg_stat_database_links
ORDER BY total_time DESC;

-- Slow queries over dblinks
SELECT 
    link_name,
    query,
    execution_time,
    rows_returned,
    timestamp
FROM pg_dblink_slow_log
WHERE execution_time > interval '1 second'
ORDER BY execution_time DESC;
```

## Common Patterns

### Data Federation

```sql
-- Create view over multiple databases
CREATE VIEW global_inventory AS
    SELECT 'US' AS region, * FROM inventory@us_db
    UNION ALL
    SELECT 'EU' AS region, * FROM inventory@eu_db
    UNION ALL
    SELECT 'ASIA' AS region, * FROM inventory@asia_db;

-- Query federated view
SELECT region, product_id, SUM(quantity) AS total_quantity
FROM global_inventory
GROUP BY region, product_id;
```

### Real-time Synchronization

```sql
-- Trigger-based synchronization
CREATE TRIGGER sync_customer_changes
AFTER INSERT OR UPDATE OR DELETE ON customers
FOR EACH ROW
EXECUTE FUNCTION sync_to_remote();

CREATE FUNCTION sync_to_remote() RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' THEN
        INSERT INTO customers@remote_db VALUES (NEW.*);
    ELSIF TG_OP = 'UPDATE' THEN
        UPDATE customers@remote_db SET ROW = NEW.* WHERE id = NEW.id;
    ELSIF TG_OP = 'DELETE' THEN
        DELETE FROM customers@remote_db WHERE id = OLD.id;
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;
```

### Microservice Database Access

```sql
-- User service database link
CREATE DATABASE LINK user_service_db
CONNECT TO svc_user IDENTIFIED BY 'svc_password'
USING 'host=user-service.internal dbname=users';

-- Order service database link
CREATE DATABASE LINK order_service_db
CONNECT TO svc_order IDENTIFIED BY 'svc_password'
USING 'host=order-service.internal dbname=orders';

-- Composite query across services
CREATE VIEW customer_orders AS
SELECT 
    u.user_id,
    u.username,
    u.email,
    o.order_id,
    o.order_date,
    o.total
FROM users@user_service_db u
LEFT JOIN orders@order_service_db o ON u.user_id = o.user_id;
```

## Best Practices

1. **Connection Management**
   - Use connection pooling for frequently accessed links
   - Set appropriate timeouts
   - Monitor connection usage

2. **Security**
   - Always use encrypted connections for sensitive data
   - Implement least-privilege access
   - Audit database link usage

3. **Performance**
   - Push operations to remote database when possible
   - Use result caching for stable data
   - Monitor network latency

4. **Reliability**
   - Implement retry logic for transient failures
   - Use connection validation
   - Plan for link unavailability

## Implementation Details

**Parser** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_dblink`: CREATE/ALTER/DROP DATABASE LINK
- Handles connection string parsing
- Validates authentication methods

**DBLink Manager**:
- Connection pooling
- Query routing
- Transaction coordination
- Security enforcement

**Code Anchors**:
- Database link parser: `src/engine/parser_ddl.cpp` (parse_ddl_dblink)
- DBLink manager: `src/engine/dblink_manager.cpp`
- Remote execution: `src/engine/remote_executor.cpp`
- AST definitions: `include/scratchbird/engine/ast.h`

## See also

- [Foreign Data](./ddl-foreign-data.md) - Generic foreign data access
- [Publication/Subscription](./ddl-publication-subscription.md) - Logical replication
- [Cluster](./ddl-cluster.md) - Cluster configuration
- [Security](./ddl-roles-users-grants.md) - Access control
- [Transactions](./session-and-transaction.md) - Distributed transactions