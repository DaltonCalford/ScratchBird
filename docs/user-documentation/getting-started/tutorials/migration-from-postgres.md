# Tutorial: Migrating from PostgreSQL

Migrate your existing PostgreSQL database to ScratchBird.

[Back to Getting Started](../index.md) | [Back to Documentation Index](../../index.md)

---

## Overview

ScratchBird is highly compatible with PostgreSQL, making migration straightforward. This guide covers:

- Schema migration
- Data migration
- Application changes
- Testing and validation

---

## Prerequisites

- Access to source PostgreSQL database
- ScratchBird installed and running
- `pg_dump` and `psql` tools
- Sufficient disk space for dump files

---

## Migration Approaches

| Approach | Best For | Downtime |
|----------|----------|----------|
| **Dump and Restore** | Small to medium databases | Minutes to hours |
| **Logical Replication** | Large databases, minimal downtime | Seconds |
| **Direct Copy** | Development/testing | Varies |

This tutorial covers the dump and restore approach.

---

## Step 1: Assess Your Database

### Check Database Size

```bash
psql -h source-host -U postgres -c "
SELECT
    pg_database.datname,
    pg_size_pretty(pg_database_size(pg_database.datname)) AS size
FROM pg_database
WHERE datname = 'your_database';"
```

### List Tables and Sizes

```bash
psql -h source-host -U postgres -d your_database -c "
SELECT
    schemaname,
    tablename,
    pg_size_pretty(pg_total_relation_size(schemaname || '.' || tablename)) AS size
FROM pg_tables
WHERE schemaname NOT IN ('pg_catalog', 'information_schema')
ORDER BY pg_total_relation_size(schemaname || '.' || tablename) DESC;"
```

### Check for Unsupported Features

```sql
-- Extensions in use
SELECT * FROM pg_extension;

-- Custom types
SELECT typname, typtype FROM pg_type
WHERE typnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'public');

-- Foreign data wrappers
SELECT * FROM pg_foreign_data_wrapper;

-- Tablespaces
SELECT * FROM pg_tablespace WHERE spcname NOT IN ('pg_default', 'pg_global');
```

---

## Step 2: Export Schema

### Export Schema Only

```bash
pg_dump -h source-host -U postgres -d your_database \
    --schema-only \
    --no-owner \
    --no-privileges \
    -f schema.sql
```

### Review Schema for Compatibility

Check `schema.sql` for:

```bash
# PostgreSQL-specific features to review
grep -E "USING (btree|hash|gist|gin|brin)" schema.sql
grep -E "TABLESPACE" schema.sql
grep -E "CREATE EXTENSION" schema.sql
grep -E "CREATE TYPE" schema.sql
```

### Common Schema Adjustments

**Extensions:**
```sql
-- Remove or comment out extension-specific features
-- CREATE EXTENSION pg_trgm;  -- Comment if not needed
```

**Index Types:**
ScratchBird supports all common PostgreSQL index types:
- BTREE (default)
- HASH
- GIN
- GIST
- BRIN

**Sequences:**
```sql
-- PostgreSQL
CREATE SEQUENCE myseq AS BIGINT;

-- ScratchBird (compatible)
CREATE SEQUENCE myseq AS BIGINT;
```

---

## Step 3: Export Data

### Full Export with Data

```bash
pg_dump -h source-host -U postgres -d your_database \
    --no-owner \
    --no-privileges \
    --inserts \
    -f full_dump.sql
```

### Export Large Tables Separately

For large tables, export separately:

```bash
# Schema for large table
pg_dump -h source-host -U postgres -d your_database \
    --table=large_table --schema-only -f large_table_schema.sql

# Data as COPY commands (faster)
pg_dump -h source-host -U postgres -d your_database \
    --table=large_table --data-only -f large_table_data.sql
```

### Parallel Export

```bash
# Export to directory format (parallel)
pg_dump -h source-host -U postgres -d your_database \
    -j 4 \
    -F d \
    -f dump_directory
```

---

## Step 4: Create Target Database

### Connect to ScratchBird

```bash
# Using PostgreSQL protocol (port 5432)
psql -h localhost -p 5432 -U admin
```

### Create Database

```sql
CREATE DATABASE your_database;
\c your_database
```

---

## Step 5: Import Schema

### Apply Schema

```bash
psql -h localhost -p 5432 -U admin -d your_database -f schema.sql
```

### Verify Tables

```bash
psql -h localhost -p 5432 -U admin -d your_database -c "\dt"
```

### Fix Any Errors

Common fixes:

```sql
-- If extension not available, create workaround
-- Example: uuid-ossp replacement
CREATE FUNCTION gen_random_uuid() RETURNS UUID AS $$
    SELECT uuid_generate_v4()
$$ LANGUAGE SQL;

-- If custom type not supported, use standard type
-- ALTER TABLE ... ALTER COLUMN ... TYPE ...
```

---

## Step 6: Import Data

### Full Import

```bash
psql -h localhost -p 5432 -U admin -d your_database -f full_dump.sql
```

### Monitor Progress

In another terminal:

```bash
psql -h localhost -p 5432 -U admin -d your_database -c "
SELECT relname, n_live_tup
FROM pg_stat_user_tables
ORDER BY n_live_tup DESC;"
```

### Import Large Tables

For large tables exported separately:

```bash
# Apply data
psql -h localhost -p 5432 -U admin -d your_database -f large_table_data.sql
```

---

## Step 7: Validate Migration

### Row Counts

Compare row counts between source and target:

