# Replication Channel Configuration

[Cluster and Groups README](../README.md)

## Synopsis

Configure logical replication between databases or cluster nodes.

## Types of Replication

| Type | Use Case | Direction |
|------|----------|-----------|
| Streaming | Real-time sync | Primary → Replica |
| Logical | Selective sync | Bidirectional |
| Snapshot | Initial load | One-time |

## Basic Replication Setup

### Group Replication (Database to Database)

```sql
-- Create replication channel
CREATE REPLICATION CHANNEL sales_to_analytics
    FROM !:prod.sales
    TO !:prod.analytics
    WITH (
        PUBLICATION = 'public.sales_data',
        FILTER = 'tables = {orders, order_items, customers}'
    );

-- Start replication
ALTER REPLICATION CHANNEL sales_to_analytics START;

-- Monitor
SELECT * FROM sb_replication_status 
WHERE channel_name = 'sales_to_analytics';
```

### Cluster Replication (Node to Node)

```sql
-- Configure cluster-wide replication
ALTER CLUSTER prod_cluster 
    SET REPLICATION_FACTOR = 3;

-- Each shard automatically replicated to 3 nodes
```

## Publication Configuration

```sql
-- Create publication (on source)
CREATE PUBLICATION sales_publication
    FOR TABLE orders, order_items, customers
    WITH (publish = 'insert, update, delete');

-- Partial replication with row filter
CREATE PUBLICATION active_orders_only
    FOR TABLE orders WHERE (status = 'active');

-- Column filter
CREATE PUBLICATION orders_redacted
    FOR TABLE orders (id, customer_id, total)  -- Excludes internal_notes
```

## Subscription Configuration

```sql
-- Create subscription (on target)
CREATE SUBSCRIPTION sales_subscription
    CONNECTION 'host=source.internal dbname=sales user=replicator'
    PUBLICATION sales_publication
    WITH (slot_name = 'sales_slot', create_slot = true);

-- Enable subscription
ALTER SUBSCRIPTION sales_subscription ENABLE;
```

## Conflict Resolution

```sql
-- Configure conflict handler
ALTER REPLICATION CHANNEL bidirectional_sync
    SET CONFLICT_RESOLUTION = 'timestamp';

-- Options:
-- 'timestamp' - Most recent wins
-- 'source' - Source always wins
-- 'target' - Target always wins
-- 'manual' - Log conflicts for review
```

## Monitoring Replication

```sql
-- Replication lag
SELECT 
    channel_name,
    source_node,
    target_node,
    last_received_lsn,
    last_applied_lsn,
    pg_size_pretty(pg_wal_lsn_diff(last_received_lsn, last_applied_lsn)) as lag
FROM sb_replication_status;

-- Replication errors
SELECT * FROM sb_replication_errors 
WHERE resolved = false;
```

## Failover Handling

### Automatic Failover

```sql
-- Configure automatic failover
ALTER REPLICATION CHANNEL primary_replica
    SET FAILOVER = 'automatic'
    SET FAILOVER_TIMEOUT = '30 seconds';

-- When primary fails, replica promoted automatically
```

### Manual Failover

```sql
-- 1. Stop replication
ALTER REPLICATION CHANNEL primary_replica STOP;

-- 2. Promote replica
ALTER DATABASE !:prod.sales SET PRIMARY = !:dr.sales;

-- 3. Update application connections
-- Point to new primary
```

## Cross-Region Replication

```sql
-- Multi-region setup
CREATE REPLICATION CHANNEL us_to_eu
    FROM !:us_prod.sales
    TO !:eu_prod.sales
    WITH (
        COMPRESSION = 'zstd',
        SSL_MODE = 'require',
        BATCH_SIZE = '1000'
    );

-- Async replication for long distance
ALTER REPLICATION CHANNEL us_to_eu 
    SET SYNC_MODE = 'async';
```

## Replication Slots

```sql
-- Create replication slot (prevents WAL removal)
SELECT * FROM pg_create_logical_replication_slot('my_slot', 'pgoutput');

-- Monitor slot lag
SELECT 
    slot_name,
    pg_size_pretty(pg_wal_lsn_diff(pg_current_wal_lsn(), restart_lsn)) as lag
FROM pg_replication_slots;

-- Drop slot when done
SELECT pg_drop_replication_slot('my_slot');
```

## Troubleshooting

### Replication Lag

```sql
-- Check replication status
SELECT * FROM sb_replication_status;

-- Common causes:
-- 1. Network issues
-- 2. Large transaction
-- 3. Slow apply on target
-- 4. Lock contention

-- Solutions:
-- - Increase batch size
-- - Add indexes on target
-- - Parallel apply workers
```

### Replication Errors

```bash
# View logs
tail -f /var/log/scratchbird/replication.log

# Restart replication
sb_ctl reload
ALTER REPLICATION CHANNEL name START;
```

## Best Practices

1. **Monitor lag** - Alert if > 30 seconds
2. **Test failover** - Regular drills
3. **Use slots** - Prevent data loss
4. **Compress** - For WAN replication
5. **SSL** - Always encrypt
6. **Dedicated user** - Limited privileges

## See Also

- [Group Topology](01_group_topology_setup.md)
- [Cluster Setup](03_bootstrap_cluster_identity.md)
- [DR Strategy](../../disaster_recovery_and_continuity/dr_strategy_and_rto_rpo/README.md)
