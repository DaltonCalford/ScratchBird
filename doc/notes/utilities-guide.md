# ScratchBird Utilities Guide

## Overview

ScratchBird v0.5.0 includes a comprehensive set of database utilities designed for database administration, monitoring, and maintenance. All utilities are built with modern C++17 standards and feature complete ScratchBird branding.

## Core Database Utilities

### scratchbird - Main Database Server
The primary database server process providing multi-user database access.

**Key Features:**
- Hierarchical schema support (8 levels deep)
- Schema-aware database links
- PostgreSQL-compatible data types
- Multi-generational concurrency control
- Connection pooling and real-time monitoring

**Linux Usage:**
```bash
# Start server in foreground
./scratchbird

# Start with specific configuration
./scratchbird -c /path/to/scratchbird.conf

# Start with custom port
./scratchbird -p 4050

# Show server status
./scratchbird -status

# Run in daemon mode
./scratchbird -d
```

**Windows Usage:**
```bash
# Install as Windows service
scratchbird.exe -install

# Start service
net start ScratchBird

# Run in console mode
scratchbird.exe -f

# Service management
scratchbird.exe -uninstall
```

### sb_isql - Interactive SQL Utility
Interactive SQL command-line interface with schema awareness.

**Key Features:**
- Hierarchical schema navigation
- PostgreSQL-compatible syntax
- Advanced command history
- Schema-aware autocomplete
- Comprehensive help system

**Usage:**
```bash
# Connect to database
./sb_isql -user SYSDBA -password masterkey database.fdb

# Connect with specific schema
./sb_isql -user SYSDBA -password masterkey -schema finance.accounting database.fdb

# Execute SQL file
./sb_isql -input script.sql database.fdb

# Output to file
./sb_isql -output results.txt database.fdb
```

**Schema Commands:**
```sql
-- Set current schema
SET SCHEMA 'finance.accounting';

-- Show current schema
SHOW SCHEMA;

-- Create hierarchical schema
CREATE SCHEMA company.hr.employees;

-- List all schemas
SELECT * FROM RDB$SCHEMAS;
```

### sb_gbak - Advanced Backup/Restore
Database backup and restore utility with schema preservation.

**Key Features:**
- Hierarchical schema preservation
- Incremental backup support
- Cross-platform compatibility
- Schema-selective backup
- Comprehensive metadata handling

**Backup Examples:**
```bash
# Full database backup
./sb_gbak -backup -user SYSDBA -password masterkey database.fdb backup.fbk

# Schema-specific backup
./sb_gbak -backup -schema finance.accounting database.fdb finance_backup.fbk

# Incremental backup
./sb_gbak -backup -incremental -level 1 database.fdb incr_backup.fbk

# Backup with statistics
./sb_gbak -backup -statistics database.fdb backup.fbk
```

**Restore Examples:**
```bash
# Full database restore
./sb_gbak -restore -user SYSDBA -password masterkey backup.fbk new_database.fdb

# Restore with schema mapping
./sb_gbak -restore -schema_mapping old_schema:new_schema backup.fbk database.fdb

# Restore specific schema
./sb_gbak -restore -schema finance.accounting backup.fbk database.fdb
```

### sb_gfix - Database Maintenance
Database validation, repair, and maintenance utility.

**Key Features:**
- Database validation with schema awareness
- Index rebuilding and optimization
- Transaction management
- Schema integrity checking
- Performance tuning

**Usage:**
```bash
# Full database validation
./sb_gfix -validate -user SYSDBA -password masterkey database.fdb

# Schema-specific validation
./sb_gfix -validate -schema finance.accounting database.fdb

# Force database shutdown
./sb_gfix -shutdown -user SYSDBA -password masterkey database.fdb

# Repair database
./sb_gfix -mend -user SYSDBA -password masterkey database.fdb

# Set database to read-only
./sb_gfix -mode read_only database.fdb
```

### sb_gsec - Security Management
User and security management utility.

**Key Features:**
- User account management
- Role-based security
- Schema-level permissions
- Password policy enforcement
- Authentication plugin support

