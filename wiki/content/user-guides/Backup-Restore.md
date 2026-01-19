# Backup and Restore

**Status:** Alpha documentation
**Last Updated:** 2026-01-19

---

## Overview

This guide covers backup and restore operations from an end-user perspective. Whether you need to back up your development database, restore from a backup, or migrate data between environments, this guide provides practical instructions.

**Topics covered:**
- Quick backup commands
- Restore operations
- Selective backup and restore
- Export and import data
- Common scenarios

For enterprise backup strategies, automated backups, and disaster recovery planning, see the [Administration Guide](../admin/backup-restore.md).

---

## Part 1: Quick Start

### Backup Your Database

**Full database backup:**
```bash
# Backup entire database to a file
sb_backup -U username -d mydb -o backup.sbk

# With compression (recommended)
sb_backup -U username -d mydb -o backup.sbk --compress

# Backup with timestamp in filename
sb_backup -U username -d mydb -o "mydb_$(date +%Y%m%d_%H%M%S).sbk"
```

**Backup specific tables:**
```bash
# Backup only certain tables
sb_backup -U username -d mydb -t customers -t orders -o tables_backup.sbk
```

### Restore Your Database

**Restore to existing database:**
```bash
# Restore backup to database
sb_restore -U username -d mydb backup.sbk

# Restore with clean (drop existing objects first)
sb_restore -U username -d mydb --clean backup.sbk
```

**Restore to new database:**
```bash
# Create database and restore
sb_restore -U username --create -d newdb backup.sbk
```

---

## Part 2: Backup Operations

### sb_backup Command Reference

```bash
sb_backup [OPTIONS] -d DATABASE -o OUTPUT_FILE

Options:
  -U, --username USER    Database username
  -d, --database DB      Database to backup
  -o, --output FILE      Output backup file
  -H, --host HOST        Database host (default: localhost)
  -p, --port PORT        Database port (default: 3092)
  -t, --table TABLE      Backup specific table (can repeat)
  -n, --schema SCHEMA    Backup specific schema (can repeat)
  --schema-only          Backup schema without data
  --data-only            Backup data without schema
  --compress             Compress backup file
  --format FORMAT        Backup format: sbk (default), sql, csv
  --verbose              Show detailed progress
  --help                 Show help message
```

### Backup Formats

**SBK Format (Default):**
```bash
# Native ScratchBird backup format
# - Binary format, efficient and fast
# - Supports all features
# - Recommended for full backups
sb_backup -U admin -d mydb -o backup.sbk
```

**SQL Format:**
```bash
# Plain SQL dump
# - Human-readable
# - Can be edited
# - Portable to other databases
sb_backup -U admin -d mydb -o backup.sql --format=sql
```

**CSV Format:**
```bash
# CSV export (data only)
# - One file per table
# - Good for data exchange
# - Creates directory with CSV files
sb_backup -U admin -d mydb -o backup_dir/ --format=csv
```

### Backup Specific Objects

**Tables only:**
```bash
# Single table
sb_backup -U admin -d mydb -t customers -o customers.sbk

# Multiple tables
sb_backup -U admin -d mydb -t customers -t orders -t products -o tables.sbk

# Tables matching pattern (via shell)
for table in $(sb_isql -U admin -d mydb -c "\dt" | grep "order"); do
    sb_backup -U admin -d mydb -t "$table" -o "${table}.sbk"
done
```

**Schema only:**
```bash
# Specific schema
sb_backup -U admin -d mydb -n sales -o sales_schema.sbk

# Multiple schemas
sb_backup -U admin -d mydb -n sales -n inventory -o schemas.sbk
```

**Schema without data:**
```bash
# DDL only - useful for setting up new environments
sb_backup -U admin -d mydb --schema-only -o schema.sbk
```

**Data without schema:**
```bash
# Data only - useful for refreshing test data
sb_backup -U admin -d mydb --data-only -o data.sbk
```

### Backup Progress and Verification

**Show progress:**
```bash
sb_backup -U admin -d mydb -o backup.sbk --verbose
# Output:
# Backing up database: mydb
# Table customers: 10,000 rows
# Table orders: 50,000 rows
# Table products: 1,000 rows
# ...
# Backup complete: backup.sbk (15.2 MB)
```

**Verify backup:**
```bash
# List contents of backup
sb_backup --list backup.sbk

# Verify backup integrity
sb_backup --verify backup.sbk
```

---

## Part 3: Restore Operations

### sb_restore Command Reference

