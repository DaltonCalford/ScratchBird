# ScratchBird Migration Guide

## Overview

This guide walks you through migrating from legacy databases (PostgreSQL, MySQL, SQL Server, Firebird) to ScratchBird with zero downtime using the Live Migration Passthrough feature.

**Key Benefits:**
- Zero application downtime
- No code changes required
- Incremental, table-by-table migration
- Full rollback capability
- Built-in data validation

---

## Quick Start (5 Minutes)

### Step 1: Create Foreign Server

```sql
-- Connect to ScratchBird
sb_isql mydb.sbdb

-- Create server definition pointing to your source database
CREATE SERVER legacy_db
    FOREIGN DATA WRAPPER postgresql_fdw
    OPTIONS (
        host 'legacy-db.example.com',
        port '5432',
        dbname 'production'
    );

-- Create user mapping
CREATE USER MAPPING FOR CURRENT_USER
    SERVER legacy_db
    OPTIONS (
        user 'migration_user',
        password 'secret'
    );
```

### Step 2: Create Migration Source

```sql
CREATE MIGRATION SOURCE legacy
    FROM SERVER legacy_db
    OPTIONS (
        cdc_mode 'logical_replication',
        batch_size 10000
    );
```

### Step 3: Start Migration

```sql
-- Import schema and start migrating a table
IMPORT FOREIGN SCHEMA public
    FROM SERVER legacy_db
    INTO public;

START MIGRATION FOR TABLE users FROM legacy;
```

### Step 4: Monitor Progress

```sql
SHOW MIGRATION STATUS;
```

### Step 5: Cutover When Ready

```sql
-- Validate first
VALIDATE MIGRATION FOR TABLE users;

-- Execute cutover
CUTOVER TABLE users;
```

---

## Migration Planning

### 1. Source Database Assessment

Before starting, assess your source database:

```sql
-- Connect to source database and gather information

-- PostgreSQL
SELECT
    schemaname,
    tablename,
    pg_size_pretty(pg_total_relation_size(schemaname||'.'||tablename)) as size,
    n_live_tup as row_count
FROM pg_stat_user_tables
ORDER BY pg_total_relation_size(schemaname||'.'||tablename) DESC;

-- MySQL
SELECT
    table_schema,
    table_name,
    ROUND(data_length / 1024 / 1024, 2) as size_mb,
    table_rows
FROM information_schema.tables
WHERE table_schema NOT IN ('mysql', 'information_schema', 'performance_schema')
ORDER BY data_length DESC;
```

**Checklist:**
- [ ] Total database size
- [ ] Number of tables
- [ ] Largest tables (for migration ordering)
- [ ] Tables with complex types (JSON, arrays, custom types)
- [ ] Foreign key relationships
- [ ] Triggers and stored procedures
- [ ] Current replication setup

### 2. Schema Compatibility Analysis

Check for potential compatibility issues:

| Source Feature | ScratchBird Support | Notes |
|----------------|---------------------|-------|
| Standard SQL types | ✅ Full | INTEGER, VARCHAR, etc. |
| JSON/JSONB | ✅ Full | Native JSON support |
| Arrays | ✅ Full | All array types |
| UUID | ✅ Full | Native UUID type |
| SERIAL/IDENTITY | ✅ Full | Maps to GENERATED |
| Custom types | ⚠️ Partial | May need conversion |
| PostGIS | ⚠️ Planned | Geometry types in roadmap |
| Full-text search | ✅ Full | Native tsvector/tsquery |

**Problem Types to Watch:**
```sql
-- Find tables with potentially problematic types
SELECT table_name, column_name, data_type
FROM information_schema.columns
WHERE table_schema = 'public'
  AND data_type IN ('user-defined', 'ARRAY', 'json', 'jsonb', 'xml');
```

### 3. Estimating Migration Duration

| Database Size | Bulk Load Time | CDC Catch-up | Total |
|---------------|----------------|--------------|-------|
| < 1 GB | < 5 min | < 1 min | ~10 min |
| 1-10 GB | 15-60 min | 1-5 min | ~1 hour |
| 10-100 GB | 1-8 hours | 5-30 min | ~8 hours |
| 100 GB - 1 TB | 8-48 hours | 30-120 min | ~2 days |
| > 1 TB | Days | Hours | ~1 week |

