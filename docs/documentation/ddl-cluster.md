### DDL: Cluster Configuration

**What it is**

Cluster configuration in ScratchBird manages distributed database deployments, including node management, data distribution, failover mechanisms, and load balancing. A cluster consists of multiple database nodes working together to provide high availability, scalability, and fault tolerance. Cluster DDL commands control node membership, service endpoints, replication topology, and distribution policies.

**Why it matters**

- **High Availability**: Automatic failover ensures continuous service
- **Scalability**: Distribute load across multiple nodes
- **Fault Tolerance**: Survive node failures without data loss
- **Geographic Distribution**: Place data close to users
- **Load Balancing**: Optimize resource utilization

**How to use it**

Define a cluster with CREATE CLUSTER, add nodes with CREATE CLUSTER NODE, configure services for different workloads, and set up automatic failover policies. Monitor cluster health through system views and manage topology changes with ALTER commands. The cluster coordinator handles routing, failover, and rebalancing automatically.

## Cluster Management

### CREATE CLUSTER

```sql
-- Basic cluster creation
CREATE CLUSTER production_cluster;

-- Cluster with configuration
CREATE CLUSTER ha_cluster
WITH (
    replication_factor = 3,
    consistency_level = 'quorum',
    failure_detection_time = '10 seconds',
    auto_failover = true
);

-- Geo-distributed cluster
CREATE CLUSTER global_cluster
WITH (
    topology = 'multi_region',
    regions = 'us-east,us-west,eu-central',
    cross_region_replication = 'async'
);

-- Sharded cluster
CREATE CLUSTER sharded_cluster
WITH (
    sharding_method = 'hash',
    shard_count = 16,
    rebalance_threshold = 0.1
);
```

### ALTER CLUSTER

```sql
-- Change cluster settings
ALTER CLUSTER production_cluster
SET replication_factor = 3;

-- Enable automatic failover
ALTER CLUSTER production_cluster
SET auto_failover = true;

-- Adjust failure detection
ALTER CLUSTER production_cluster
SET failure_detection_time = '5 seconds';

-- Change consistency level
ALTER CLUSTER production_cluster
SET consistency_level = 'strong';

-- Rename cluster
ALTER CLUSTER old_cluster RENAME TO new_cluster;
```

### DROP CLUSTER

```sql
-- Drop cluster (requires all nodes offline)
DROP CLUSTER obsolete_cluster;

-- Drop if exists
DROP CLUSTER IF EXISTS test_cluster;

-- Force drop (emergency only)
DROP CLUSTER failed_cluster CASCADE;
```

## Node Management

### CREATE CLUSTER NODE

```sql
-- Add primary node
CREATE CLUSTER NODE node1
IN CLUSTER production_cluster
WITH (
    host = '192.168.1.10',
    port = 5439,
    role = 'primary',
    zone = 'us-east-1a',
    weight = 100
);

-- Add replica nodes
CREATE CLUSTER NODE node2
IN CLUSTER production_cluster
WITH (
    host = '192.168.1.11',
    port = 5439,
    role = 'replica',
    zone = 'us-east-1b',
    weight = 100,
    sync_priority = 1
);

CREATE CLUSTER NODE node3
IN CLUSTER production_cluster
WITH (
    host = '192.168.1.12',
    port = 5439,
    role = 'replica',
    zone = 'us-east-1c',
    weight = 100,
    sync_priority = 2
);

-- Add witness node (for quorum)
CREATE CLUSTER NODE witness
IN CLUSTER production_cluster
WITH (
    host = '192.168.1.20',
    port = 5439,
    role = 'witness',
    zone = 'us-east-1d'
);

-- Add analytics node
CREATE CLUSTER NODE analytics_node
IN CLUSTER production_cluster
WITH (
    host = '192.168.1.30',
    port = 5439,
    role = 'read_only',
    zone = 'us-east-1a',
    workload = 'analytics'
);
```

### ALTER CLUSTER NODE

```sql
-- Promote replica to primary
ALTER CLUSTER NODE node2
IN CLUSTER production_cluster
SET role = 'primary';

-- Change node weight (for load balancing)
ALTER CLUSTER NODE node3
SET weight = 50;

-- Mark node for maintenance
ALTER CLUSTER NODE node1
SET status = 'maintenance';

-- Change sync priority
ALTER CLUSTER NODE node2
SET sync_priority = 1;

-- Update node address
ALTER CLUSTER NODE node1
SET host = '192.168.1.100', port = 5440;
```

### DROP CLUSTER NODE

```sql
-- Remove node from cluster
DROP CLUSTER NODE node3
FROM CLUSTER production_cluster;

-- Force removal (node unreachable)
DROP CLUSTER NODE failed_node
FROM CLUSTER production_cluster
CASCADE;
```

## Service Configuration

### CREATE CLUSTER SERVICE

