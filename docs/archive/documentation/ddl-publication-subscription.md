### DDL: Publication and Subscription

**What it is**

Publication and subscription provide logical replication capabilities, allowing selective data replication between databases. Publications define sets of tables to replicate, while subscriptions connect to publications and receive changes. This mechanism enables fine-grained control over what data is replicated, supporting use cases like data distribution, load balancing, and multi-tenant architectures.

**Why it matters**

- **Selective Replication**: Replicate only specific tables or columns
- **Cross-Version Compatibility**: Replicate between different database versions
- **Multi-Master Patterns**: Support bidirectional replication scenarios
- **Data Distribution**: Distribute subsets of data to different locations
- **Zero-Downtime Migration**: Enable gradual migrations

**How to use it**

On the publisher database, create a publication that includes the tables you want to replicate. On the subscriber database, create a subscription that connects to the publication. The subscription will initially copy the data and then continuously apply changes. Monitor replication lag and conflicts through system views.

## Publications

### CREATE PUBLICATION

```sql
-- Basic publication for all tables
CREATE PUBLICATION pub_all FOR ALL TABLES;

-- Publication for specific tables
CREATE PUBLICATION pub_orders 
FOR TABLE orders, order_items, customers;

-- Publication with row filtering
CREATE PUBLICATION pub_active_customers
FOR TABLE customers 
WHERE (status = 'active');

-- Publication for specific operations
CREATE PUBLICATION pub_inserts_only
FOR TABLE audit_log
WITH (publish = 'insert');

-- Publication with column lists
CREATE PUBLICATION pub_customer_basic
FOR TABLE customers (id, name, email, created_at);

-- Publication for schema
CREATE PUBLICATION pub_sales_schema
FOR ALL TABLES IN SCHEMA sales;
```

### Publication Options

```sql
-- Full publication options
CREATE PUBLICATION complex_pub
FOR TABLE 
    orders WHERE (status IN ('pending', 'processing')),
    customers (id, name, email) WHERE (region = 'US'),
    products
WITH (
    publish = 'insert, update, delete, truncate',
    publish_via_partition_root = false
);

-- Publish only specific operations
CREATE PUBLICATION audit_pub
FOR TABLE audit_events
WITH (
    publish = 'insert',  -- No updates or deletes
    publish_via_partition_root = true
);
```

### ALTER PUBLICATION

```sql
-- Add tables to publication
ALTER PUBLICATION pub_orders ADD TABLE invoices, payments;

-- Remove tables from publication
ALTER PUBLICATION pub_orders DROP TABLE customers;

-- Change publication options
ALTER PUBLICATION pub_orders 
SET (publish = 'insert, update');

-- Add row filter
ALTER PUBLICATION pub_customers
SET TABLE customers WHERE (country = 'USA');

-- Change column list
ALTER PUBLICATION pub_customers
SET TABLE customers (id, name, email, status);

-- Rename publication
ALTER PUBLICATION old_pub RENAME TO new_pub;

-- Change owner
ALTER PUBLICATION pub_orders OWNER TO replication_admin;
```

### DROP PUBLICATION

```sql
-- Drop publication
DROP PUBLICATION pub_old;

-- Drop if exists
DROP PUBLICATION IF EXISTS pub_temp;

-- Drop with cascade (drops dependent subscriptions)
DROP PUBLICATION pub_main CASCADE;
```

## Subscriptions

### CREATE SUBSCRIPTION

```sql
-- Basic subscription
CREATE SUBSCRIPTION sub_orders
CONNECTION 'host=publisher.example.com port=5439 dbname=source user=repuser password=secret'
PUBLICATION pub_orders;

-- Subscription with options
CREATE SUBSCRIPTION sub_reporting
CONNECTION 'host=primary.db port=5439 dbname=prod user=replica'
PUBLICATION pub_all
WITH (
    copy_data = true,
    create_slot = true,
    enabled = true,
    slot_name = 'sub_reporting_slot',
    synchronous_commit = 'off',
    two_phase = false
);

-- Subscription without initial copy
CREATE SUBSCRIPTION sub_incremental
CONNECTION 'host=source.db dbname=main user=replicator'
PUBLICATION pub_updates
WITH (copy_data = false);

-- Disabled subscription (enable manually later)
CREATE SUBSCRIPTION sub_standby
CONNECTION 'host=master.db dbname=primary user=replica'
PUBLICATION pub_main
WITH (enabled = false);
```

### Subscription Security

```sql
-- Create replication user on publisher
CREATE USER replication_user REPLICATION LOGIN PASSWORD 'SecurePass123!';
GRANT SELECT ON ALL TABLES IN SCHEMA public TO replication_user;

-- Connection string with SSL
CREATE SUBSCRIPTION sub_secure
CONNECTION 'host=publisher.db port=5439 dbname=source 
           user=replication_user password=SecurePass123!
           sslmode=require sslcert=client.crt sslkey=client.key'
PUBLICATION pub_sensitive;

-- Using connection URI
CREATE SUBSCRIPTION sub_uri
CONNECTION 'postgresql://repuser:password@publisher.db:5439/source?sslmode=require'
PUBLICATION pub_data;
```