```bash
sb_restore [OPTIONS] -d DATABASE BACKUP_FILE

Options:
  -U, --username USER    Database username
  -d, --database DB      Target database
  -H, --host HOST        Database host (default: localhost)
  -p, --port PORT        Database port (default: 3092)
  --create               Create database before restore
  --clean                Drop existing objects before restore
  --data-only            Restore data only (schema must exist)
  --schema-only          Restore schema only (no data)
  -t, --table TABLE      Restore specific table only
  -n, --schema SCHEMA    Restore specific schema only
  --no-owner             Don't restore ownership
  --no-privileges        Don't restore privileges
  --verbose              Show detailed progress
  --help                 Show help message
```

### Basic Restore Operations

**Restore to existing database:**
```bash
# Simple restore (adds to existing objects)
sb_restore -U admin -d mydb backup.sbk

# Clean restore (drops and recreates objects)
sb_restore -U admin -d mydb --clean backup.sbk
```

**Restore to new database:**
```bash
# Create database and restore
sb_restore -U admin --create -d newdb backup.sbk

# Or manually:
sb_isql -U admin -c "CREATE DATABASE newdb"
sb_restore -U admin -d newdb backup.sbk
```

**Restore from SQL dump:**
```bash
# SQL format can be restored with sb_isql
sb_isql -U admin -d mydb -f backup.sql

# Or via PostgreSQL protocol
psql -h localhost -p 5432 -U admin -d mydb -f backup.sql
```

### Selective Restore

**Restore specific tables:**
```bash
# Single table
sb_restore -U admin -d mydb -t customers backup.sbk

# Multiple tables
sb_restore -U admin -d mydb -t customers -t orders backup.sbk
```

**Restore specific schema:**
```bash
sb_restore -U admin -d mydb -n sales backup.sbk
```

**Restore schema only (no data):**
```bash
sb_restore -U admin -d mydb --schema-only backup.sbk
```

**Restore data only (schema must exist):**
```bash
sb_restore -U admin -d mydb --data-only backup.sbk
```

### Restore Options

**Skip ownership and privileges:**
```bash
# Useful when restoring to different user/environment
sb_restore -U admin -d mydb --no-owner --no-privileges backup.sbk
```

**Restore with progress:**
```bash
sb_restore -U admin -d mydb --verbose backup.sbk
# Output:
# Restoring to database: mydb
# Creating table: customers
# Loading data: customers (10,000 rows)
# Creating table: orders
# Loading data: orders (50,000 rows)
# Creating indexes...
# Creating foreign keys...
# Restore complete
```

---

## Part 4: Export and Import

### Using COPY Command

**Export table to CSV:**
```sql
-- Export entire table
COPY customers TO '/path/to/customers.csv' WITH (FORMAT CSV, HEADER true);

-- Export with specific columns
COPY (SELECT id, name, email FROM customers)
TO '/path/to/customers_partial.csv'
WITH (FORMAT CSV, HEADER true);

-- Export query results
COPY (
    SELECT c.name, COUNT(o.id) as order_count
    FROM customers c
    LEFT JOIN orders o ON c.id = o.customer_id
    GROUP BY c.name
)
TO '/path/to/customer_orders.csv'
WITH (FORMAT CSV, HEADER true);
```

**Import from CSV:**
```sql
-- Import entire file
COPY customers FROM '/path/to/customers.csv' WITH (FORMAT CSV, HEADER true);

-- Import specific columns
COPY customers (id, name, email)
FROM '/path/to/customers.csv'
WITH (FORMAT CSV, HEADER true);

-- Import with options
COPY customers FROM '/path/to/customers.csv'
WITH (
    FORMAT CSV,
    HEADER true,
    DELIMITER ',',
    NULL 'NULL',
    QUOTE '"'
);
```

### Using sb_isql for Export

**Export to CSV:**
```bash
# Export query results
sb_isql -U admin -d mydb -c "SELECT * FROM customers" --csv > customers.csv

# Export with specific format
sb_isql -U admin -d mydb -c "\COPY customers TO 'customers.csv' CSV HEADER"
```

**Export to JSON:**
```bash
# Export as JSON (requires json_agg)
sb_isql -U admin -d mydb -c "
SELECT json_agg(row_to_json(customers))
FROM customers
" > customers.json
```

### Bulk Data Operations

**Fast bulk insert:**
```sql
-- Disable indexes during bulk load
ALTER TABLE customers DISABLE TRIGGER ALL;

-- Load data
COPY customers FROM '/path/to/data.csv' WITH (FORMAT CSV);

-- Re-enable triggers and rebuild indexes
ALTER TABLE customers ENABLE TRIGGER ALL;
REINDEX TABLE customers;
ANALYZE customers;
```