**Usage:**
```bash
# Add user
./sb_gsec -add username -password userpass -user SYSDBA -password masterkey

# Grant schema access
./sb_gsec -grant_schema finance.accounting username

# Modify user
./sb_gsec -modify username -password newpass

# List users
./sb_gsec -display -user SYSDBA -password masterkey

# Delete user
./sb_gsec -delete username
```

### sb_gstat - Database Statistics
Database statistics and performance analysis utility.

**Key Features:**
- Schema-aware statistics
- Performance metrics
- Index analysis
- Table distribution analysis
- Hierarchical schema reporting

**Usage:**
```bash
# Database statistics
./sb_gstat -user SYSDBA -password masterkey database.fdb

# Schema-specific statistics
./sb_gstat -schema finance.accounting database.fdb

# Header information only
./sb_gstat -header database.fdb

# Index statistics
./sb_gstat -index database.fdb

# Table analysis
./sb_gstat -table EMPLOYEES database.fdb
```

## Advanced Utilities

### sb_guard - Process Monitor
Process monitoring and restart utility for server reliability.

**Key Features:**
- Automatic server restart
- Process health monitoring
- Windows service integration
- Configurable restart policies
- Comprehensive logging

**Linux Usage:**
```bash
# Monitor ScratchBird server
./sb_guard ./scratchbird

# Run once (no restart)
./sb_guard -o ./scratchbird

# Run forever with restart
./sb_guard -f ./scratchbird

# Custom server arguments
./sb_guard ./scratchbird -p 4050 -c custom.conf
```

**Windows Usage:**
```bash
# Monitor as Windows service
sb_guard.exe -s scratchbird.exe

# Console mode monitoring
sb_guard.exe scratchbird.exe
```

### sb_svcmgr - Service Manager
Service management interface for administrative tasks.

**Key Features:**
- Database service management
- Schema administration
- Backup scheduling
- Performance monitoring
- Remote administration

**Usage:**
```bash
# Connect to service manager
./sb_svcmgr localhost:service_mgr -user SYSDBA -password masterkey

# Database backup via service
./sb_svcmgr localhost:service_mgr -backup database.fdb backup.fbk

# Schema operations
./sb_svcmgr localhost:service_mgr -schema_info finance.accounting

# User management
./sb_svcmgr localhost:service_mgr -user_add newuser newpass

# Statistics collection
./sb_svcmgr localhost:service_mgr -statistics database.fdb
```

### sb_tracemgr - Trace Manager
Database tracing and performance monitoring utility.

**Key Features:**
- Real-time SQL tracing
- Performance analysis
- Schema access tracking
- Connection monitoring
- Configurable trace sessions

**Usage:**
```bash
# Start trace session
./sb_tracemgr localhost:service_mgr -user SYSDBA -password masterkey \
  -start -name MyTrace -config trace.conf

# List active traces
./sb_tracemgr localhost:service_mgr -user SYSDBA -password masterkey -list

# Stop trace session
./sb_tracemgr localhost:service_mgr -user SYSDBA -password masterkey \
  -stop -sessionid 1

# Suspend/resume trace
./sb_tracemgr localhost:service_mgr -suspend -sessionid 1
./sb_tracemgr localhost:service_mgr -resume -sessionid 1
```

**Trace Configuration Example:**
```
# trace.conf
database = database.fdb
{
    enabled = true
    log_statement_finish = true
    log_procedure_finish = true
    log_context_variables = true
    log_schema_access = true
    max_sql_length = 2048
}
```

### sb_nbackup - Incremental Backup
Incremental and differential backup utility.

**Key Features:**
- Multi-level incremental backups
- Schema-aware backup strategies
- Differential backup support
- Backup validation
- Restore point management

**Usage:**
```bash
# Level 0 backup (full)
./sb_nbackup -backup -level 0 database.fdb backup_level0.nbk

# Level 1 backup (incremental)
./sb_nbackup -backup -level 1 database.fdb backup_level1.nbk

# Restore from incremental
./sb_nbackup -restore backup_level0.nbk backup_level1.nbk database.fdb

# Schema-specific incremental
./sb_nbackup -backup -schema finance.accounting -level 1 database.fdb schema_backup.nbk

# Validate backup
./sb_nbackup -validate backup_level0.nbk backup_level1.nbk
```

