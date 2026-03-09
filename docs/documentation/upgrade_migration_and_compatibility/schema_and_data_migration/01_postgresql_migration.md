# PostgreSQL Migration Guide

[Schema and Data Migration README](../README.md)

## Synopsis

Migrate databases from PostgreSQL to ScratchBird.

## Migration Methods

### Method 1: Logical Dump and Restore (Recommended)

```bash
# 1. Extract schema from PostgreSQL
pg_dump -h pg_host -U postgres --schema-only mydb > mydb_schema.sql

# 2. Extract data
pg_dump -h pg_host -U postgres --data-only --inserts mydb > mydb_data.sql

# 3. Create database in ScratchBird
sb_isql -c "CREATE DATABASE mydb;"

# 4. Load schema (may need adjustments)
sb_pg_isql -d mydb -f mydb_schema.sql 2>&1 | tee schema_load.log

# 5. Load data
sb_pg_isql -d mydb -f mydb_data.sql 2>&1 | tee data_load.log
```

### Method 2: FDW Incremental Migration

```sql
-- 1. Set up FDW in ScratchBird
CREATE EXTENSION postgres_fdw;

CREATE SERVER pg_source
    FOREIGN DATA WRAPPER postgres_fdw
    OPTIONS (host 'pg_host', port '5432', dbname 'mydb');

CREATE USER MAPPING FOR current_user
    SERVER pg_source
    OPTIONS (user 'postgres', password 'secret');

-- 2. Import schema
IMPORT FOREIGN SCHEMA public
    FROM SERVER pg_source
    INTO source_schema;

-- 3. Create local tables
CREATE TABLE local_users (LIKE source_schema.users INCLUDING ALL);

-- 4. Migrate data in batches
INSERT INTO local_users
SELECT * FROM source_schema.users
WHERE created_at < '2024-01-01';

-- 5. Verify
SELECT count(*) FROM local_users;
SELECT count(*) FROM source_schema.users;
```

### Method 3: Live Replication

```bash
# Use migration proxy
sb_migrate_proxy \
    --source-host=pg_host \
    --source-port=5432 \
    --target-environment=!:prod \
    --target-database=mydb \
    --replicate-mode=continuous
```

## Schema Conversion

### Automatic Conversions

| PostgreSQL | ScratchBird | Status |
|------------|-------------|--------|
| `SERIAL` | `SERIAL` | ✅ Automatic |
| `BIGSERIAL` | `BIGSERIAL` | ✅ Automatic |
| `VARCHAR(n)` | `VARCHAR(n)` | ✅ Automatic |
| `TEXT` | `TEXT` | ✅ Automatic |
| `TIMESTAMPTZ` | `TIMESTAMPTZ` | ✅ Automatic |
| `JSONB` | `JSONB` | ✅ Automatic |
| `UUID` | `UUID` | ✅ Automatic (v7) |

### Manual Adjustments

| PostgreSQL | ScratchBird | Action |
|------------|-------------|--------|
| `CITEXT` | `TEXT` + lowercase CHECK | Add functional index |
| `HSTORE` | `JSONB` | Convert data |
| Custom types | Domain or composite | Recreate manually |
| `pg_trgm` indexes | GIN indexes | May need adjustment |

## Data Type Mapping

```sql
-- Check type mappings
SELECT 
    column_name,
    data_type as pg_type,
    CASE data_type
        WHEN 'citext' THEN 'TEXT'
        WHEN 'hstore' THEN 'JSONB'
        ELSE data_type
    END as sb_type
FROM information_schema.columns
WHERE table_schema = 'public';
```

## Index Migration

```sql
-- PostgreSQL
CREATE INDEX idx_users_email ON users USING btree(email);
CREATE INDEX idx_logs_data ON logs USING gin(data);

-- ScratchBird (same syntax)
CREATE INDEX idx_users_email ON users(email);
CREATE INDEX idx_logs_data ON logs USING GIN (data);
```

## Function Migration

### PL/pgSQL Compatibility

Most PL/pgSQL functions work without changes:

```sql
-- This works in both
CREATE OR REPLACE FUNCTION get_user_count()
RETURNS INTEGER AS $$
DECLARE
    v_count INTEGER;
BEGIN
    SELECT COUNT(*) INTO v_count FROM users;
    RETURN v_count;
END;
$$ LANGUAGE plpgsql;
```

### Functions Requiring Changes

| PostgreSQL | ScratchBird | Change |
|------------|-------------|--------|
| `pg_sleep()` | `pg_sleep()` | Same |
| `uuid-ossp` | Built-in | Use `gen_random_uuid()` |
| `pgcrypto` | Built-in | Hash functions built-in |

## Verification

```bash
# Row counts
sb_pg_isql -d mydb -c "SELECT 'users' as table, count(*) FROM users
  UNION ALL
  SELECT 'orders', count(*) FROM orders;"

# Compare with PostgreSQL
psql -h pg_host -U postgres -d mydb -c "SELECT 'users' as table, count(*) FROM users
  UNION ALL
  SELECT 'orders', count(*) FROM orders;"

# Checksum verification
sb_pg_isql -d mydb -c "SELECT md5(string_agg(id::text, ',' ORDER BY id)) FROM users;"
```

## Post-Migration

1. **Update Connection Strings**
   ```
   # Old (PostgreSQL)
   postgresql://user:pass@pg_host:5432/mydb
   
   # New (ScratchBird)
   postgresql://user:pass@sb_host:5432/mydb
   ```

2. **Verify Application Compatibility**
   - Test all queries
   - Verify transaction handling
   - Check error handling

3. **Performance Tuning**
   - Analyze tables: `ANALYZE;`
   - Review execution plans
   - Adjust configuration

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Extension not found | Check SB extension list |
| Type mismatch | Cast explicitly |
| Index failure | Recreate with SB syntax |
| Permission denied | Grant appropriate privileges |

## See Also

- [MySQL Migration](02_mysql_migration.md)
- [Firebird Migration](03_firebird_migration.md)
