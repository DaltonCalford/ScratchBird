<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# Group and Cluster Trust Models

[Prev](./08_cli_network_mode.md) | [Next](./README.md) | [Topic README](./README.md) | [Developers Guide README](../README.md)

## Coverage and Evidence Status

Status: Complete

## Synopsis

ScratchBird supports two distributed deployment models: **Groups** (loosely coupled independent databases) and **Clusters** (tightly coupled distributed systems with shared security).

## Groups vs Clusters

| Aspect | GROUP | CLUSTER |
|--------|-------|---------|
| **Coupling** | Loose | Tight |
| **Security** | Per-database | Shared distributed |
| **Authentication** | Each database independent | T-of-N consensus |
| **Key Management** | Local | Shamir's Secret Sharing |
| **Data Sharing** | Cross-database queries | Automatic sharding |
| **Failover** | Manual / Linked connections | Automatic |
| **Legacy Support** | Yes (PG, MySQL, FB) | No (SB only) |
| **Use Case** | Multi-tenant isolation | High availability |

## Database Groups

### Group Model

```
┌─────────────────────────────────────────────────────────────┐
│                    DATABASE GROUP                            │
│                     "Production"                             │
│                                                              │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐   │
│  │ Database A   │    │ Database B   │    │ Database C   │   │
│  │ (Native SB)  │    │ (Emulated PG)│    │ (Emulated FB)│   │
│  │              │    │              │    │              │   │
│  │ • Own users  │    │ • Own users  │    │ • Own users  │   │
│  │ • Own auth   │    │ • Own auth   │    │ • Own auth   │   │
│  │ • Own keys   │    │ • Own keys   │    │ • Own keys   │   │
│  │ • Own files  │    │ • Own files  │    │ • Own files  │   │
│  └──────┬───────┘    └──────┬───────┘    └──────┬───────┘   │
│         │                   │                   │            │
│         │                   │ Cross-database    │            │
│         │                   │ queries possible  │            │
│         └───────────────────┴───────────────────┘            │
│                                                              │
│  Characteristics:                                            │
│  • MAY share auth source (LDAP) but NOT required             │
│  • Each maintains internal grants/rights                     │
│  • Separate configuration per database                       │
│  • Linked connections for startup/shutdown                   │
└─────────────────────────────────────────────────────────────┘
```

### Group Use Cases

1. **Multi-tenant SaaS** - Each customer in separate database
2. **Legacy Migration** - Gradual migration from PostgreSQL/MySQL/Firebird
3. **Environment Separation** - Dev, staging, prod in same group
4. **Compliance Boundaries** - Isolate regulated data

### Cross-Database Queries (Groups)

```sql
-- Query across databases in same group
SELECT 
    a.user_id,
    a.account_balance,
    b.support_tickets
FROM !:prod.crm.accounts a
JOIN !:prod.support.tickets b ON a.user_id = b.user_id
WHERE a.account_balance > 1000;
```

### Creating a Group

```sql
-- Create database group
CREATE GROUP production_group;

-- Add databases to group
ALTER DATABASE !:prod.crm SET GROUP = production_group;
ALTER DATABASE !:prod.support SET GROUP = production_group;
ALTER DATABASE !:prod.analytics SET GROUP = production_group;

-- Enable linked connections
ALTER GROUP production_group ENABLE LINKED CONNECTIONS;
```

## Clusters

### Cluster Model

```
┌─────────────────────────────────────────────────────────────┐
│                    SCRATCHBIRD CLUSTER                       │
│                                                              │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  SECURITY LAYER - Distributed Key Management          │  │
│  │  • No single node holds complete security info        │  │
│  │  • Shamir's Secret Sharing (T of N)                   │  │
│  │  • Consensus required for authentication              │  │
│  └───────────────────────────────────────────────────────┘  │
│                              │                               │
│  ┌─────────────┬─────────────┼─────────────┬─────────────┐  │
│  │             │             │             │             │  │
│  ▼             ▼             ▼             ▼             ▼  │
│┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐      │
││Node 1│◄─►│Node 2│◄─►│Node 3│◄─►│Node 4│◄─►│Node 5│      │
││Leader│   │Follow│   │Follow│   │Follow│   │Follow│      │
│└──┬───┘   └──────┘   └──────┘   └──────┘   └──────┘      │
│   │                                                        │
│   │ Raft Consensus (T=3 of N=5)                           │
│   │ • Leader election                                      │
│   │ • Log replication                                      │
│   │ • Shard distribution                                   │
│   │                                                        │
│   │ Automatic Sharding                                     │
│   │ • Data partitioned across nodes                        │
│   │ • Queries routed to appropriate shards                 │
│   │ • Automatic rebalancing                                │
│                                                            │
│  Shared Objects:                                           │
│  • Users (same across cluster)                            │
│  • Roles (same across cluster)                            │
│  • Domains (same across cluster)                          │
│  • Groups (same across cluster)                           │
└─────────────────────────────────────────────────────────────┘
```

### Cluster Security - Shamir's Secret Sharing