### sb_gssplit - File Splitter
File splitting and joining utility for large database files.

**Key Features:**
- Size-based file splitting
- Automatic file joining
- Validation checksums
- Cross-platform compatibility
- Batch processing support

**Usage:**
```bash
# Split file into 1GB chunks
./sb_gssplit -split 1GB large_backup.fbk

# Split with custom naming
./sb_gssplit -split 500MB -prefix backup_part backup.fbk

# Join split files
./sb_gssplit -join backup_part.001 backup_part.002 backup_part.003 restored_backup.fbk

# Validate split files
./sb_gssplit -validate backup_part.001 backup_part.002 backup_part.003
```

### sb_lock_print - Lock Analyzer
Database lock analysis and transaction monitoring utility.

**Key Features:**
- Real-time lock monitoring
- Transaction analysis
- Schema access patterns
- Deadlock detection
- Performance impact analysis

**Usage:**
```bash
# Show current locks
./sb_lock_print database.fdb

# Continuous monitoring
./sb_lock_print -continuous database.fdb

# Schema-specific locks
./sb_lock_print -schema finance.accounting database.fdb

# Transaction details
./sb_lock_print -transactions database.fdb

# Deadlock analysis
./sb_lock_print -deadlocks database.fdb
```

## Common Usage Patterns

### Schema Management Workflow
```bash
# 1. Create hierarchical schema
./sb_isql -user SYSDBA -password masterkey database.fdb
SQL> CREATE SCHEMA company.finance.accounting;

# 2. Backup schema
./sb_gbak -backup -schema company.finance.accounting database.fdb schema_backup.fbk

# 3. Monitor schema usage
./sb_lock_print -schema company.finance.accounting database.fdb

# 4. Collect schema statistics
./sb_gstat -schema company.finance.accounting database.fdb
```

### Performance Monitoring
```bash
# 1. Start trace session
./sb_tracemgr localhost:service_mgr -start -name PerfTrace -config perf.conf

# 2. Monitor locks and transactions
./sb_lock_print -continuous database.fdb

# 3. Collect database statistics
./sb_gstat -header database.fdb

# 4. Stop trace and analyze
./sb_tracemgr localhost:service_mgr -stop -sessionid 1
```

### Backup Strategy
```bash
# 1. Full backup
./sb_gbak -backup database.fdb full_backup.fbk

# 2. Incremental backup
./sb_nbackup -backup -level 0 database.fdb incr_base.nbk
./sb_nbackup -backup -level 1 database.fdb incr_1.nbk

# 3. Split large backups
./sb_gssplit -split 1GB full_backup.fbk

# 4. Validate backups
./sb_gbak -validate full_backup.fbk
```

## Troubleshooting

### Common Issues

**Issue**: Connection refused to database
**Solution**: Ensure ScratchBird server is running
```bash
./scratchbird -status
./sb_guard ./scratchbird  # Start with monitoring
```

**Issue**: Schema not found
**Solution**: Check schema hierarchy and permissions
```bash
./sb_isql -user SYSDBA -password masterkey database.fdb
SQL> SELECT * FROM RDB$SCHEMAS WHERE RDB$SCHEMA_NAME LIKE 'finance%';
```

**Issue**: Backup fails with schema errors
**Solution**: Validate database and schema integrity
```bash
./sb_gfix -validate -schema finance.accounting database.fdb
```

### Performance Tips

1. **Use schema-specific operations** when possible to reduce overhead
2. **Monitor locks regularly** during high-concurrency periods
3. **Implement incremental backups** for large databases
4. **Use trace sessions** to identify performance bottlenecks
5. **Regular statistics collection** helps optimize query plans

## Version Information

All utilities in ScratchBird v0.5.0 report the version string:
```
<utility> version SB-T0.5.0.1 ScratchBird 0.5 f90eae0
```

This ensures consistent versioning and helps identify compatibility issues.

## Next Steps

- [Quick Start Guide](quick-start.md) - Get started with ScratchBird
- [Core Features](core-features.md) - Learn about advanced features
- [Build Instructions](build-instructions.md) - Build from source
- [Architecture Overview](architecture.md) - Understanding the system