**Factors affecting speed:**
- Network bandwidth between servers
- Source database load (avoid peak hours)
- Number of indexes to rebuild
- Complexity of data types
- Foreign key constraint checking

### 4. Risk Assessment

| Risk | Mitigation |
|------|------------|
| Data loss | Full rollback capability; source unchanged |
| Downtime | Zero-downtime migration; fallback to source |
| Performance impact on source | Rate limiting; off-peak scheduling |
| Schema drift | Pre-migration schema freeze |
| Application errors | Thorough testing with dual-write |

---

## Step-by-Step Walkthroughs

### PostgreSQL to ScratchBird

#### Prerequisites

1. **Source Requirements:**
   - PostgreSQL 9.6+ (10+ recommended for logical replication)
   - `wal_level = logical` in postgresql.conf
   - Migration user with REPLICATION privilege

2. **Setup on PostgreSQL:**
```sql
-- Create migration user
CREATE USER migration_user WITH REPLICATION PASSWORD 'secret';

-- Grant read access
GRANT USAGE ON SCHEMA public TO migration_user;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO migration_user;
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT SELECT ON TABLES TO migration_user;

-- Create publication for logical replication
CREATE PUBLICATION scratchbird_migration FOR ALL TABLES;
```

3. **Verify settings:**
```sql
SHOW wal_level;  -- Should be 'logical'
SELECT * FROM pg_publication WHERE pubname = 'scratchbird_migration';
```

#### Migration Steps

```sql
-- 1. Create server connection
CREATE SERVER legacy_pg
    FOREIGN DATA WRAPPER postgresql_fdw
    OPTIONS (
        host 'pg-prod.example.com',
        port '5432',
        dbname 'production',
        sslmode 'require'
    );

CREATE USER MAPPING FOR CURRENT_USER
    SERVER legacy_pg
    OPTIONS (user 'migration_user', password 'secret');

-- 2. Create migration source
CREATE MIGRATION SOURCE pg_source
    FROM SERVER legacy_pg
    OPTIONS (
        cdc_mode 'logical_replication',
        publication 'scratchbird_migration',
        batch_size 50000,
        parallel_workers 4
    );

-- 3. Import schema
IMPORT FOREIGN SCHEMA public
    FROM SERVER legacy_pg
    INTO public;

-- 4. Start migration (one table at a time, or all)
START MIGRATION FOR TABLE users FROM pg_source;
START MIGRATION FOR TABLE orders FROM pg_source;
-- Or migrate entire schema:
-- START MIGRATION FOR SCHEMA public FROM pg_source;

-- 5. Monitor progress
SHOW MIGRATION STATUS;

-- 6. Wait for DUAL_WRITE state, then validate
VALIDATE MIGRATION FOR TABLE users;

-- 7. Cutover
CUTOVER TABLE users;
```

### MySQL to ScratchBird

#### Prerequisites

1. **Source Requirements:**
   - MySQL 5.7+ or MariaDB 10+
   - Binary logging enabled
   - GTID mode recommended

2. **Setup on MySQL:**
```sql
-- Create migration user
CREATE USER 'migration_user'@'%' IDENTIFIED BY 'secret';
GRANT SELECT, REPLICATION SLAVE, REPLICATION CLIENT ON *.* TO 'migration_user'@'%';
FLUSH PRIVILEGES;

-- Verify binary logging
SHOW VARIABLES LIKE 'log_bin';  -- Should be ON
SHOW VARIABLES LIKE 'gtid_mode';  -- Should be ON for GTID
SHOW VARIABLES LIKE 'binlog_format';  -- Should be ROW
```

#### Migration Steps