```
Key Distribution:
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  Master Key: K (never stored complete anywhere)            │
│                                                             │
│  Split into shares: S1, S2, S3, S4, S5                     │
│  Threshold: T = 3 (need 3 shares to reconstruct)           │
│                                                             │
│  Distribution:                                             │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐            │
│  │Node 1│ │Node 2│ │Node 3│ │Node 4│ │Node 5│            │
│  │  S1  │ │  S2  │ │  S3  │ │  S4  │ │  S5  │            │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘            │
│                                                             │
│  Authentication requires T=3 nodes to agree:               │
│  • Any 3 nodes can reconstruct key                         │
│  • No single node can decrypt data alone                   │
│  • Compromise of <3 nodes doesn't expose key               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Creating a Cluster

```sql
-- Bootstrap first node
CREATE CLUSTER analytics_cluster 
    WITH (
        node_id = 1,
        listen_address = '192.168.1.10',
        raft_port = 3093,
        shard_count = 256
    );

-- Join additional nodes
ALTER CLUSTER analytics_cluster 
    ADD NODE (
        node_id = 2,
        address = '192.168.1.11',
        raft_port = 3093
    );

-- Initialize with T=3 of N=5
ALTER CLUSTER analytics_cluster 
    SET CONSENSUS THRESHOLD = 3;
```

## Authentication Flow Comparison

### Group Authentication

```
Client ──► Listener ──► Database ──► Auth Policy
                │            │
                │            ├─► Local UUID lookup
                │            ├─► Local authentication
                │            └─► Local permission check
                │
Each database independent
```

### Cluster Authentication

```
Client ──► Listener ──► Node ──► Consensus Request
                                      │
                    ┌─────────────────┼─────────────────┐
                    │                 │                 │
                    ▼                 ▼                 ▼
                  Node 1           Node 2           Node 3
                    │                 │                 │
                    └─────────────────┼─────────────────┘
                                      │
                              T of N Agreement
                                      │
                                      ▼
                              Authentication
                              Successful

Requires T nodes to agree on identity
```

## Replication Channels

### Group Replication

```sql
-- Logical replication between group databases
CREATE REPLICATION CHANNEL crm_to_analytics
    FROM !:prod.crm
    TO !:prod.analytics
    WITH (PUBLICATION = 'sales_data');
```

### Cluster Replication

```sql
-- Automatic shard replication in cluster
-- Configured at cluster level, not per-database
ALTER CLUSTER analytics_cluster
    SET REPLICATION_FACTOR = 3;  -- Each shard on 3 nodes
```

## Failover Comparison

### Group Failover

```
Database A (Primary)    Database B (Secondary)
         │                       │
         │  Manual promotion     │
         ▼                       ▼
    Failure ─────────────►  Promote B to Primary
         
         Requires manual intervention
         or linked connection automation
```

### Cluster Failover

```
Node 1 (Leader)    Node 2 (Follower)    Node 3 (Follower)
       │                  │                    │
       │  Heartbeat       │                    │
       │─────────────────►│◄───────────────────│
       │                  │                    │
       ▼                  │                    │
    Failure              │                    │
                         │                    │
    Raft Election ◄──────┴────────────────────┘
                         │
                         ▼
                  Node 2 elected Leader
                  
    Automatic, < 3 seconds
```

## When to Use What

### Use Groups When:
- Running multiple independent applications
- Migrating from legacy databases gradually
- Need per-database isolation
- Don't need automatic failover
- Have distinct authentication requirements per database

### Use Clusters When:
- Need high availability (99.99%+)
- Have large datasets requiring sharding
- Need automatic failover
- Can use SB-native only (no emulation)
- Need strong distributed security
- Want automatic load balancing

## Hybrid Deployments

```
┌─────────────────────────────────────────────────────────────┐
│                    HYBRID DEPLOYMENT                        │
│                                                              │
│  ┌──────────────────────┐    ┌──────────────────────────┐  │
│  │   PRODUCTION CLUSTER │    │      LEGACY GROUP        │  │
│  │  ┌──────┐  ┌──────┐  │    │  ┌──────┐    ┌──────┐   │  │
│  │  │Node 1│  │Node 2│  │    │  │Legacy│    │Legacy│   │  │
│  │  │  SB  │  │  SB  │  │    │  │  PG  │    │  FB  │   │  │
│  │  └──────┘  └──────┘  │    │  └──────┘    └──────┘   │  │
│  │       Raft Consensus   │    │       Group Link        │  │
│  └──────────────────────┘    └──────────────────────────┘  │
│                                                              │
│  • Migrate from Group to Cluster over time                  │
│  • Keep legacy systems in Group                             │
│  • New systems in Cluster                                   │
└─────────────────────────────────────────────────────────────┘
```

## Completion Checklist

- [x] Group model documented
- [x] Cluster model documented
- [x] Security comparison (local vs distributed)
- [x] Authentication flow comparison
- [x] Failover comparison
- [x] Decision guidance (when to use what)
- [x] Hybrid deployment patterns
- [x] Diagrams for both models

## See Also

- [System Topology](01_system_topology.md)
- [Cluster and Distribution](../../language_reference/syntax_guide/cluster_and_distribution/README.md)
- [Disaster Recovery and Continuity](../../disaster_recovery_and_continuity/README.md)