**Insert from another table:**
```sql
-- Copy data between tables
INSERT INTO customers_archive
SELECT * FROM customers WHERE created_at < '2025-01-01';

-- Copy with transformation
INSERT INTO customers_normalized (id, full_name, email)
SELECT id, first_name || ' ' || last_name, LOWER(email)
FROM customers_raw;
```

---

## Part 5: Common Scenarios

### Development Database Refresh

**Refresh dev from production:**
```bash
# 1. Backup production (run on prod server)
sb_backup -U admin -d proddb -o prod_backup.sbk --compress

# 2. Transfer backup to dev server
scp prod_backup.sbk dev-server:/backups/

# 3. Restore to dev (on dev server)
sb_restore -U admin -d devdb --clean prod_backup.sbk

# 4. Anonymize sensitive data
sb_isql -U admin -d devdb -c "
UPDATE customers SET
    email = 'user' || id || '@example.com',
    phone = '555-0000',
    address = '123 Test St'
"
```

### Clone Database

**Create a copy of database:**
```bash
# Method 1: Backup and restore
sb_backup -U admin -d mydb -o temp.sbk
sb_restore -U admin --create -d mydb_copy temp.sbk
rm temp.sbk

# Method 2: Using CREATE DATABASE (if supported)
sb_isql -U admin -c "CREATE DATABASE mydb_copy TEMPLATE mydb"
```

### Migrate Between Servers

**Move database to new server:**
```bash
# On source server
sb_backup -U admin -d mydb -o migration.sbk --compress

# Transfer to target
scp migration.sbk target-server:/tmp/

# On target server
sb_restore -U admin --create -d mydb /tmp/migration.sbk
```

### Partial Data Restore

**Restore only recent data:**
```bash
# 1. Backup only recent orders
sb_isql -U admin -d proddb -c "
COPY (SELECT * FROM orders WHERE created_at > '2026-01-01')
TO '/tmp/recent_orders.csv' CSV HEADER
"

# 2. Import to target database
sb_isql -U admin -d devdb -c "
COPY orders FROM '/tmp/recent_orders.csv' CSV HEADER
"
```

### Disaster Recovery Test

**Test your backups regularly:**
```bash
#!/bin/bash
# backup_test.sh - Test backup integrity

BACKUP_FILE=$1
TEST_DB="test_restore_$(date +%s)"

echo "Testing backup: $BACKUP_FILE"

# Create test database and restore
sb_restore -U admin --create -d "$TEST_DB" "$BACKUP_FILE"

# Run validation queries
sb_isql -U admin -d "$TEST_DB" -c "
SELECT
    (SELECT COUNT(*) FROM customers) AS customers,
    (SELECT COUNT(*) FROM orders) AS orders,
    (SELECT COUNT(*) FROM products) AS products
"

# Cleanup
sb_isql -U admin -c "DROP DATABASE $TEST_DB"

echo "Backup test complete"
```

---

## Part 6: Working with Different Protocols

### PostgreSQL Protocol

**Backup via pg_dump:**
```bash
# Using PostgreSQL tools with ScratchBird
pg_dump -h localhost -p 5432 -U admin -d mydb -f backup.sql

# Restore with psql
psql -h localhost -p 5432 -U admin -d mydb -f backup.sql
```

### MySQL Protocol

**Backup via mysqldump:**
```bash
# Using MySQL tools with ScratchBird
mysqldump -h localhost -P 3306 -u admin -p mydb > backup.sql

# Restore with mysql
mysql -h localhost -P 3306 -u admin -p mydb < backup.sql
```

### Firebird Protocol

**Backup via gbak:**
```bash
# Using Firebird tools with ScratchBird
gbak -b -user SYSDBA -password masterkey localhost:mydb backup.fbk

# Restore
gbak -r -user SYSDBA -password masterkey backup.fbk localhost:mydb_new
```

---

## Part 7: Troubleshooting

### Common Issues

**Backup fails with "permission denied":**
```bash
# Check file permissions
ls -la /backup/path/

# Ensure directory exists and is writable
mkdir -p /backup/path
chmod 755 /backup/path

# Or backup to a different location
sb_backup -U admin -d mydb -o ~/backups/mydb.sbk
```

**Restore fails with "database exists":**
```bash
# Option 1: Use --clean to drop existing objects
sb_restore -U admin -d mydb --clean backup.sbk

# Option 2: Drop and recreate database
sb_isql -U admin -c "DROP DATABASE mydb"
sb_restore -U admin --create -d mydb backup.sbk
```

