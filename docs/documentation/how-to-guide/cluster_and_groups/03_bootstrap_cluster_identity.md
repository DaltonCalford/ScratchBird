# Cluster Bootstrap and Setup

[Cluster and Groups README](../README.md)

## Synopsis

Set up a ScratchBird Cluster for high availability and automatic sharding.

## Prerequisites

- 3+ nodes (for T-of-N consensus)
- Network connectivity between all nodes
- Synchronized clocks (NTP)
- Shared storage NOT required (distributed)

## Architecture

```
        Client Connections
                │
    ┌───────────┼───────────┐
    │           │           │
   Node 1     Node 2     Node 3
   (Shard A)  (Shard B)  (Shard C)
    │           │           │
    └───────────┼───────────┘
                │
         Raft Consensus
         (T=2 of N=3)
```

## Step 1: Prepare Nodes

### On Each Node

```bash
# Install ScratchBird on all nodes
# Node 1: 192.168.1.11
# Node 2: 192.168.1.12
# Node 3: 192.168.1.13

# Configure node identification
# On /etc/scratchbird/scratchbird.conf for each node:
```

Node 1:
```ini
cluster.node_id = 1
cluster.listen_address = '192.168.1.11'
cluster.raft_port = 3093
cluster.data_port = 3094
```

Node 2:
```ini
cluster.node_id = 2
cluster.listen_address = '192.168.1.12'
cluster.raft_port = 3093
cluster.data_port = 3094
```

Node 3:
```ini
cluster.node_id = 3
cluster.listen_address = '192.168.1.13'
cluster.raft_port = 3093
cluster.data_port = 3094
```

## Step 2: Bootstrap First Node

On Node 1:

```bash
# Initialize cluster
sb_cluster init \
    --node-id=1 \
    --listen-address=192.168.1.11 \
    --raft-port=3093 \
    --data-port=3094 \
    --cluster-name=prod_cluster \
    --consensus-threshold=2

# Start node
sb_ctl start
```

## Step 3: Join Additional Nodes

On Node 2:

```bash
# Join existing cluster
sb_cluster join \
    --node-id=2 \
    --listen-address=192.168.1.12 \
    --raft-port=3093 \
    --data-port=3094 \
    --bootstrap-node=192.168.1.11:3093

sb_ctl start
```

On Node 3:

```bash
sb_cluster join \
    --node-id=3 \
    --listen-address=192.168.1.13 \
    --raft-port=3093 \
    --data-port=3094 \
    --bootstrap-node=192.168.1.11:3093

sb_ctl start
```

## Step 4: Verify Cluster

```sql
-- Check cluster status
SELECT * FROM sb_cluster_nodes;

-- Expected output:
-- node_id | address       | status  | role
-- 1       | 192.168.1.11  | active  | leader
-- 2       | 192.168.1.12  | active  | follower
-- 3       | 192.168.1.13  | active  | follower

-- Check consensus
SELECT * FROM sb_cluster_consensus;

-- Check shard distribution
SELECT * FROM sb_cluster_shards;
```

## Step 5: Configure Sharding

```sql
-- Enable automatic sharding
ALTER CLUSTER prod_cluster 
    SET SHARDING = 'automatic';

-- Set shard count (256 default)
ALTER CLUSTER prod_cluster 
    SET SHARD_COUNT = 256;

-- Set replication factor
ALTER CLUSTER prod_cluster 
    SET REPLICATION_FACTOR = 3;
```

## Step 6: Create Database

```sql
-- Create sharded database
CREATE DATABASE myapp
    WITH CLUSTER = 'prod_cluster';

-- Tables created in this database will be automatically sharded
CREATE TABLE events (
    id UUID DEFAULT gen_random_uuid(),
    event_time TIMESTAMPTZ,
    data JSONB
) PARTITION BY HASH (id);
```

## Security Configuration

```sql
-- Cluster uses T-of-N authentication
-- No single node has complete security info

-- Add user (distributed to all nodes)
CREATE CLUSTER USER app_admin 
    WITH PASSWORD 'secure_password';

-- Grant cluster-wide privileges
GRANT CLUSTER ADMIN TO app_admin;
```

## Monitoring

```bash
# Check cluster health
sb_cluster status

# Output:
# Cluster: prod_cluster
# Nodes: 3/3 active
# Consensus: healthy (T=2)
# Shards: 256 total, 85.3 avg per node
```

## Failover Testing

```bash
# Simulate node failure (on Node 1)
sb_ctl stop -D /var/lib/scratchbird/data

# Check that cluster continues
# Leader election should occur within 3 seconds

# Restart node
sb_ctl start
# Node rejoins as follower
```

## Maintenance

### Adding Nodes

```bash
# New node 4
sb_cluster join \
    --node-id=4 \
    --listen-address=192.168.1.14 \
    --bootstrap-node=192.168.1.11:3093

# Rebalance shards
sb_cluster rebalance
```

### Removing Nodes

```bash
# Graceful removal
sb_cluster remove-node --node-id=4

# Emergency removal (node dead)
sb_cluster force-remove-node --node-id=4 --from-node=1
```

## Limitations

- Minimum 3 nodes for production
- No heterogeneous node sizes (yet)
- SB-only (no legacy engine support)
- Cross-shard transactions have overhead

## See Also

- [Group Setup](01_group_topology_setup.md)
- [Replication and Channels](04_configure_replication_channels.md)
- [Failover and Recovery](../cluster_and_groups/06_run_cluster_failure_drills.md)