**On PostgreSQL:**
```sql
SELECT 'users' AS table_name, COUNT(*) FROM users
UNION ALL
SELECT 'orders', COUNT(*) FROM orders
UNION ALL
SELECT 'products', COUNT(*) FROM products;
```

**On ScratchBird:**
```sql
-- Same query
SELECT 'users' AS table_name, COUNT(*) FROM users
UNION ALL
SELECT 'orders', COUNT(*) FROM orders
UNION ALL
SELECT 'products', COUNT(*) FROM products;
```

### Sample Data Verification

```sql
-- Compare sample records
SELECT * FROM users ORDER BY id LIMIT 10;
SELECT * FROM orders ORDER BY id LIMIT 10;
```

### Constraint Validation

```sql
-- Check foreign key integrity
SELECT
    tc.table_name,
    tc.constraint_name,
    tc.constraint_type
FROM information_schema.table_constraints tc
WHERE tc.constraint_type IN ('PRIMARY KEY', 'FOREIGN KEY', 'UNIQUE')
ORDER BY tc.table_name;
```

### Index Verification

```sql
-- List indexes
SELECT
    tablename,
    indexname,
    indexdef
FROM pg_indexes
WHERE schemaname = 'public'
ORDER BY tablename, indexname;
```

---

## Step 8: Update Application

### Connection String Changes

**Before (PostgreSQL):**
```
postgresql://user:pass@pg-server:5432/mydb
```

**After (ScratchBird - same format works!):**
```
postgresql://user:pass@sb-server:5432/mydb
```

### Driver Compatibility

ScratchBird's PostgreSQL protocol is compatible with standard drivers:

| Language | Driver | Status |
|----------|--------|--------|
| Python | psycopg2, asyncpg | Compatible |
| Node.js | pg, node-postgres | Compatible |
| Java | JDBC PostgreSQL | Compatible |
| Go | lib/pq, pgx | Compatible |
| Ruby | pg gem | Compatible |
| PHP | PDO PostgreSQL | Compatible |

### Python Example

```python
# No code changes needed!
import psycopg2

# Just change the host
conn = psycopg2.connect(
    host="scratchbird-server",  # Changed from pg-server
    port=5432,
    database="mydb",
    user="admin",
    password="secret"
)
```

### Node.js Example

```javascript
const { Pool } = require('pg');

// Just change the host
const pool = new Pool({
    host: 'scratchbird-server',  // Changed from pg-server
    port: 5432,
    database: 'mydb',
    user: 'admin',
    password: 'secret'
});
```

---

## Step 9: Testing

### Functional Tests

Run your application's test suite against ScratchBird:

```bash
# Set environment to use ScratchBird
export DATABASE_URL="postgresql://admin:pass@localhost:5432/mydb_test"

# Run tests
npm test  # or pytest, etc.
```

### Performance Testing

Compare query performance:

```sql
-- Enable timing in sb_isql
\timing on

-- Run sample queries
SELECT * FROM large_table WHERE indexed_column = 'value';
EXPLAIN ANALYZE SELECT * FROM complex_query...;
```

### Load Testing

```bash
# Use pgbench (compatible with ScratchBird)
pgbench -h localhost -p 5432 -U admin -d mydb -c 10 -j 2 -T 60
```

---

## Step 10: Cutover Checklist

### Pre-Cutover

- [ ] All schema migrated
- [ ] All data migrated
- [ ] Row counts match
- [ ] Sample data verified
- [ ] Application tests passing
- [ ] Performance acceptable
- [ ] Backups of both systems

### Cutover Steps

1. **Stop writes to PostgreSQL**
   ```sql
   -- On PostgreSQL
   ALTER DATABASE mydb SET default_transaction_read_only = on;
   ```

2. **Final sync** (if using incremental)
   ```bash
   # Export changes since last sync
   pg_dump --data-only --inserts ... -f delta.sql

   # Apply to ScratchBird
   psql -h sb-server -f delta.sql
   ```

3. **Verify final counts**

4. **Update DNS/Load Balancer**
   - Point application to ScratchBird

5. **Monitor**
   - Watch logs for errors
   - Monitor performance

### Rollback Plan

If issues occur:

1. Update DNS back to PostgreSQL
2. Investigate issues
3. Re-sync any new data
4. Retry cutover

---

## Common Issues

### Syntax Differences

Most PostgreSQL syntax works, but check:

```sql
-- PostgreSQL-specific
SELECT * FROM table TABLESAMPLE BERNOULLI(10);
-- May need alternative approach in ScratchBird
```

### Extension Features

If using PostgreSQL extensions:

| Extension | ScratchBird Alternative |
|-----------|------------------------|
| `pg_trgm` | Built-in LIKE with indexes |
| `postgis` | Native spatial types (if GEOS enabled) |
| `uuid-ossp` | Built-in `gen_random_uuid()` |
| `hstore` | Use JSON/JSONB |

### Performance Differences

If queries are slower:

1. Check index usage: `EXPLAIN ANALYZE`
2. Update statistics: `ANALYZE tablename;`
3. Review query plans
4. Consider ScratchBird-specific optimizations

---

## Summary

Migration from PostgreSQL to ScratchBird is straightforward due to wire protocol compatibility:

1. Export schema and data with `pg_dump`
2. Create target database
3. Import with `psql` (using PostgreSQL port)
4. Validate data
5. Update connection strings
6. Test application
7. Cutover

Most applications require only a connection string change.

---

## Next Steps

1. [Configure security](../../admin/security.md)
2. [Set up backups](../../admin/backup-restore.md)
3. [Performance tuning](../../admin/performance-tuning.md)
4. [Monitoring](../../admin/monitoring.md)