### ALTER SUBSCRIPTION

```sql
-- Enable/disable subscription
ALTER SUBSCRIPTION sub_orders ENABLE;
ALTER SUBSCRIPTION sub_orders DISABLE;

-- Change connection
ALTER SUBSCRIPTION sub_orders 
CONNECTION 'host=new-publisher.db port=5439 dbname=source user=repuser';

-- Add/remove publications
ALTER SUBSCRIPTION sub_orders 
ADD PUBLICATION pub_customers, pub_products;

ALTER SUBSCRIPTION sub_orders 
DROP PUBLICATION pub_old;

-- Change subscription options
ALTER SUBSCRIPTION sub_orders 
SET (synchronous_commit = 'on');

-- Refresh publication
ALTER SUBSCRIPTION sub_orders 
REFRESH PUBLICATION;

-- Skip transactions (for conflict resolution)
ALTER SUBSCRIPTION sub_orders 
SKIP (lsn = '0/1234567');
```

### DROP SUBSCRIPTION

```sql
-- Drop subscription
DROP SUBSCRIPTION sub_old;

-- Drop if exists
DROP SUBSCRIPTION IF EXISTS sub_temp;

-- Drop with cascade
DROP SUBSCRIPTION sub_main CASCADE;
```

## Replication Patterns

### One-to-Many Distribution

```sql
-- Publisher: Central database
CREATE PUBLICATION pub_reference_data
FOR TABLE 
    products,
    categories,
    pricing,
    configurations
WITH (publish = 'insert, update, delete');

-- Subscriber 1: Regional server
CREATE SUBSCRIPTION sub_region_us
CONNECTION 'host=central.db dbname=main'
PUBLICATION pub_reference_data;

-- Subscriber 2: Regional server
CREATE SUBSCRIPTION sub_region_eu
CONNECTION 'host=central.db dbname=main'
PUBLICATION pub_reference_data;
```

### Selective Table Replication

```sql
-- Publisher: Replicate only active records
CREATE PUBLICATION pub_active_only
FOR TABLE 
    customers WHERE (status = 'active'),
    orders WHERE (created_at > CURRENT_DATE - INTERVAL '1 year'),
    products WHERE (discontinued = false);

-- Subscriber: Reporting database
CREATE SUBSCRIPTION sub_reporting_active
CONNECTION 'host=oltp.db dbname=transactional'
PUBLICATION pub_active_only
WITH (copy_data = true);
```

### Bidirectional Replication

```sql
-- Database A: Publish local changes
CREATE PUBLICATION pub_db_a
FOR TABLE shared_data
WHERE (source_db = 'A');

CREATE SUBSCRIPTION sub_from_b
CONNECTION 'host=db-b.example.com dbname=app'
PUBLICATION pub_db_b;

-- Database B: Publish local changes
CREATE PUBLICATION pub_db_b
FOR TABLE shared_data
WHERE (source_db = 'B');

CREATE SUBSCRIPTION sub_from_a
CONNECTION 'host=db-a.example.com dbname=app'
PUBLICATION pub_db_a;
```

### Sharded Data Collection

```sql
-- Shard 1: Publish shard data
CREATE PUBLICATION pub_shard_1
FOR TABLE customers WHERE (shard_id = 1);

-- Shard 2: Publish shard data
CREATE PUBLICATION pub_shard_2
FOR TABLE customers WHERE (shard_id = 2);

-- Central: Subscribe to all shards
CREATE SUBSCRIPTION sub_shard_1
CONNECTION 'host=shard1.db dbname=app'
PUBLICATION pub_shard_1;

CREATE SUBSCRIPTION sub_shard_2
CONNECTION 'host=shard2.db dbname=app'
PUBLICATION pub_shard_2;
```

## Monitoring Replication

### Publication Views

```sql
-- View publications
SELECT 
    pubname,
    pubowner::regrole AS owner,
    puballtables,
    pubinsert,
    pubupdate,
    pubdelete,
    pubtruncate
FROM pg_publication;

-- View publication tables
SELECT 
    pubname,
    schemaname,
    tablename,
    attnames AS columns,
    rowfilter
FROM pg_publication_tables
ORDER BY pubname, tablename;

-- Publication statistics
SELECT 
    pubname,
    COUNT(*) AS table_count,
    array_agg(tablename) AS tables
FROM pg_publication_tables
GROUP BY pubname;
```

### Subscription Views

```sql
-- View subscriptions
SELECT 
    subname,
    subowner::regrole AS owner,
    subenabled,
    subconninfo,
    subslotname,
    subsynccommit,
    subpublications
FROM pg_subscription;

-- Subscription status
SELECT 
    subname,
    pid,
    relid::regclass AS table_name,
    received_lsn,
    last_msg_send_time,
    last_msg_receipt_time,
    latest_end_lsn,
    latest_end_time
FROM pg_stat_subscription;

-- Replication lag
SELECT 
    subname,
    pg_size_pretty(pg_wal_lsn_diff(pg_current_wal_lsn(), received_lsn)) AS lag_bytes,
    EXTRACT(EPOCH FROM (now() - last_msg_receipt_time)) AS lag_seconds
FROM pg_stat_subscription
WHERE pid IS NOT NULL;
```

