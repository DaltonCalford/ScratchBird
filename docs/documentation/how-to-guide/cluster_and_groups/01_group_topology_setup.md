# Database Group Setup

[Cluster and Groups README](../README.md)

## Synopsis

Set up a Database Group for loosely coupled multi-tenant databases.

## What is a Database Group?

A Database Group is a collection of independent databases that:
- Share configuration (optionally)
- Can query each other
- Start/stop together (linked connections)
- May share authentication sources

## Creating a Group

### Step 1: Create Databases

```sql
-- Create databases in same environment
CREATE DATABASE !:prod.crm;
CREATE DATABASE !:prod.erp;
CREATE DATABASE !:prod.analytics;
```

### Step 2: Create Group

```sql
-- Create group configuration
CREATE GROUP production_group;

-- Add databases to group
ALTER DATABASE !:prod.crm SET GROUP = production_group;
ALTER DATABASE !:prod.erp SET GROUP = production_group;
ALTER DATABASE !:prod.analytics SET GROUP = production_group;
```

### Step 3: Configure Linked Connections

```sql
-- Enable linked startup/shutdown
ALTER GROUP production_group ENABLE LINKED CONNECTIONS;

-- Set startup order
ALTER GROUP production_group SET STARTUP ORDER = (
    !:prod.crm,
    !:prod.erp,
    !:prod.analytics
);
```

## Cross-Database Queries

```sql
-- Query across databases in group
SELECT 
    c.customer_id,
    c.name,
    o.total_orders
FROM !:prod.crm.public.customers c
JOIN !:prod.erp.public.orders o ON c.customer_id = o.customer_id
WHERE c.status = 'active';
```

## Authentication Options

### Independent Authentication

```sql
-- Each database has own users
CREATE USER !:prod.crm.app WITH PASSWORD 'crm_secret';
CREATE USER !:prod.erp.app WITH PASSWORD 'erp_secret';
```

### Shared Authentication Source

```sql
-- Configure LDAP for group
ALTER GROUP production_group SET AUTHENTICATION = 'ldap';
ALTER GROUP production_group SET AUTH_LDAP_SERVER = 'ldap.company.com';

-- Users authenticate against LDAP
-- But permissions are per-database
```

## Replication Between Group Databases

```sql
-- Set up replication channel
CREATE REPLICATION CHANNEL crm_to_analytics
    FROM !:prod.crm
    TO !:prod.analytics
    WITH (
        PUBLICATION = 'customer_data',
        FILTER = 'tables = {customers, orders}'
    );

-- Start replication
ALTER REPLICATION CHANNEL crm_to_analytics START;
```

## Monitoring Group Health

```sql
-- Check database status
SELECT 
    database_name,
    status,
    connections,
    transaction_rate
FROM sb_group_status
WHERE group_name = 'production_group';

-- Check cross-database query performance
SELECT * FROM sb_cross_db_stats;
```

## Failover Considerations

Groups do NOT provide automatic failover. Each database is independent:

```bash
# If crm database fails
# - erp and analytics continue running
# - Manual intervention required
# - Restore from backup or promote replica
```

## When to Use Groups vs Clusters

| Use Case | Recommendation |
|----------|----------------|
| Multi-tenant SaaS | Group |
| Legacy migration | Group |
| Environment separation | Group |
| High availability | Cluster |
| Automatic failover | Cluster |
| Sharding | Cluster |

## See Also

- [Cluster Setup](03_bootstrap_cluster_identity.md)
- [Replication Channels](../cluster_and_groups/04_configure_replication_channels.md)