```sql
-- 1. Create server connection
CREATE SERVER legacy_mysql
    FOREIGN DATA WRAPPER mysql_fdw
    OPTIONS (
        host 'mysql-prod.example.com',
        port '3306',
        dbname 'production'
    );

CREATE USER MAPPING FOR CURRENT_USER
    SERVER legacy_mysql
    OPTIONS (user 'migration_user', password 'secret');

-- 2. Create migration source
CREATE MIGRATION SOURCE mysql_source
    FROM SERVER legacy_mysql
    OPTIONS (
        cdc_mode 'binlog',
        use_gtid TRUE,
        batch_size 50000
    );

-- 3. Import and migrate
IMPORT FOREIGN SCHEMA production
    FROM SERVER legacy_mysql
    INTO public;

START MIGRATION FOR SCHEMA public FROM mysql_source;

-- 4. Monitor and cutover
SHOW MIGRATION STATUS;
VALIDATE MIGRATION FOR SCHEMA public;
CUTOVER SCHEMA public;
```

### SQL Server to ScratchBird

#### Prerequisites

1. **Source Requirements:**
   - SQL Server 2016+ (2019+ recommended)
   - Change Tracking or CDC enabled

2. **Setup on SQL Server:**
```sql
-- Enable Change Tracking at database level
ALTER DATABASE production
SET CHANGE_TRACKING = ON
(CHANGE_RETENTION = 7 DAYS, AUTO_CLEANUP = ON);

-- Enable for each table
ALTER TABLE dbo.users ENABLE CHANGE_TRACKING;
ALTER TABLE dbo.orders ENABLE CHANGE_TRACKING;

-- Create migration user
CREATE LOGIN migration_user WITH PASSWORD = 'secret';
CREATE USER migration_user FOR LOGIN migration_user;
GRANT SELECT ON SCHEMA::dbo TO migration_user;
GRANT VIEW CHANGE TRACKING ON SCHEMA::dbo TO migration_user;
```

#### Migration Steps

```sql
-- 1. Create server connection
CREATE SERVER legacy_mssql
    FOREIGN DATA WRAPPER tds_fdw
    OPTIONS (
        host 'mssql-prod.example.com',
        port '1433',
        dbname 'production'
    );

CREATE USER MAPPING FOR CURRENT_USER
    SERVER legacy_mssql
    OPTIONS (user 'migration_user', password 'secret');

-- 2. Create migration source
CREATE MIGRATION SOURCE mssql_source
    FROM SERVER legacy_mssql
    OPTIONS (
        cdc_mode 'change_tracking',
        batch_size 25000
    );

-- 3. Import and migrate
IMPORT FOREIGN SCHEMA dbo
    FROM SERVER legacy_mssql
    INTO public;

START MIGRATION FOR SCHEMA public FROM mssql_source;
```

### Firebird to ScratchBird

#### Prerequisites

1. **Source Requirements:**
   - Firebird 2.5+ (3.0+ recommended)
   - Shadow tables for CDC (no native logical replication)

2. **Setup on Firebird:**
```sql
-- Create migration user
CREATE USER MIGRATION_USER PASSWORD 'secret';
GRANT SELECT ON TABLE users TO MIGRATION_USER;
GRANT SELECT ON TABLE orders TO MIGRATION_USER;

-- Note: ScratchBird will create shadow tables and triggers
-- for CDC tracking automatically
```

#### Migration Steps

```sql
-- 1. Create server connection
CREATE SERVER legacy_firebird
    FOREIGN DATA WRAPPER firebird_fdw
    OPTIONS (
        host 'firebird-prod.example.com',
        port '3050',
        dbname '/data/production.fdb'
    );

CREATE USER MAPPING FOR CURRENT_USER
    SERVER legacy_firebird
    OPTIONS (user 'MIGRATION_USER', password 'secret');

-- 2. Create migration source (trigger-based CDC)
CREATE MIGRATION SOURCE firebird_source
    FROM SERVER legacy_firebird
    OPTIONS (
        cdc_mode 'trigger',  -- Uses shadow tables
        batch_size 10000
    );

-- 3. Import and migrate
IMPORT FOREIGN SCHEMA PUBLIC
    FROM SERVER legacy_firebird
    INTO public;

START MIGRATION FOR SCHEMA public FROM firebird_source;
```

---

## Schema Migration

### Automatic Schema Import