```sql
-- Read-write service (routes to primary)
CREATE CLUSTER SERVICE read_write
IN CLUSTER production_cluster
WITH (
    target_role = 'primary',
    port = 5439,
    load_balancing = 'off'
);

-- Read-only service (load balanced across replicas)
CREATE CLUSTER SERVICE read_only
IN CLUSTER production_cluster
WITH (
    target_role = 'replica',
    port = 5440,
    load_balancing = 'round_robin',
    max_connections_per_node = 100
);

-- Analytics service
CREATE CLUSTER SERVICE analytics
IN CLUSTER production_cluster
WITH (
    target_nodes = 'analytics_node',
    port = 5441,
    query_timeout = '30 minutes',
    work_mem = '1GB'
);

-- Geographic routing service
CREATE CLUSTER SERVICE geo_routing
IN CLUSTER global_cluster
WITH (
    routing = 'nearest',
    fallback = 'any',
    latency_threshold = '50ms'
);
```

### Service Load Balancing

```sql
-- Round-robin load balancing
ALTER CLUSTER SERVICE read_only
SET load_balancing = 'round_robin';

-- Least connections
ALTER CLUSTER SERVICE read_only
SET load_balancing = 'least_conn';

-- Weighted distribution
ALTER CLUSTER SERVICE read_only
SET load_balancing = 'weighted';

-- Session affinity
ALTER CLUSTER SERVICE app_service
SET session_affinity = true,
    affinity_timeout = '1 hour';
```

## Failover Configuration

### Automatic Failover

```sql
-- Configure automatic failover
ALTER CLUSTER production_cluster
SET (
    auto_failover = true,
    failover_timeout = '30 seconds',
    min_sync_replicas = 1,
    max_replication_lag = '10MB'
);

-- Failover priorities
ALTER CLUSTER NODE node2 SET failover_priority = 100;
ALTER CLUSTER NODE node3 SET failover_priority = 90;
ALTER CLUSTER NODE node4 SET failover_priority = 80;

-- Prevent specific node from becoming primary
ALTER CLUSTER NODE analytics_node 
SET failover_priority = 0;
```

### Manual Failover

```sql
-- Planned failover
ALTER CLUSTER production_cluster
FAILOVER TO node2;

-- Force failover (emergency)
ALTER CLUSTER production_cluster
FORCE FAILOVER TO node3;

-- Switchover (graceful role swap)
ALTER CLUSTER production_cluster
SWITCHOVER FROM node1 TO node2;
```

## Data Distribution

### Sharding Configuration

```sql
-- Hash-based sharding
CREATE CLUSTER sharded_cluster
WITH (sharding_method = 'hash');

-- Distribute table across shards
ALTER TABLE large_table
DISTRIBUTE BY HASH(customer_id)
TO CLUSTER sharded_cluster;

-- Range-based sharding
CREATE CLUSTER range_cluster
WITH (sharding_method = 'range');

ALTER TABLE time_series
DISTRIBUTE BY RANGE(timestamp)
TO CLUSTER range_cluster
WITH (
    shard_1 = '[2024-01-01, 2024-04-01)',
    shard_2 = '[2024-04-01, 2024-07-01)',
    shard_3 = '[2024-07-01, 2024-10-01)',
    shard_4 = '[2024-10-01, 2025-01-01)'
);

-- List-based sharding
ALTER TABLE regional_data
DISTRIBUTE BY LIST(region)
TO CLUSTER regional_cluster
WITH (
    shard_us = ('US', 'CA', 'MX'),
    shard_eu = ('UK', 'FR', 'DE', 'IT'),
    shard_asia = ('JP', 'CN', 'IN', 'KR')
);
```

### Rebalancing

```sql
-- Manual rebalance
ALTER CLUSTER sharded_cluster REBALANCE;

-- Rebalance specific table
ALTER TABLE large_table REBALANCE
IN CLUSTER sharded_cluster;

-- Configure auto-rebalancing
ALTER CLUSTER sharded_cluster
SET (
    auto_rebalance = true,
    rebalance_threshold = 0.2,  -- 20% imbalance triggers rebalance
    rebalance_window = '02:00-04:00'  -- Maintenance window
);
```

## Monitoring

### Cluster Status

```sql
-- View cluster configuration
SELECT 
    cluster_name,
    replication_factor,
    consistency_level,
    auto_failover,
    node_count,
    status
FROM pg_clusters;

-- Node status
SELECT 
    cluster_name,
    node_name,
    host,
    port,
    role,
    status,
    last_heartbeat,
    replication_lag
FROM pg_cluster_nodes
ORDER BY cluster_name, role, node_name;

-- Service endpoints
SELECT 
    cluster_name,
    service_name,
    port,
    target_role,
    load_balancing,
    active_connections
FROM pg_cluster_services;
```

### Health Monitoring