**Restore fails with "foreign key violation":**
```bash
# Disable foreign key checks during restore
sb_restore -U admin -d mydb --disable-triggers backup.sbk

# Or restore in correct order
sb_restore -U admin -d mydb -t parent_table backup.sbk
sb_restore -U admin -d mydb -t child_table backup.sbk
```

**Out of disk space:**
```bash
# Check disk space
df -h

# Use compression
sb_backup -U admin -d mydb -o backup.sbk --compress

# Backup to different location
sb_backup -U admin -d mydb -o /mnt/external/backup.sbk
```

### Verify Backup Contents

```bash
# List tables in backup
sb_backup --list backup.sbk

# Sample output:
# Tables:
#   customers (10,000 rows, 1.2 MB)
#   orders (50,000 rows, 5.8 MB)
#   products (1,000 rows, 0.3 MB)
# Total: 3 tables, 61,000 rows, 7.3 MB

# Verify backup integrity
sb_backup --verify backup.sbk
# Output: Backup verified successfully
```

### Restore to Specific Point

```bash
# If using incremental backups, restore in order
sb_restore -U admin -d mydb full_backup.sbk
sb_restore -U admin -d mydb --data-only incremental_1.sbk
sb_restore -U admin -d mydb --data-only incremental_2.sbk
```

---

## Part 8: Best Practices

### Backup Naming Conventions

```bash
# Include database name and timestamp
mydb_20260119_143000.sbk

# Include backup type
mydb_full_20260119.sbk
mydb_schema_20260119.sbk
mydb_data_20260119.sbk

# Include environment
prod_mydb_20260119.sbk
staging_mydb_20260119.sbk
```

### Backup Script Example

```bash
#!/bin/bash
# daily_backup.sh

DB_NAME="mydb"
BACKUP_DIR="/backups"
RETENTION_DAYS=7
DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_FILE="${BACKUP_DIR}/${DB_NAME}_${DATE}.sbk"

# Create backup
sb_backup -U admin -d "$DB_NAME" -o "$BACKUP_FILE" --compress

# Verify backup
if sb_backup --verify "$BACKUP_FILE"; then
    echo "Backup successful: $BACKUP_FILE"
else
    echo "Backup verification failed!"
    exit 1
fi

# Remove old backups
find "$BACKUP_DIR" -name "${DB_NAME}_*.sbk" -mtime +$RETENTION_DAYS -delete

echo "Backup complete"
```

### Before Major Changes

```bash
# Always backup before:
# - Schema migrations
# - Bulk data updates
# - Version upgrades
# - Configuration changes

# Quick pre-change backup
sb_backup -U admin -d mydb -o "pre_migration_$(date +%Y%m%d_%H%M%S).sbk"

# Make your changes...

# If something goes wrong:
sb_restore -U admin -d mydb --clean pre_migration_*.sbk
```

---

## Quick Reference

### Essential Commands

| Task | Command |
|------|---------|
| Full backup | `sb_backup -U admin -d mydb -o backup.sbk` |
| Compressed backup | `sb_backup -U admin -d mydb -o backup.sbk --compress` |
| Schema only | `sb_backup -U admin -d mydb -o schema.sbk --schema-only` |
| Specific table | `sb_backup -U admin -d mydb -t orders -o orders.sbk` |
| Restore | `sb_restore -U admin -d mydb backup.sbk` |
| Clean restore | `sb_restore -U admin -d mydb --clean backup.sbk` |
| Create and restore | `sb_restore -U admin --create -d newdb backup.sbk` |
| List backup | `sb_backup --list backup.sbk` |
| Verify backup | `sb_backup --verify backup.sbk` |

### File Size Guidelines

| Database Size | Backup Size (compressed) | Backup Time |
|---------------|-------------------------|-------------|
| < 100 MB | ~20-40 MB | < 1 min |
| 100 MB - 1 GB | ~40-400 MB | 1-5 min |
| 1 GB - 10 GB | ~400 MB - 4 GB | 5-30 min |
| > 10 GB | Varies | Use incremental |

---

## See Also

- [Administration Backup Guide](../admin/backup-restore.md) - Enterprise backup strategies
- [Performance Tuning](Performance-Tuning.md) - Optimize backup performance
- [Troubleshooting](../troubleshooting/) - Common issues
- [sb_backup Reference](../cli-tools/sb-backup.md) - Full command reference