```sql
-- Import all tables from a schema
IMPORT FOREIGN SCHEMA source_schema
    FROM SERVER server_name
    INTO target_schema;

-- Import specific tables
IMPORT FOREIGN SCHEMA source_schema
    LIMIT TO (users, orders, products)
    FROM SERVER server_name
    INTO target_schema;

-- Import excluding certain tables
IMPORT FOREIGN SCHEMA source_schema
    EXCEPT (temp_table, log_table)
    FROM SERVER server_name
    INTO target_schema;
```

### Type Mapping Considerations

| PostgreSQL | MySQL | SQL Server | Firebird | ScratchBird |
|------------|-------|------------|----------|-------------|
| SERIAL | AUTO_INCREMENT | IDENTITY | GENERATOR | GENERATED BY DEFAULT |
| BIGSERIAL | BIGINT AUTO | BIGINT IDENTITY | BIGINT GENERATOR | BIGINT GENERATED |
| TEXT | LONGTEXT | NVARCHAR(MAX) | BLOB SUB_TYPE TEXT | TEXT |
| BYTEA | BLOB | VARBINARY(MAX) | BLOB | BYTEA |
| BOOLEAN | TINYINT(1) | BIT | SMALLINT | BOOLEAN |
| JSON/JSONB | JSON | NVARCHAR(MAX) | BLOB | JSON |
| TIMESTAMP | DATETIME | DATETIME2 | TIMESTAMP | TIMESTAMP |
| INTERVAL | - | - | - | INTERVAL |
| UUID | CHAR(36) | UNIQUEIDENTIFIER | CHAR(36) | UUID |
| ARRAY | - | - | - | ARRAY |
| INET/CIDR | VARCHAR | VARCHAR | VARCHAR | INET/CIDR |

### Index and Constraint Handling

```sql
-- By default, indexes are created after bulk load
START MIGRATION FOR TABLE large_table FROM source
    OPTIONS (defer_indexes TRUE);

-- To create indexes during load (slower but progressive):
START MIGRATION FOR TABLE large_table FROM source
    OPTIONS (defer_indexes FALSE);

-- To create indexes concurrently after load:
START MIGRATION FOR TABLE large_table FROM source
    OPTIONS (concurrent_indexes TRUE);
```

### Sequence/Auto-Increment Migration

```sql
-- Sequences are automatically migrated
-- Current value is synced during cutover

-- To manually sync a sequence:
SELECT setval('users_id_seq',
    (SELECT MAX(id) FROM users) + 1);
```

---

## Data Migration Strategies

### Small Databases (< 10 GB)

Simple bulk load with immediate cutover:

```sql
-- Single command migration
START MIGRATION FOR SCHEMA public FROM source
    OPTIONS (
        batch_size 50000,
        parallel_workers 4,
        defer_indexes TRUE
    );

-- Wait for completion (usually < 1 hour)
-- Then cutover all at once
CUTOVER SCHEMA public;
```

### Medium Databases (10 GB - 1 TB)

Parallel batch migration with CDC:

```sql
-- Configure for throughput
CREATE MIGRATION SOURCE source_name
    FROM SERVER server_name
    OPTIONS (
        cdc_mode 'logical_replication',
        batch_size 100000,
        parallel_workers 8,
        rate_limit 100000  -- 100K rows/sec max
    );

-- Migrate in priority order
START MIGRATION FOR TABLE critical_table FROM source_name;
-- Wait for critical tables to reach DUAL_WRITE

START MIGRATION FOR TABLE less_critical FROM source_name;

-- Cutover critical tables first
CUTOVER TABLE critical_table;

-- Continue with remaining tables
CUTOVER TABLE less_critical;
```

### Large Databases (> 1 TB)

Partitioned migration with careful scheduling:

```sql
-- Migrate by partition or date range
START MIGRATION FOR TABLE orders FROM source_name
    OPTIONS (
        where_clause 'order_date >= ''2024-01-01''',
        order_by 'order_id',
        batch_size 50000,
        parallel_workers 4
    );

-- For historical data (can be slower)
START MIGRATION FOR TABLE orders_archive FROM source_name
    OPTIONS (
        where_clause 'order_date < ''2024-01-01''',
        rate_limit 50000  -- Lower rate for historical
    );
```