```sql
-- Cluster health summary
SELECT 
    cluster_name,
    total_nodes,
    healthy_nodes,
    unhealthy_nodes,
    (healthy_nodes::float / total_nodes) * 100 AS health_percentage,
    last_failover,
    failover_count
FROM pg_cluster_health;

-- Replication lag monitoring
SELECT 
    cluster_name,
    node_name,
    role,
    pg_size_pretty(replication_lag_bytes) AS lag_size,
    replication_lag_time,
    CASE 
        WHEN replication_lag_time > interval '1 minute' THEN 'CRITICAL'
        WHEN replication_lag_time > interval '10 seconds' THEN 'WARNING'
        ELSE 'OK'
    END AS lag_status
FROM pg_cluster_replication
WHERE role = 'replica'
ORDER BY replication_lag_time DESC;

-- Connection distribution
SELECT 
    service_name,
    node_name,
    connection_count,
    ROUND(100.0 * connection_count / SUM(connection_count) OVER (PARTITION BY service_name), 2) AS percentage
FROM pg_cluster_connections
ORDER BY service_name, connection_count DESC;
```

## Topology Patterns

### Primary-Replica

```sql
-- Simple primary-replica setup
CREATE CLUSTER pr_cluster;

CREATE CLUSTER NODE primary IN CLUSTER pr_cluster
WITH (host = 'db1.example.com', role = 'primary');

CREATE CLUSTER NODE replica1 IN CLUSTER pr_cluster
WITH (host = 'db2.example.com', role = 'replica', sync_priority = 1);

CREATE CLUSTER NODE replica2 IN CLUSTER pr_cluster
WITH (host = 'db3.example.com', role = 'replica', sync_priority = 2);
```

### Multi-Master

```sql
-- Multi-master configuration
CREATE CLUSTER mm_cluster
WITH (
    topology = 'multi_master',
    conflict_resolution = 'last_write_wins'
);

CREATE CLUSTER NODE master1 IN CLUSTER mm_cluster
WITH (host = 'db1.example.com', role = 'master');

CREATE CLUSTER NODE master2 IN CLUSTER mm_cluster
WITH (host = 'db2.example.com', role = 'master');

-- Configure conflict resolution
ALTER CLUSTER mm_cluster
SET conflict_resolution_function = 'custom_merge_function';
```

### Federation

```sql
-- Federated cluster
CREATE CLUSTER federation
WITH (topology = 'federated');

-- Regional subclusters
CREATE CLUSTER SUBCLUSTER us_cluster IN federation
WITH (region = 'us', autonomous = true);

CREATE CLUSTER SUBCLUSTER eu_cluster IN federation
WITH (region = 'eu', autonomous = true);

CREATE CLUSTER SUBCLUSTER asia_cluster IN federation
WITH (region = 'asia', autonomous = true);

-- Cross-region replication
ALTER CLUSTER federation
SET cross_region_replication = 'async',
    cross_region_lag_threshold = '5 seconds';
```

## Disaster Recovery

### Backup Strategy

```sql
-- Configure cluster-wide backup
ALTER CLUSTER production_cluster
SET (
    backup_enabled = true,
    backup_schedule = '0 2 * * *',  -- Daily at 2 AM
    backup_retention = '30 days',
    backup_location = 's3://backups/cluster/'
);

-- Point-in-time recovery
ALTER CLUSTER production_cluster
SET (
    archive_mode = 'on',
    archive_command = 'aws s3 cp %p s3://wal-archive/%f',
    archive_timeout = '5min'
);
```

### Disaster Recovery Testing

```sql
-- Create DR cluster
CREATE CLUSTER dr_cluster
AS REPLICA OF production_cluster
WITH (
    location = 'us-west',
    lag_threshold = '1 hour',
    promotion_mode = 'manual'
);

-- Test failover to DR
ALTER CLUSTER dr_cluster PROMOTE;

-- Failback to production
ALTER CLUSTER production_cluster REBUILD FROM dr_cluster;
```

## Best Practices

1. **Node Placement**
   - Distribute across availability zones
   - Consider network latency
   - Balance resources

2. **Monitoring**
   - Set up alerting for node failures
   - Monitor replication lag
   - Track resource usage

3. **Maintenance**
   - Schedule maintenance windows
   - Perform rolling updates
   - Test failover regularly

4. **Security**
   - Encrypt inter-node communication
   - Use private networks
   - Implement access controls

## Implementation Details

**Parser** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_cluster`: CREATE/ALTER/DROP CLUSTER
- `parse_ddl_cluster_node`: Node management
- `parse_ddl_cluster_service`: Service configuration

**Cluster Manager**:
- Node discovery and health checks
- Failover coordination
- Load balancing
- Replication management

**Code Anchors**:
- Cluster parser: `src/engine/parser_ddl.cpp` (parse_ddl_cluster)
- Node parser: `src/engine/parser_ddl.cpp` (parse_ddl_cluster_node)
- Service parser: `src/engine/parser_ddl.cpp` (parse_ddl_cluster_service)
- Cluster manager: `src/engine/cluster_manager.cpp`
- AST definitions: `include/scratchbird/engine/ast.h`

## See also

- [Publication/Subscription](./ddl-publication-subscription.md) - Logical replication
- [Foreign Data](./ddl-foreign-data.md) - Cross-database queries
- [Configuration](./configuration.md) - Cluster settings
- [High Availability](./missing-and-future.md) - HA patterns
- [Installation](./installation.md) - Cluster setup