### Conflict Resolution

```sql
-- Monitor conflicts
SELECT 
    subname,
    relname,
    conflict_type,
    conflict_count,
    last_conflict_time
FROM pg_stat_subscription_conflicts;

-- Handle conflicts with triggers
CREATE OR REPLACE FUNCTION handle_replication_conflict()
RETURNS TRIGGER AS $$
BEGIN
    -- Log conflict
    INSERT INTO replication_conflicts (
        table_name,
        operation,
        local_data,
        remote_data,
        resolved_at
    ) VALUES (
        TG_TABLE_NAME,
        TG_OP,
        to_jsonb(OLD),
        to_jsonb(NEW),
        CURRENT_TIMESTAMP
    );
    
    -- Resolution strategy: Last write wins
    RETURN NEW;
    
    -- Alternative: First write wins
    -- RETURN OLD;
    
    -- Alternative: Merge
    -- NEW.updated_at = GREATEST(OLD.updated_at, NEW.updated_at);
    -- RETURN NEW;
END;
$$ LANGUAGE plpgsql;
```

## Performance Optimization

### Parallel Apply

```sql
-- Enable parallel apply workers
ALTER SUBSCRIPTION sub_large_tables
SET (streaming = 'parallel', num_parallel_apply_workers = 4);

-- Monitor parallel workers
SELECT 
    subname,
    pid AS leader_pid,
    array_agg(worker_pid) AS worker_pids
FROM pg_stat_subscription_workers
GROUP BY subname, pid;
```

### Batch Processing

```sql
-- Configure batch size
ALTER SUBSCRIPTION sub_bulk
SET (batch_size = 1000);

-- Temporary disable sync commit for bulk operations
ALTER SUBSCRIPTION sub_bulk
SET (synchronous_commit = 'off');

-- Re-enable after bulk load
ALTER SUBSCRIPTION sub_bulk
SET (synchronous_commit = 'on');
```

### Network Optimization

```sql
-- Compression for WAN replication
CREATE SUBSCRIPTION sub_remote
CONNECTION 'host=remote.db dbname=source 
           options=''-c wal_sender_compression=on'''
PUBLICATION pub_compressed;

-- Increase buffer sizes
ALTER SUBSCRIPTION sub_high_volume
SET (wal_receiver_buffer_size = '256MB');
```

## Maintenance Operations

### Resynchronization

```sql
-- Refresh subscription metadata
ALTER SUBSCRIPTION sub_main 
REFRESH PUBLICATION;

-- Resync specific table
ALTER SUBSCRIPTION sub_main
REFRESH PUBLICATION WITH (copy_data = true);

-- Full resynchronization
DROP SUBSCRIPTION sub_main;
CREATE SUBSCRIPTION sub_main
CONNECTION 'host=publisher.db dbname=source'
PUBLICATION pub_main
WITH (copy_data = true);
```

### Backup Considerations

```sql
-- Backup with publications
pg_dump --include-publication=pub_main source_db > backup.sql

-- Backup with subscriptions
pg_dump --include-subscription=sub_main target_db > backup.sql

-- Exclude replication objects
pg_dump --exclude-publication='*' --exclude-subscription='*' db > backup.sql
```

## Best Practices

1. **Security**
   - Use dedicated replication users
   - Implement SSL/TLS connections
   - Restrict network access
   - Monitor unauthorized access

2. **Performance**
   - Index foreign keys on subscriber
   - Monitor replication lag
   - Use appropriate batch sizes
   - Consider network bandwidth

3. **Reliability**
   - Implement conflict resolution
   - Monitor for errors
   - Plan for failover scenarios
   - Regular testing

4. **Maintenance**
   - Regular VACUUM on subscriber
   - Monitor disk space
   - Archive old data
   - Update statistics

## Implementation Details

**Parser** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_publication`: CREATE/ALTER/DROP PUBLICATION
- `parse_ddl_subscription`: CREATE/ALTER/DROP SUBSCRIPTION

**Replication Engine**:
- Logical decoding
- Change data capture
- Apply workers
- Conflict detection

**Code Anchors**:
- Publication parser: `src/engine/parser_ddl.cpp` (parse_ddl_publication)
- Subscription parser: `src/engine/parser_ddl.cpp` (parse_ddl_subscription)
- Replication logic: `src/engine/logical_replication.cpp`
- AST definitions: `include/scratchbird/engine/ast.h`

## See also

- [Foreign Data](./ddl-foreign-data.md) - Foreign data wrappers
- [Cluster](./ddl-cluster.md) - Cluster configuration
- [Database Links](./ddl-database-links.md) - Direct connections
- [Configuration](./configuration.md) - Replication settings
- [Performance](./explain-analyze.md) - Query optimization