---

## Application Integration

### Connection String Changes

**Before (PostgreSQL):**
```
postgresql://user:pass@legacy-db:5432/production
```

**After (ScratchBird with PostgreSQL protocol):**
```
postgresql://user:pass@scratchbird:5432/production
```

**ScratchBird Native Protocol:**
```
scratchbird://user:pass@scratchbird:3092/production
```

### ORM Compatibility

| ORM/Framework | Protocol | Compatibility |
|---------------|----------|---------------|
| SQLAlchemy | PostgreSQL | ✅ Full |
| Django ORM | PostgreSQL | ✅ Full |
| ActiveRecord | PostgreSQL | ✅ Full |
| Hibernate | PostgreSQL/MySQL | ✅ Full |
| Entity Framework | TDS | ✅ Full |
| Sequelize | PostgreSQL/MySQL | ✅ Full |
| Prisma | PostgreSQL/MySQL | ✅ Full |

### Testing Checklist

- [ ] Connection test with new endpoint
- [ ] Basic CRUD operations
- [ ] Complex queries (JOINs, subqueries, CTEs)
- [ ] Transaction handling
- [ ] Prepared statements
- [ ] Connection pooling behavior
- [ ] Error handling
- [ ] Performance baseline

---

## Monitoring Migration

### Dashboard Setup

Key metrics to display:

```sql
-- Migration overview
SELECT
    source_table,
    state,
    ROUND(100.0 * bulk_rows_migrated / NULLIF(bulk_rows_total, 0), 1) as progress_pct,
    cdc_lag_ms / 1000.0 as cdc_lag_sec
FROM SYS$MIGRATION_STATE
ORDER BY state, source_table;
```

### Key Metrics to Watch

| Metric | Warning | Critical |
|--------|---------|----------|
| CDC Lag | > 30s | > 300s |
| Bulk Load Rate | < 1000 rows/s | < 100 rows/s |
| Conflict Rate | > 1/hour | > 10/hour |
| Error Rate | > 0.01% | > 0.1% |

### Alerting Thresholds

```yaml
# Prometheus alerts
- alert: MigrationCDCLag
  expr: scratchbird_migration_cdc_lag_seconds > 60
  for: 5m
  annotations:
    summary: "Migration CDC lag exceeds 1 minute"

- alert: MigrationStalled
  expr: rate(scratchbird_migration_bulk_rows_migrated[10m]) == 0
  annotations:
    summary: "Migration bulk load has stalled"
```

### Progress Estimation

```sql
-- Estimated time remaining
SELECT
    source_table,
    bulk_rows_total - bulk_rows_migrated as rows_remaining,
    CASE
        WHEN rows_per_second > 0 THEN
            (bulk_rows_total - bulk_rows_migrated) / rows_per_second || ' seconds'
        ELSE 'Unknown'
    END as eta
FROM SYS$MIGRATION_STATE
JOIN (
    SELECT table_id,
           rate(bulk_rows_migrated, '1 minute') as rows_per_second
    FROM SYS$MIGRATION_METRICS
) m USING (table_id)
WHERE state = 'BULK_LOADING';
```

---

## Cutover Procedures

### Pre-Cutover Checklist

- [ ] CDC lag < 5 seconds
- [ ] No pending conflicts
- [ ] Validation passed
- [ ] All indexes built
- [ ] Foreign keys validated
- [ ] Application tested with dual-write
- [ ] Rollback procedure tested
- [ ] Stakeholders notified

### Cutover Execution

```sql
-- 1. Verify readiness
VALIDATE MIGRATION FOR TABLE users;
SHOW MIGRATION STATUS FOR TABLE users;

-- 2. Execute cutover (brief write pause)
CUTOVER TABLE users
    TIMEOUT 60;  -- Max 60 seconds for final sync

-- 3. Verify success
SELECT state FROM SYS$MIGRATION_STATE
WHERE source_table = 'users';
-- Should be 'LOCAL_ONLY'

-- 4. Monitor for issues
SHOW MIGRATION ERRORS FOR TABLE users LAST '10 minutes';
```

