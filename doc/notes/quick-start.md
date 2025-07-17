# ScratchBird Quick Start Guide

## Installation

### Linux x86_64
```bash
# Download release package
wget https://github.com/dcalford/ScratchBird/releases/download/v0.5.0/scratchbird-v0.5.0-linux-x86_64.tar.gz

# Extract and install
tar -xzf scratchbird-v0.5.0-linux-x86_64.tar.gz
cd scratchbird-v0.5.0-linux-x86_64
sudo ./install.sh
```

### Windows x64
```bash
# Download from releases page:
# https://github.com/dcalford/ScratchBird/releases/download/v0.5.0/scratchbird-v0.5.0-windows-x64.zip

# Extract and run installer.exe as administrator
```

## First Steps

### Verify Installation
```bash
# Check version
sb_isql -z

# Expected output:
# sb_isql version SB-T0.5.0.1 ScratchBird 0.5 f90eae0
```

### Create Your First Database
```bash
# Start ISQL
sb_isql

# Create database
SQL> CREATE DATABASE '/path/to/mydb.fdb';
SQL> CONNECT '/path/to/mydb.fdb';
```

## Try New Features

### Hierarchical Schemas (New in v0.5.0)
```sql
-- Create nested schema hierarchy
CREATE SCHEMA finance;
CREATE SCHEMA finance.accounting;
CREATE SCHEMA finance.accounting.reports;

-- Create table in nested schema
CREATE TABLE finance.accounting.reports.monthly_summary (
    id INTEGER,
    month_name VARCHAR(20),
    total_amount DECIMAL(15,2)
);

-- Insert data
INSERT INTO finance.accounting.reports.monthly_summary 
VALUES (1, 'January', 50000.00);

-- Query with full hierarchical path
SELECT * FROM finance.accounting.reports.monthly_summary;
```

### Schema Navigation
```sql
-- Set current schema context
SET SCHEMA 'finance.accounting';

-- Now you can reference tables directly
SELECT * FROM reports.monthly_summary;

-- View current schema
SHOW SCHEMA;
```

## Basic Operations

### Database Backup
```bash
sb_gbak -b -user SYSDBA -password masterkey /path/to/mydb.fdb backup.fbk
```

### Database Restore
```bash
sb_gbak -c backup.fbk /path/to/restored.fdb
```

### Database Statistics
```bash
sb_gstat -h -user SYSDBA -password masterkey /path/to/mydb.fdb
```

## What's New in v0.5.0

1. **Hierarchical Schema Support**: 8-level deep schema nesting
2. **Schema-Aware Database Links**: Remote schema targeting (demonstration)
3. **2PC External Data Sources**: Distributed transaction framework
4. **Complete ScratchBird Branding**: Zero Firebird references
5. **GPRE-Free Utilities**: 96.3% code reduction, modern C++17
6. **PostgreSQL-Compatible Data Types**: Network types, unsigned integers, range types
7. **UUID IDENTITY Columns**: Automatic UUID generation with versioning
8. **PascalCase Object Identifiers**: SQL Server-style case-insensitive mode

## Next Steps

- Read [Core Features](core-features.md) for detailed feature documentation
- See [Build Instructions](build-instructions.md) for compilation guide
- Check [Migration Guide](migration-guide.md) for upgrading from Firebird