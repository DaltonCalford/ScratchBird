# Beta Replication Migration and Operations Guide

**Document:** 08_MIGRATION_OPERATIONS.md
**Status:** BETA SPECIFICATION
**Version:** 1.0
**Date:** 2026-01-09
**Authority:** Chief Architect

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Migration from Alpha to Beta](#migration-from-alpha-to-beta)
3. [Operations Guide](#operations-guide)
4. [Observability](#observability)
5. [Capacity Planning](#capacity-planning)
6. [Disaster Recovery](#disaster-recovery)
7. [Performance Tuning](#performance-tuning)
8. [Troubleshooting](#troubleshooting)
9. [Security Operations](#security-operations)
10. [Rollback Procedures](#rollback-procedures)

---

## Executive Summary

### Purpose

This document provides operational guidance for deploying, monitoring, and maintaining ScratchBird Beta replication features (UUIDv8-HLC, leaderless quorum, schema colocation, time-partitioned Merkle forests, distributed MGA). It covers migration from Alpha, day-to-day operations, observability, capacity planning, disaster recovery, and troubleshooting.

### Target Audience

- **Database Administrators**: Cluster deployment, configuration, monitoring
- **DevOps Engineers**: CI/CD, infrastructure automation, disaster recovery
- **Developers**: Troubleshooting application-level issues, performance tuning

### Key Topics

| Topic | Focus | Audience |
|-------|-------|----------|
| Migration | Alpha → Beta upgrade | DBAs, DevOps |
| Operations | Monitoring, maintenance | DBAs |
| Observability | Metrics, logs, traces | DBAs, DevOps |
| Capacity Planning | Sizing, scaling | DBAs, DevOps |
| Disaster Recovery | Backups, failover | DBAs, DevOps |
| Performance Tuning | Configuration optimization | DBAs, Developers |
| Troubleshooting | Common issues, resolutions | DBAs, Developers |

---

## Migration from Alpha to Beta

### Pre-Migration Checklist

**Verify Alpha Completion**:
- [ ] All Alpha parser remediation tasks (P-001 to P-010) complete
- [ ] Alpha test suite passing (all integration tests green)
- [ ] No known P0/P1 bugs in Alpha

**Backup Existing Data**:
- [ ] Full database backup (all shards)
- [ ] Export catalog schema (`sys_tables`, `sys_columns`, etc.)
- [ ] Document current cluster topology (node IDs, shard assignments)

**Review Beta Requirements**:
- [ ] Read `/docs/specifications/beta_requirements/replication/uuidv7-optimized/00_BETA_REPLICATION_INDEX.md`
- [ ] Understand new features (leaderless quorum, schema colocation, etc.)
- [ ] Plan table-level replication modes (which tables need Mode 3?)

### Migration Strategy

**Option 1: In-Place Upgrade** (Recommended)
- Upgrade ScratchBird binaries (Alpha → Beta)
- Run migration scripts (add catalog columns, create Merkle forests)
- Tables default to UUIDv7 + Mode 1 replication (no breaking changes)
- Opt-in to Beta features per table (Mode 3, UUIDv8, partition keys)

**Option 2: Blue-Green Deployment**
- Set up new Beta cluster (parallel to Alpha)
- Replicate data from Alpha to Beta (logical replication)
- Switch applications to Beta cluster (cutover)
- Decommission Alpha cluster after validation

**Recommended**: Option 1 (in-place upgrade, backward compatible)

### Migration Steps (In-Place Upgrade)

#### Step 1: Upgrade Binaries (Week 1, Phase 6)

```bash
# Stop ScratchBird service on all nodes
sudo systemctl stop scratchbird

# Backup Alpha binaries
sudo cp /usr/local/bin/scratchbird /usr/local/bin/scratchbird_alpha_backup

# Install Beta binaries
sudo wget https://releases.scratchbird.com/beta/scratchbird_beta_v1.0.tar.gz
sudo tar -xzf scratchbird_beta_v1.0.tar.gz -C /usr/local/bin/

# Verify version
/usr/local/bin/scratchbird --version
# Expected: ScratchBird Beta v1.0.0 (2026-01-09)
```

#### Step 2: Run Migration Scripts (Week 1, Phase 6)

```bash
# Run catalog migration (add replication config columns)
/usr/local/bin/scratchbird_migrate --from=alpha --to=beta

# Migration script adds:
#   ALTER TABLE sys_tables ADD COLUMN replication_mode VARCHAR(20) DEFAULT 'primary_replica';
#   ALTER TABLE sys_tables ADD COLUMN replication_factor INTEGER DEFAULT 1;
#   ALTER TABLE sys_tables ADD COLUMN write_quorum INTEGER DEFAULT 1;
#   ALTER TABLE sys_tables ADD COLUMN read_quorum INTEGER DEFAULT 1;
#   ALTER TABLE sys_tables ADD COLUMN uuid_type VARCHAR(10) DEFAULT 'v7';
#   ALTER TABLE sys_tables ADD COLUMN partition_key_columns TEXT DEFAULT NULL;
```

#### Step 3: Start Beta Cluster (Week 1, Phase 6)

```bash
# Start ScratchBird service on all nodes
sudo systemctl start scratchbird

# Verify cluster membership
scratchbird_cli --command "SHOW CLUSTER MEMBERS"
# Expected: All nodes join cluster, gossip protocol operational

# Verify catalog migration
scratchbird_cli --command "SELECT table_name, replication_mode, uuid_type FROM sys_tables LIMIT 10"
# Expected: All tables default to replication_mode='primary_replica', uuid_type='v7'
```

#### Step 4: Opt-In to Beta Features (Week 2, Phase 6)

**Enable Mode 3 for IoT table**:

```sql
-- Alter existing table to use leaderless quorum
ALTER TABLE sensor_data SET replication_mode = 'leaderless_quorum';
ALTER TABLE sensor_data SET replication_factor = 3;
ALTER TABLE sensor_data SET write_quorum = 2;
ALTER TABLE sensor_data SET read_quorum = 2;
ALTER TABLE sensor_data SET uuid_type = 'v8_hlc';

-- Verify configuration
SELECT table_name, replication_mode, write_quorum, read_quorum, uuid_type
FROM sys_tables
WHERE table_name = 'sensor_data';
-- Expected: replication_mode='leaderless_quorum', uuid_type='v8_hlc'
```

**Enable Schema Colocation**:

```sql
-- Alter orders table to partition by user_id (colocate with users)
ALTER TABLE orders SET partition_key_columns = 'user_id';

-- Verify colocation
SELECT table_name, partition_key_columns
FROM sys_tables
WHERE table_name IN ('users', 'orders');
-- Expected: Both tables partition by user_id
```

#### Step 5: Verify Beta Features (Week 2, Phase 6)

**Test Mode 3 Write**:

```sql
-- Insert into Mode 3 table
INSERT INTO sensor_data (sensor_id, value) VALUES (gen_uuidv8_hlc(), 42.5);

-- Verify replication (query all 3 replicas)
-- Node A:
SELECT * FROM sensor_data WHERE sensor_id = <uuid>;
-- Node B:
SELECT * FROM sensor_data WHERE sensor_id = <uuid>;
-- Node C:
SELECT * FROM sensor_data WHERE sensor_id = <uuid>;

-- Expected: All 3 nodes have the row (W=2 quorum succeeded)
```

**Test Colocated Transaction**:

```sql
-- Multi-table transaction (users + orders, both partition by user_id)
BEGIN TRANSACTION;
INSERT INTO users (user_id, name) VALUES ('user_123', 'Alice');
INSERT INTO orders (order_id, user_id, amount) VALUES (gen_uuidv8_hlc(), 'user_123', 100.0);
COMMIT;

-- Verify single-shard execution (check logs)
-- Expected: Transaction executed on single shard (no 2PC)
```

### Migration Rollback

**If Migration Fails**:

```bash
# Stop Beta cluster
sudo systemctl stop scratchbird

# Restore Alpha binaries
sudo cp /usr/local/bin/scratchbird_alpha_backup /usr/local/bin/scratchbird

# Restore database from backup
scratchbird_restore --backup=/var/backups/scratchbird_alpha_20260109.tar.gz

# Start Alpha cluster
sudo systemctl start scratchbird
```

---

## Operations Guide

### Daily Operations

**Monitoring Dashboard**:
- Check cluster health (all nodes online)
- Monitor replication lag (< 100ms same DC, < 1s cross-DC)
- Review anti-entropy status (divergence detected, repairs completed)
- Check disk usage (Merkle forest metadata, hint logs)

**Weekly Maintenance**:
- Review slow query log (identify hot shards)
- Analyze anti-entropy reports (detect recurring divergence)
- Rotate logs (`/var/log/scratchbird/*.log`)
- Verify backup integrity (restore test)

**Monthly Tasks**:
- Review capacity planning (disk, CPU, memory usage trends)
- Update monitoring thresholds (based on observed workload)
- Patch ScratchBird (security updates, bug fixes)

### Configuration Management

**Cluster Configuration** (`/etc/scratchbird/cluster.yaml`):

```yaml
cluster:
  name: scratchbird_prod
  nodes:
    - id: 1
      address: node1.scratchbird.local:5432
      datacenter: us-east-1a
    - id: 2
      address: node2.scratchbird.local:5432
      datacenter: us-east-1b
    - id: 3
      address: node3.scratchbird.local:5432
      datacenter: us-east-1c

replication:
  default_mode: primary_replica
  default_factor: 2
  write_timeout_ms: 5000
  read_timeout_ms: 3000
  hint_expiration_days: 7

anti_entropy:
  interval_ms: 60000
  merkle_tree_window_hours: 1
  sealing_grace_period_minutes: 10

gossip:
  interval_ms: 10000
  fanout: 5
  heartbeat_timeout_ms: 30000
```

**Table Configuration** (via SQL):

```sql
-- Show table replication config
SELECT table_name, replication_mode, replication_factor, write_quorum, read_quorum
FROM sys_tables;

-- Update table config
ALTER TABLE sensor_data SET write_quorum = 3;  -- Increase durability
ALTER TABLE logs SET replication_factor = 5;   -- Increase redundancy
```

### Service Management

**Start/Stop Cluster**:

```bash
# Start all nodes
for node in node1 node2 node3; do
    ssh $node "sudo systemctl start scratchbird"
done

# Stop all nodes (graceful shutdown)
for node in node1 node2 node3; do
    ssh $node "sudo systemctl stop scratchbird"
done

# Restart node (rolling restart, no downtime)
ssh node1 "sudo systemctl restart scratchbird"
# Wait for node to rejoin cluster
sleep 30
ssh node2 "sudo systemctl restart scratchbird"
sleep 30
ssh node3 "sudo systemctl restart scratchbird"
```

**Add/Remove Nodes**:

```bash
# Add new node (Node 4)
# 1. Install ScratchBird on new node
ssh node4 "sudo apt install scratchbird-beta"

# 2. Configure node ID in cluster.yaml
# (Edit /etc/scratchbird/cluster.yaml, add node4)

# 3. Start new node
ssh node4 "sudo systemctl start scratchbird"

# 4. Node joins via gossip, receives shard assignments

# Remove node (Node 3)
# 1. Drain shards from node (migrate to other nodes)
scratchbird_cli --command "DRAIN NODE 3"

# 2. Wait for drain to complete (check shard map)
scratchbird_cli --command "SHOW SHARD MAP"

# 3. Stop node
ssh node3 "sudo systemctl stop scratchbird"

# 4. Remove from cluster.yaml
```

---

## Observability

### Metrics

**Prometheus Metrics** (exported on `:9090/metrics`):

```prometheus
# Replication lag
scratchbird_replication_lag_ms{shard_id="001", replica="node2"} 15

# Write throughput
scratchbird_write_tps{shard_id="001", mode="leaderless_quorum"} 52340

# Read latency (p99)
scratchbird_read_latency_p99_ms{shard_id="001", mode="leaderless_quorum"} 18

# Anti-entropy overhead
scratchbird_anti_entropy_cpu_percent{shard_id="001"} 1.8

# Hint log size
scratchbird_hint_log_size_bytes{target_node="node3"} 1048576

# HLC counter (detect clock skew)
scratchbird_hlc_counter{node_id="1"} 42

# Quorum failures
scratchbird_quorum_write_failures_total{shard_id="001"} 3
```

**Grafana Dashboard** (recommended panels):
- Cluster health (nodes online, shard distribution)
- Replication lag (per shard)
- Write/read throughput (per shard)
- Anti-entropy status (divergences detected, repairs completed)
- HLC counter histogram (detect clock skew outliers)

### Logging

**Log Levels**:
- `ERROR`: Critical issues (quorum not achieved, node failures)
- `WARNING`: Non-critical issues (hint delivery failed, clock skew detected)
- `INFO`: Operational events (node join/leave, shard migration)
- `DEBUG`: Verbose (replication messages, Merkle tree comparisons)

**Log Files**:
- `/var/log/scratchbird/scratchbird.log` - Main log
- `/var/log/scratchbird/replication.log` - Replication events
- `/var/log/scratchbird/anti_entropy.log` - Anti-entropy reports
- `/var/log/scratchbird/slow_query.log` - Slow queries (>1s)

**Example Log Entries**:

```
[INFO] 2026-01-09 15:30:00 Node 2 joined cluster (gossip)
[INFO] 2026-01-09 15:30:05 Shard 001 assigned to {Node 1, Node 2, Node 3}
[WARNING] 2026-01-09 15:35:12 Clock skew detected: Node 3 clock 50ms behind Node 1
[ERROR] 2026-01-09 15:40:23 Quorum write failed: Shard 001, W=3, ACKs=2 (Node 3 offline)
[INFO] 2026-01-09 15:45:00 Anti-entropy: Window 150 diverged (Shard 001), repair initiated
[INFO] 2026-01-09 15:45:10 Anti-entropy: Repaired 3 rows (Shard 001, Window 150)
```

### Tracing

**Distributed Tracing** (OpenTelemetry):

```
Trace: INSERT INTO sensor_data (Mode 3, W=2, R=2)
  Span 1: Coordinator receives request [5ms]
  Span 2: Send to Node A [1ms]
  Span 3: Send to Node B [1ms]
  Span 4: Send to Node C [1ms]
  Span 5: Wait for W=2 ACKs [3ms]
    - Node A ACK received [2ms]
    - Node B ACK received [3ms]
  Span 6: Return to client [1ms]
Total: 15ms
```

**Jaeger UI**: Visualize end-to-end query execution across nodes.

---

## Capacity Planning

### Sizing Guidelines

**Single Shard Capacity**:
- **Rows**: 1M+ rows (depends on row size)
- **Storage**: 10-50 GB (before compaction)
- **Write Throughput**: 50K TPS (Mode 3, W=2, R=2)
- **Read Throughput**: 100K QPS (single shard)

**Cluster Sizing**:
- **Small Cluster** (3 nodes): 16 shards, 48M rows, 500 GB total
- **Medium Cluster** (10 nodes): 64 shards, 192M rows, 2 TB total
- **Large Cluster** (50 nodes): 256 shards, 768M rows, 8 TB total

### Scaling Strategies

**Horizontal Scaling** (add nodes):
- Add nodes to cluster (gossip discovery)
- Redistribute shards (rebalance)
- No downtime (rolling rebalance)

**Vertical Scaling** (upgrade nodes):
- Upgrade CPU/memory/disk on existing nodes
- Rolling upgrade (one node at a time)

**Shard Splitting** (increase shard count):
- Split hot shards into smaller shards
- Requires data migration (planned downtime or shadow migration)

### Resource Requirements

**Per Node**:
- **CPU**: 8-16 cores (more for high TPS)
- **Memory**: 32-64 GB (more for large active Merkle tree)
- **Disk**: 1-10 TB SSD (NVMe recommended)
- **Network**: 1-10 Gbps (low latency critical)

**Merkle Forest Overhead**:
- **Active Tree**: 48 bytes per row (in-memory)
- **Sealed Trees**: 48 bytes per tree (metadata in-memory, tree on disk)
- **Example**: 1M rows, 720 sealed trees → 48 MB + 34 KB = 48 MB

**Hint Log Overhead**:
- **Per Hint**: ~500 bytes (row data + metadata)
- **Example**: 1000 pending hints → 500 KB

---

## Disaster Recovery

### Backup Strategy

**Full Backup** (weekly):
- Backup all shards (snapshot LSM SSTables)
- Backup catalog (sys_tables, sys_columns, etc.)
- Backup Merkle forest metadata (sealed tree roots)

```bash
# Full backup
scratchbird_backup --type=full --output=/var/backups/scratchbird_full_20260109.tar.gz

# Verify backup
scratchbird_verify_backup --file=/var/backups/scratchbird_full_20260109.tar.gz
```

**Incremental Backup** (daily):
- Backup only new SSTables (since last backup)
- Backup commit log (since last backup)

```bash
# Incremental backup
scratchbird_backup --type=incremental --output=/var/backups/scratchbird_inc_20260109.tar.gz
```

### Recovery Procedures

**Restore from Full Backup**:

```bash
# Stop cluster
for node in node1 node2 node3; do
    ssh $node "sudo systemctl stop scratchbird"
done

# Restore data
scratchbird_restore --file=/var/backups/scratchbird_full_20260109.tar.gz --target=/var/lib/scratchbird

# Start cluster
for node in node1 node2 node3; do
    ssh $node "sudo systemctl start scratchbird"
done

# Verify data
scratchbird_cli --command "SELECT COUNT(*) FROM sys_tables"
```

**Point-in-Time Recovery**:

```bash
# Restore full backup
scratchbird_restore --file=/var/backups/scratchbird_full_20260109.tar.gz

# Replay incremental backups
scratchbird_restore --file=/var/backups/scratchbird_inc_20260110.tar.gz --replay-commit-log

# Replay commit log up to target time
scratchbird_replay_commit_log --until="2026-01-10 15:30:00"
```

### Failover

**Node Failure**:
- Automatic: Gossip detects node offline (30 seconds)
- Quorum writes/reads continue (if W + R ≤ N - 1)
- Hinted handoff stores pending writes
- Recovery: Node rejoins, receives hints + anti-entropy repairs

**Datacenter Failure**:
- Multi-DC deployment (3 DCs, N=5, W=3, R=3)
- 1 DC fails → Cluster operational (3 nodes remaining)
- 2 DCs fail → Cluster read-only (no write quorum)

**Disaster Recovery Runbook**:
1. Detect failure (monitoring alerts)
2. Assess impact (how many nodes/shards affected)
3. Restore from backup (if multiple nodes lost)
4. Or wait for anti-entropy (if single node lost)
5. Validate data integrity (checksum verification)

---

## Performance Tuning

### Tuning Parameters

**Write Performance**:

```yaml
replication:
  write_quorum: 2  # Lower = faster writes, less durability
  write_timeout_ms: 5000  # Increase for slow networks

anti_entropy:
  interval_ms: 120000  # Increase to reduce overhead
  merkle_tree_window_hours: 4  # Larger windows = fewer trees
```

**Read Performance**:

```yaml
replication:
  read_quorum: 1  # Lower = faster reads, may be stale
  read_timeout_ms: 3000  # Decrease for low-latency queries
```

**Compaction Tuning**:

```yaml
lsm:
  l0_compaction_trigger: 4  # Increase to reduce compaction frequency
  max_bytes_for_level_base: 268435456  # 256 MB (larger = less compaction)
```

### Query Optimization

**Use Partition Key in WHERE**:

```sql
-- Good: Single-shard query
SELECT * FROM orders WHERE user_id = 'user_123';

-- Bad: Scatter-gather (all shards)
SELECT * FROM orders WHERE status = 'pending';
```

**Colocate Related Tables**:

```sql
-- Good: Colocated join (single-shard)
SELECT u.name, o.amount
FROM users u JOIN orders o ON u.user_id = o.user_id
WHERE u.user_id = 'user_123';

-- Bad: Non-colocated join (shuffle)
SELECT o.order_id, p.product_name
FROM orders o JOIN products p ON o.product_id = p.product_id;
```

**Use Appropriate Replication Mode**:

| Table Characteristics | Recommended Mode |
|-----------------------|------------------|
| Single-region, read-heavy | Mode 1 (Primary-Replica) |
| Multi-region, write-heavy | Mode 3 (Leaderless Quorum) |
| Strong consistency required | Mode 1 (Primary-Replica) |
| High availability required | Mode 3 (Leaderless Quorum) |

---

## Troubleshooting

### Common Issues

#### Issue 1: Quorum Not Achieved

**Symptom**: Writes fail with error "QuorumNotAchieved"

**Diagnosis**:
```bash
# Check cluster membership
scratchbird_cli --command "SHOW CLUSTER MEMBERS"

# Check shard map (how many replicas online?)
scratchbird_cli --command "SHOW SHARD MAP"
```

**Resolution**:
- Verify nodes are online (gossip heartbeat)
- Check network connectivity (ping, traceroute)
- Temporarily reduce W/R (emergency only):
  ```sql
  ALTER TABLE sensor_data SET write_quorum = 1;  -- Temporary
  ```

#### Issue 2: High Replication Lag

**Symptom**: Replication lag > 1 second

**Diagnosis**:
```bash
# Check replication lag per shard
scratchbird_cli --command "SHOW REPLICATION LAG"

# Check commit log size (backlog?)
du -sh /var/lib/scratchbird/commit_log/
```

**Resolution**:
- Increase network bandwidth (1 Gbps → 10 Gbps)
- Reduce write load (rate limiting)
- Add more replicas (N=3 → N=5, distribute load)

#### Issue 3: Anti-Entropy Overhead High

**Symptom**: CPU usage > 5% during anti-entropy

**Diagnosis**:
```bash
# Check Merkle forest size
scratchbird_cli --command "SHOW MERKLE FOREST SIZE"

# Check anti-entropy frequency
grep "anti_entropy_interval_ms" /etc/scratchbird/cluster.yaml
```

**Resolution**:
- Increase anti-entropy interval (60s → 120s)
- Increase Merkle tree window size (1 hour → 4 hours)
- Optimize tree serialization (disk I/O bottleneck?)

#### Issue 4: Clock Skew Detected

**Symptom**: Warning logs "Clock skew detected: Node X clock Yms behind"

**Diagnosis**:
```bash
# Check NTP status on all nodes
for node in node1 node2 node3; do
    ssh $node "timedatectl status"
done
```

**Resolution**:
- Enable NTP synchronization:
  ```bash
  sudo timedatectl set-ntp true
  ```
- Verify NTP servers configured:
  ```bash
  cat /etc/systemd/timesyncd.conf
  ```

#### Issue 5: Hint Log Growing

**Symptom**: Disk usage increasing, hint log files large

**Diagnosis**:
```bash
# Check hint log size
du -sh /var/lib/scratchbird/hints/

# Check target nodes online
scratchbird_cli --command "SHOW CLUSTER MEMBERS"
```

**Resolution**:
- Verify target nodes are online (hints should deliver)
- Manually deliver hints:
  ```bash
  scratchbird_cli --command "DELIVER HINTS TO NODE 3"
  ```
- If node permanently offline, remove hints:
  ```bash
  rm -rf /var/lib/scratchbird/hints/node3/
  ```

---

## Security Operations

### TLS Configuration

**Enable TLS for Replication**:

```yaml
# /etc/scratchbird/cluster.yaml
security:
  tls_enabled: true
  tls_cert: /etc/scratchbird/certs/node.crt
  tls_key: /etc/scratchbird/certs/node.key
  tls_ca: /etc/scratchbird/certs/ca.crt
```

**Certificate Rotation**:

```bash
# Generate new certificates (CA, node certs)
# (Use existing PKI infrastructure or cert-manager)

# Deploy new certs (rolling update)
for node in node1 node2 node3; do
    scp /tmp/new_node.crt $node:/etc/scratchbird/certs/
    ssh $node "sudo systemctl reload scratchbird"  # Reload config, no downtime
done
```

### Audit Logging

**Enable Audit Log**:

```yaml
# /etc/scratchbird/cluster.yaml
audit:
  enabled: true
  log_file: /var/log/scratchbird/audit.log
  events:
    - CREATE TABLE
    - ALTER TABLE
    - DROP TABLE
    - SET replication_mode
    - GRANT
    - REVOKE
```

**Audit Log Example**:

```
[AUDIT] 2026-01-09 15:30:00 user=admin action=ALTER TABLE table=sensor_data SET replication_mode='leaderless_quorum'
[AUDIT] 2026-01-09 15:31:00 user=admin action=SET write_quorum=3 table=sensor_data
```

---

## Rollback Procedures

### Rollback Beta → Alpha

**Scenario**: Beta features causing production issues, need to rollback to Alpha.

**Prerequisites**:
- Alpha backup available
- Downtime acceptable (rollback not zero-downtime)

**Steps**:

```bash
# 1. Stop Beta cluster
for node in node1 node2 node3; do
    ssh $node "sudo systemctl stop scratchbird"
done

# 2. Restore Alpha binaries
for node in node1 node2 node3; do
    ssh $node "sudo cp /usr/local/bin/scratchbird_alpha_backup /usr/local/bin/scratchbird"
done

# 3. Restore database from Alpha backup
scratchbird_restore --file=/var/backups/scratchbird_alpha_20260109.tar.gz --target=/var/lib/scratchbird

# 4. Start Alpha cluster
for node in node1 node2 node3; do
    ssh $node "sudo systemctl start scratchbird"
done

# 5. Verify rollback
scratchbird_cli --version
# Expected: ScratchBird Alpha v0.9.0

scratchbird_cli --command "SELECT COUNT(*) FROM sys_tables"
# Expected: Data restored
```

### Rollback Mode 3 → Mode 1 (Per Table)

**Scenario**: Leaderless quorum causing issues for specific table, revert to primary-replica.

**Steps**:

```sql
-- Revert table to Mode 1
ALTER TABLE sensor_data SET replication_mode = 'primary_replica';
ALTER TABLE sensor_data SET uuid_type = 'v7';

-- Verify
SELECT table_name, replication_mode, uuid_type
FROM sys_tables
WHERE table_name = 'sensor_data';
-- Expected: replication_mode='primary_replica', uuid_type='v7'
```

**Note**: Existing UUIDv8 rows remain (no data migration needed), new rows use UUIDv7.

---

## Appendix: Operations Checklist

### Pre-Production Checklist

**Infrastructure**:
- [ ] 3+ nodes deployed (odd number for quorum)
- [ ] Network latency < 10ms (same DC), < 100ms (cross-DC)
- [ ] Disk: SSD/NVMe (< 1ms latency)
- [ ] Monitoring: Prometheus + Grafana configured
- [ ] Backups: Automated daily backups, weekly full backups
- [ ] Disaster recovery: Tested restore procedure

**Configuration**:
- [ ] Cluster config reviewed (`/etc/scratchbird/cluster.yaml`)
- [ ] Table replication modes configured (Mode 1 vs Mode 3)
- [ ] Partition keys configured (colocated tables)
- [ ] TLS enabled (replication encrypted)
- [ ] Audit logging enabled

**Testing**:
- [ ] Load testing: 50K TPS per shard sustained
- [ ] Chaos testing: Node failures, network partitions
- [ ] Recovery testing: Restore from backup, verify data

### Post-Deployment Checklist

**Week 1**:
- [ ] Monitor replication lag (< 100ms)
- [ ] Monitor anti-entropy (divergences detected, repaired)
- [ ] Review slow query log (optimize hot queries)
- [ ] Verify hint log size (< 10 MB)

**Week 2**:
- [ ] Review capacity metrics (disk, CPU, memory usage)
- [ ] Test failover (kill one node, verify quorum operational)
- [ ] Backup verification (restore test)

**Month 1**:
- [ ] Performance benchmarks (compare with targets)
- [ ] Capacity planning (project 6-month growth)
- [ ] Disaster recovery drill (full restore)

---

**Document Status:** DRAFT (Beta Specification Phase)
**Last Updated:** 2026-01-09
**Next Review:** Weekly during implementation
**Dependencies:** All implementation phases (01-07)

---

**End of Document 08**
**End of Beta Replication Specification Suite**