### Post-Cutover Validation

```sql
-- Compare row counts
SELECT
    (SELECT COUNT(*) FROM users) as local_count,
    (SELECT COUNT(*) FROM legacy_db.public.users) as remote_count;

-- Spot check recent records
SELECT * FROM users
WHERE created_at > CURRENT_TIMESTAMP - INTERVAL '1 hour'
ORDER BY created_at DESC
LIMIT 10;
```

### Rollback Procedures

If issues occur after cutover:

```sql
-- Immediate rollback (< 1 hour after cutover)
ROLLBACK MIGRATION FOR TABLE users
    TIMEOUT 300;

-- Extended rollback (requires re-sync)
ROLLBACK MIGRATION FOR TABLE users
    PRESERVE LOCAL DATA
    TIMEOUT 3600;
```

---

## Troubleshooting

### Common Issues and Solutions

#### CDC Lag Increasing

**Symptoms:**
- CDC lag growing over time
- Migration stuck in SYNCHRONIZING

**Causes:**
- High write rate on source
- Network bandwidth limitation
- Slow apply on target

**Solutions:**
```sql
-- Check current lag
SHOW MIGRATION STATUS FOR TABLE users;

-- Increase parallel workers
ALTER MIGRATION FOR TABLE users
    SET OPTION cdc_parallel_workers = 8;

-- Check for slow queries on target
SELECT * FROM SYS$MIGRATION_ERRORS
WHERE table_name = 'users'
ORDER BY timestamp DESC;
```

#### Bulk Load Stalled

**Symptoms:**
- No progress for extended period
- State remains BULK_LOADING

**Causes:**
- Source query timeout
- Network disconnect
- Target resource exhaustion

**Solutions:**
```sql
-- Check migration errors
SHOW MIGRATION ERRORS FOR TABLE users;

-- Resume from checkpoint
RESUME MIGRATION FOR TABLE users;

-- Force restart if needed
ABORT MIGRATION FOR TABLE users;
START MIGRATION FOR TABLE users FROM source;
```

#### High Conflict Rate

**Symptoms:**
- Many conflicts detected
- Alerts firing for conflict rate

**Causes:**
- Concurrent modifications
- Clock skew between systems
- Missing primary key

**Solutions:**
```sql
-- Check conflict details
SELECT * FROM SYS$MIGRATION_CONFLICTS
WHERE table_id = (SELECT table_id FROM SYS$MIGRATION_STATE
                  WHERE source_table = 'users')
ORDER BY detected_at DESC
LIMIT 10;

-- Change conflict strategy
ALTER MIGRATION FOR TABLE users
    SET OPTION conflict_strategy = 'timestamp_wins';
```

#### Schema Mismatch

**Symptoms:**
- Migration fails to start
- Type conversion errors

**Solutions:**
```sql
-- Check schema differences
SELECT * FROM compare_schemas(
    'legacy_db', 'public', 'users',
    NULL, 'public', 'users'
);

-- Force type conversion
START MIGRATION FOR TABLE users FROM source
    OPTIONS (
        type_coercion 'lenient'
    );
```

### Performance Problems

#### Slow Bulk Load

```sql
-- Optimize batch size
ALTER MIGRATION FOR TABLE users
    SET OPTION batch_size = 100000;

-- Disable indexes during load
ALTER MIGRATION FOR TABLE users
    SET OPTION defer_indexes = TRUE;

-- Increase parallelism
ALTER MIGRATION FOR TABLE users
    SET OPTION parallel_workers = 8;
```

#### CDC Apply Slow

```sql
-- Increase apply workers
ALTER MIGRATION FOR TABLE users
    SET OPTION cdc_apply_workers = 8;

-- Batch CDC changes
ALTER MIGRATION FOR TABLE users
    SET OPTION cdc_batch_size = 5000;
```

### Data Inconsistency Handling

```sql
-- Find mismatched rows
SELECT * FROM validate_table_data('users', 'full')
WHERE mismatch_type IS NOT NULL;

-- Repair specific rows
REPAIR MIGRATION FOR TABLE users
    WHERE id IN (123, 456, 789)
    STRATEGY 'copy_from_source';

-- Full re-sync if needed
ABORT MIGRATION FOR TABLE users;
DELETE FROM users;  -- If safe
START MIGRATION FOR TABLE users FROM source;
```

---

## Best Practices

### Migration Window Planning

1. **Preparation Phase (1-2 weeks before)**
   - Assess source database
   - Plan table migration order
   - Set up monitoring
   - Test in staging environment

2. **Migration Phase**
   - Start with small, non-critical tables
   - Progress to larger tables
   - Monitor throughout

3. **Cutover Phase**
   - Schedule during low-traffic period
   - Have rollback plan ready
   - Keep source running for 1 week minimum

### Table Migration Order

1. **Reference/lookup tables** - Small, rarely modified
2. **Independent tables** - No foreign key dependencies
3. **Dependent tables** - Tables referenced by others
4. **High-traffic tables** - Most active tables
5. **Large historical tables** - Archive data (can be slower)

### Communication Templates

**Pre-Migration Notice:**
```
Subject: Database Migration Scheduled

We will be migrating [database] to ScratchBird starting [date].

Expected Impact: None - migration is transparent
Duration: Approximately [X days/weeks]
Rollback Plan: Full rollback capability available

Contact: [migration team email]
```

**Cutover Notice:**
```
Subject: [Table/Schema] Cutover Complete

The following tables have been successfully migrated:
- [table1]
- [table2]

Status: All validations passed
Action Required: None - applications continue normally

Please report any issues to [contact].
```

### Post-Migration Cleanup

After successful migration and observation period:

```sql
-- Remove migration source
DROP MIGRATION SOURCE source_name CASCADE;

-- Remove foreign server
DROP SERVER legacy_db CASCADE;

-- Clean up CDC artifacts on source
-- PostgreSQL:
DROP PUBLICATION scratchbird_migration;
SELECT pg_drop_replication_slot('scratchbird_migration_slot');

-- Archive migration history
DELETE FROM SYS$MIGRATION_HISTORY
WHERE timestamp < CURRENT_TIMESTAMP - INTERVAL '30 days';

-- Update documentation
-- Remove references to legacy database
```

---

## Appendix: Quick Reference

### SQL Command Cheat Sheet

```sql
-- Setup
CREATE SERVER ... FOREIGN DATA WRAPPER ... OPTIONS (...);
CREATE USER MAPPING FOR ... SERVER ... OPTIONS (...);
CREATE MIGRATION SOURCE ... FROM SERVER ... OPTIONS (...);

-- Control
START MIGRATION FOR TABLE ... FROM ...;
START MIGRATION FOR SCHEMA ... FROM ...;
PAUSE MIGRATION FOR TABLE ...;
RESUME MIGRATION FOR TABLE ...;
ABORT MIGRATION FOR TABLE ...;
CUTOVER TABLE ...;
ROLLBACK MIGRATION FOR TABLE ...;

-- Monitoring
SHOW MIGRATION STATUS;
SHOW MIGRATION STATUS FOR TABLE ...;
SHOW MIGRATION ERRORS;
SHOW MIGRATION CONFLICTS;
VALIDATE MIGRATION FOR TABLE ...;

-- Cleanup
DROP MIGRATION SOURCE ...;
DROP SERVER ...;
```

### Migration States

| State | Reads | Writes | CDC |
|-------|-------|--------|-----|
| NOT_STARTED | Remote | Remote | Off |
| BULK_LOADING | Remote | Blocked | Off |
| SYNCHRONIZING | Remote | Remote | On |
| DUAL_WRITE | Local | Both | On |
| CUTOVER_READY | Local | Both | On |
| LOCAL_ONLY | Local | Local | Off |

### Configuration Defaults

| Option | Default | Range |
|--------|---------|-------|
| batch_size | 10,000 | 100 - 1,000,000 |
| parallel_workers | 4 | 1 - 32 |
| rate_limit | 0 (unlimited) | 0 - 10,000,000 |
| cdc_batch_size | 1,000 | 100 - 100,000 |
| validation_sample | 0.01 (1%) | 0 - 1.0 |
