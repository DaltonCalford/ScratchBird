# ScratchBird Utilities - Complete Reference Documentation

## Overview

**ScratchBird Utilities** are command-line tools that provide comprehensive database administration, maintenance, and development capabilities. These utilities have been enhanced with modern features while maintaining 100% backward compatibility with Firebird tools. All utilities support ScratchBird's advanced features including hierarchical schemas, database links, and modern security frameworks.

### Utility Suite Features

ScratchBird provides enterprise-grade command-line utilities with advanced capabilities:

- **Complete Firebird Compatibility**: 100% backward compatibility with existing Firebird workflows
- **Hierarchical Schema Support**: Full support for nested schemas and qualified names
- **Database Link Integration**: Schema-aware remote database connectivity
- **Modern Compression**: LZ4, ZSTD, GZIP, BZIP2 support across utilities
- **Advanced Security**: Enhanced authentication, encryption, and auditing
- **Parallel Processing**: Multi-threaded operations for improved performance
- **Enterprise Features**: Comprehensive monitoring, validation, and management tools

---

## Core Database Utilities

### sb_isql - Interactive SQL Tool

**Purpose**: Interactive SQL command-line interface for database development and administration.

#### **Basic Usage**
```bash
# Connect to database
sb_isql -user sysdba -password masterkey mydb.fdb

# Execute SQL file
sb_isql -user sysdba -password masterkey -input script.sql mydb.fdb

# Extract database DDL
sb_isql -user sysdba -password masterkey -x mydb.fdb

# Connect with schema context
sb_isql -user sysdba -password masterkey -schema finance.accounting mydb.fdb
```

#### **Command-Line Options**

**Connection Options**
```bash
-user <username>        Database username
-password <password>    Database password  
-role <role>           SQL role name
-trusted               Use trusted authentication (Windows/SSPI)
-fetch_password        Fetch password from file or environment

# Connection examples
sb_isql -user manager -password secret sales.fdb
sb_isql -trusted -role finance_user accounting.fdb
sb_isql -user admin -fetch_password production.fdb
```

**Input/Output Options**
```bash
-input <file>          Read commands from SQL file
-output <file>         Write output to file
-merge <file>          Merge stderr and stdout to file
-echo                  Echo commands to output
-bail                  Stop execution on first error
-quiet                 Minimal output mode

# I/O examples
sb_isql -input schema.sql -output results.txt -echo mydb.fdb
sb_isql -merge session.log -bail -user admin production.fdb
```

**Schema Options (ScratchBird Enhancement)**
```bash
-schema <name>         Set current schema context
-home_schema <name>    Set home schema for resolution

# Schema examples
sb_isql -schema finance.accounting -user accountant mydb.fdb
sb_isql -schema company.americas.sales -home_schema company sales.fdb
sb_isql -schema enterprise.hr.payroll hr_system.fdb
```

**Display Options**
```bash
-stats                 Show performance statistics
-plan                  Show execution plans
-noheaders            Don't show column headers
-list                 List format output
-pagesize <size>      Set page size for output
-term <char>          Set statement terminator (default ;)

# Display examples
sb_isql -stats -plan -pagesize 50 analytics.fdb
sb_isql -list -noheaders -term "|" reporting.fdb
```

**DDL Extraction Options**
```bash
-x                    Extract DDL for database objects
-a                    Extract DDL for all objects (including system)

# DDL extraction examples
sb_isql -x -output schema.sql mydb.fdb
sb_isql -a -output complete_schema.sql mydb.fdb
```

#### **Interactive Commands**

**Schema Navigation (ScratchBird Enhancement)**
```sql
-- Set current schema
SET SCHEMA 'finance.accounting';
SET SCHEMA 'company.americas.sales';

-- Show current schema context
SHOW SCHEMA;
SHOW HOME SCHEMA;

-- List available schemas
SHOW SCHEMAS;
SHOW SCHEMAS LIKE 'finance.%';
```

**Database Object Commands**
```sql
-- Show database objects
SHOW TABLES;
SHOW VIEWS;
SHOW PROCEDURES;
SHOW FUNCTIONS;
SHOW TRIGGERS;
SHOW INDEXES;

-- Show with schema qualification
SHOW TABLES IN SCHEMA finance.accounting;
SHOW PROCEDURES IN SCHEMA enterprise.hr;

-- Show system information
SHOW DATABASE;
SHOW USERS;
SHOW ROLES;
```

**Performance and Diagnostics**
```sql
-- Performance statistics
SET STATISTICS ON;
SELECT COUNT(*) FROM large_table;
SET STATISTICS OFF;

-- Execution plans
SET PLAN ON;
SELECT * FROM customers WHERE region = 'WEST';
SET PLAN OFF;

-- Auto-commit control
SET AUTOCOMMIT OFF;
INSERT INTO orders VALUES (1, 'Test');
ROLLBACK;
SET AUTOCOMMIT ON;
```

#### **Advanced Features**

**Database Link Support (ScratchBird Enhancement)**
```sql
-- Connect to remote databases through links
SELECT * FROM remote_customers@finance_link;
SELECT * FROM employees@hr_link;

-- Schema-aware database links
SELECT * FROM finance.reports.monthly_summary@reporting_link;
```

**Hierarchical Schema Operations**
```sql
-- Create nested schemas
CREATE SCHEMA company;
CREATE SCHEMA company.americas;
CREATE SCHEMA company.americas.sales;

-- Reference objects in nested schemas
SELECT * FROM company.americas.sales.customers;
INSERT INTO company.americas.sales.orders VALUES (...);
```

#### **Configuration File Support**
```bash
# Create sb_isql configuration file: ~/.sb_isql.conf
database_alias = production:/opt/scratchbird/databases/production.fdb
default_user = admin
default_schema = application.main
statistics = on
plan = on
pagesize = 100

# Use configuration
sb_isql production
```

---

### sb_gbak - Backup and Restore Tool

**Purpose**: Complete database backup and restore utility with advanced compression and filtering.

#### **Basic Operations**

**Database Backup**
```bash
# Simple backup
sb_gbak -backup mydb.fdb mydb.fbk

# Backup with options
sb_gbak -backup -user sysdba -password masterkey -verbose mydb.fdb mydb.fbk

# Metadata-only backup
sb_gbak -backup -metadata -user admin production.fdb schema_only.fbk

# Compressed backup
sb_gbak -backup -compress -user admin large_db.fdb compressed.fbk
```

**Database Restore**
```bash
# Simple restore
sb_gbak -restore mydb.fbk newdb.fdb

# Replace existing database
sb_gbak -restore -replace mydb.fbk existing.fdb

# Create database with specific page size
sb_gbak -restore -page_size 16384 mydb.fbk newdb.fdb

# Metadata-only restore
sb_gbak -restore -metadata_only schema.fbk newdb.fdb
```

#### **Command-Line Options**

**Backup Options**
```bash
-backup                Create database backup
-metadata              Backup metadata only (no data)
-transportable         Create transportable backup format
-garbage_collect       Don't collect garbage during backup
-ignore_checksums      Ignore page checksums
-limbo                 Ignore limbo transactions
-external              Convert external tables to internal
-compress              Compress backup file

# Advanced backup examples
sb_gbak -backup -metadata -transportable schema.fdb portable_schema.fbk
sb_gbak -backup -garbage_collect -ignore_checksums quick_backup.fdb quick.fbk
sb_gbak -backup -compress -external complete.fdb compressed_complete.fbk
```

**Restore Options**
```bash
-restore               Restore from backup
-create                Create new database (same as -restore)
-replace               Replace existing database
-inactive              Deactivate indexes during restore
-no_validity           Skip validity checking during restore
-one_at_a_time         Restore one table at a time
-use_all_space         Use all available space (no reserve)
-metadata_only         Restore metadata only
-data_only            Restore data only
-kill_shadows         Kill shadow files
-page_size <size>     Set page size for new database

# Advanced restore examples
sb_gbak -restore -inactive -no_validity fast_restore.fbk fast_db.fdb
sb_gbak -restore -one_at_a_time -use_all_space large.fbk large_db.fdb
sb_gbak -restore -page_size 32768 backup.fbk optimized.fdb
```

#### **Schema Filtering (ScratchBird Enhancement)**

**Schema-Level Filtering**
```bash
# Backup specific schemas only
sb_gbak -backup -include_schema finance mydb.fdb finance_only.fbk
sb_gbak -backup -include_schema "finance.accounting" mydb.fdb accounting.fbk

# Exclude specific schemas
sb_gbak -backup -skip_schema temp -skip_schema debug mydb.fdb clean.fbk

# Multiple schema patterns
sb_gbak -backup -include_schema "finance.*" -include_schema "hr.*" mydb.fdb business.fbk
```

**Table-Level Filtering**
```bash
# Backup specific tables
sb_gbak -backup -include_table customers -include_table orders mydb.fdb partial.fbk

# Skip temporary tables
sb_gbak -backup -skip_table "temp_*" -skip_table "log_*" mydb.fdb clean.fbk

# Schema-qualified table filtering
sb_gbak -backup -include_table "finance.accounting.*" mydb.fdb accounting.fbk
```

#### **Advanced Compression (ScratchBird Enhancement)**

**Compression Types**
```bash
# LZ4 compression (fast)
sb_gbak -backup -compression lz4 large_db.fdb fast_compressed.fbk

# ZSTD compression (best ratio)
sb_gbak -backup -compression zstd large_db.fdb best_compressed.fbk

# GZIP compression (compatible)
sb_gbak -backup -compression gzip large_db.fdb compatible.fbk

# BZIP2 compression (high compression)
sb_gbak -backup -compression bzip2 large_db.fdb high_compressed.fbk
```

**Compression with Levels**
```bash
# Compression level control
sb_gbak -backup -compression zstd:9 -user admin huge_db.fdb max_compressed.fbk
sb_gbak -backup -compression lz4:1 -user admin speed_db.fdb fast.fbk
```

#### **Encryption Support (ScratchBird Enhancement)**

**Backup Encryption**
```bash
# AES-256 encryption
sb_gbak -backup -encryption aes256 -password_file keys.txt sensitive.fdb encrypted.fbk

# ChaCha20 encryption
sb_gbak -backup -encryption chacha20 -password mykey sensitive.fdb encrypted.fbk

# Combined compression and encryption
sb_gbak -backup -compression zstd -encryption aes256 -password_file keys.txt data.fdb secure.fbk
```

#### **Parallel Processing (ScratchBird Enhancement)**

**Multi-threaded Operations**
```bash
# Parallel backup (4 threads)
sb_gbak -backup -parallel 4 -user admin huge_db.fdb parallel.fbk

# Parallel restore (8 threads)
sb_gbak -restore -parallel 8 huge_backup.fbk restored_db.fdb

# Combined with compression
sb_gbak -backup -parallel 4 -compression lz4 large_db.fdb fast_parallel.fbk
```

#### **Progress Monitoring**

**Progress Tracking**
```bash
# Verbose progress
sb_gbak -backup -verbose -progress mydb.fdb mydb.fbk

# Progress with statistics
sb_gbak -backup -verbose -statistics -progress large_db.fdb large.fbk

# Progress callback (for scripting)
sb_gbak -backup -progress_callback /scripts/progress.sh mydb.fdb mydb.fbk
```

#### **Validation and Integrity**

**Enhanced Validation (ScratchBird Enhancement)**
```bash
# Real-time validation during backup
sb_gbak -backup -validate -checksum mydb.fdb validated.fbk

# Integrity checking during restore
sb_gbak -restore -verify -validate backup.fbk verified.fdb

# Deep validation mode
sb_gbak -backup -validate deep -user admin critical.fdb deep_validated.fbk
```

---

### sb_gstat - Database Statistics Tool

**Purpose**: Comprehensive database analysis and statistics reporting tool.

#### **Basic Usage**
```bash
# Basic database statistics
sb_gstat mydb.fdb

# Header information only
sb_gstat -header mydb.fdb

# Complete analysis
sb_gstat -all mydb.fdb

# Specific table analysis
sb_gstat -table customers mydb.fdb
```

#### **Analysis Options**

**Data Analysis**
```bash
-all                   Analyze all database components
-data                  Analyze data pages only
-index                 Analyze index leaf pages only
-header                Analyze header page only
-system                Analyze system relations
-record                Analyze record versions
-encryption            Analyze database encryption status

# Analysis examples
sb_gstat -data -index -user admin production.fdb
sb_gstat -system -record large_db.fdb
sb_gstat -encryption secure_db.fdb
```

**Schema Analysis (ScratchBird Enhancement)**
```bash
-schema <name>         Analyze specific schema
-table <name>          Analyze specific table

# Schema-specific analysis
sb_gstat -schema finance.accounting accounting.fdb
sb_gstat -schema "company.americas.*" regional.fdb
sb_gstat -table finance.accounting.transactions accounting.fdb
```

#### **Connection Options**
```bash
-user <username>       Database username
-password <password>   Database password
-role <role>          SQL role name
-trusted              Use trusted authentication
-fetch_password       Fetch password from file

# Connection examples
sb_gstat -user admin -password secret -role dba mydb.fdb
sb_gstat -trusted production.fdb
```

#### **Output Control**
```bash
-nocreation           Suppress creation date in output
-verbose              Detailed output
-brief                Brief output (summary only)

# Output examples
sb_gstat -nocreation -brief summary.fdb
sb_gstat -verbose -all detailed_analysis.fdb
```

#### **Enhanced Reporting (ScratchBird Enhancement)**

**Performance Analysis**
```bash
# Index efficiency analysis
sb_gstat -index_efficiency mydb.fdb

# Page utilization report
sb_gstat -page_utilization mydb.fdb

# Fragmentation analysis
sb_gstat -fragmentation mydb.fdb

# Cache hit ratio analysis
sb_gstat -cache_analysis mydb.fdb
```

**Schema Hierarchy Analysis**
```bash
# Schema tree structure
sb_gstat -schema_tree mydb.fdb

# Schema storage utilization
sb_gstat -schema_storage mydb.fdb

# Cross-schema dependencies
sb_gstat -schema_dependencies mydb.fdb
```

#### **Sample Output Analysis**

**Header Analysis**
```
Database header page information:
    Flags                   0
    Checksum                12345
    Generation              152
    Page size               8192
    ODS version             13.0
    Oldest transaction      145
    Oldest active           149
    Oldest snapshot         149
    Next transaction        152
    Bumped transaction      1
    Sequence number         0
    Next attachment ID      6
    Implementation ID       24
    Shadow count            0
    Page buffers            0
    Next header page        0
    Database dialect        4   ← ScratchBird SQL Dialect 4
    Creation date           Dec 27, 2024 14:30:15
    Attributes              force write, hierarchical schemas enabled
```

**Schema Analysis**
```
Schema: FINANCE.ACCOUNTING
    Tables: 15
    Views: 8
    Procedures: 12
    Functions: 6
    Total pages: 2,856
    Data pages: 2,234
    Index pages: 622
    Average fill: 78.5%
    
    Largest tables:
        TRANSACTIONS (1,245 pages, 89.2% fill)
        JOURNAL_ENTRIES (567 pages, 82.1% fill)
        ACCOUNT_BALANCES (234 pages, 91.3% fill)
```

---

### sb_gfix - Database Maintenance Tool

**Purpose**: Database validation, repair, and maintenance operations.

#### **Basic Operations**

**Database Validation**
```bash
# Basic validation
sb_gfix -validate mydb.fdb

# Full validation with details
sb_gfix -validate -full -user sysdba mydb.fdb

# Read-only validation
sb_gfix -validate -readonly mydb.fdb
```

**Database Repair**
```bash
# Repair corrupted database
sb_gfix -mend mydb.fdb

# Force repair (ignore errors)
sb_gfix -mend -force mydb.fdb

# Repair with backup
sb_gfix -mend -backup mydb.fdb
```

#### **Validation Modes (ScratchBird Enhancement)**

**Validation Levels**
```bash
-validate              Basic validation
-validate basic        Basic structure validation
-validate normal       Normal validation (default)
-validate full         Full validation including data
-validate deep         Deep forensic validation
-validate forensic     Comprehensive forensic analysis

# Validation examples
sb_gfix -validate basic quick_check.fdb
sb_gfix -validate full -user admin production.fdb
sb_gfix -validate forensic -backup corrupted.fdb
```

**Schema-Specific Validation**
```bash
# Validate specific schema
sb_gfix -validate -schema finance.accounting mydb.fdb

# Validate multiple schemas
sb_gfix -validate -schema "finance.*" -schema "hr.*" mydb.fdb

# Validate with schema dependency checking
sb_gfix -validate -schema_dependencies mydb.fdb
```

#### **Repair Strategies (ScratchBird Enhancement)**

**Intelligent Repair**
```bash
-mend                  Basic repair
-mend conservative     Conservative repair (minimal changes)
-mend normal          Normal repair (default)
-mend aggressive      Aggressive repair (maximum recovery)
-mend forensic        Forensic repair (preserve evidence)

# Repair examples
sb_gfix -mend conservative -backup sensitive.fdb
sb_gfix -mend aggressive -force corrupted.fdb
sb_gfix -mend forensic -preserve_evidence incident.fdb
```

**Automatic Backup During Repair**
```bash
# Automatic backup before repair
sb_gfix -mend -backup -user admin damaged.fdb

# Backup to specific location
sb_gfix -mend -backup_to /backup/emergency.fbk damaged.fdb

# Compressed backup during repair
sb_gfix -mend -backup -compress damaged.fdb
```

#### **Sweep Operations**

**Database Sweep**
```bash
# Manual sweep
sb_gfix -sweep mydb.fdb

# Cooperative sweep (non-blocking)
sb_gfix -sweep -cooperative mydb.fdb

# Background sweep
sb_gfix -sweep -background mydb.fdb
```

**Sweep Configuration**
```bash
# Set sweep interval
sb_gfix -set_sweep_interval 20000 mydb.fdb

# Disable automatic sweep
sb_gfix -set_sweep_interval 0 mydb.fdb

# Show sweep status
sb_gfix -sweep_status mydb.fdb
```

#### **Index Maintenance**

**Index Rebuilding**
```bash
# Rebuild all indexes
sb_gfix -rebuild_indexes mydb.fdb

# Rebuild specific index
sb_gfix -rebuild_index idx_customer_name mydb.fdb

# Rebuild schema indexes
sb_gfix -rebuild_schema_indexes finance.accounting mydb.fdb
```

**Index Validation**
```bash
# Validate all indexes
sb_gfix -validate_indexes mydb.fdb

# Validate with detailed report
sb_gfix -validate_indexes -detailed mydb.fdb
```

#### **Transaction Management**

**Limbo Transaction Resolution**
```bash
# Show limbo transactions
sb_gfix -limbo mydb.fdb

# Commit limbo transactions
sb_gfix -commit_limbo mydb.fdb

# Rollback limbo transactions
sb_gfix -rollback_limbo mydb.fdb
```

**Active Transaction Management**
```bash
# Show active transactions
sb_gfix -active_transactions mydb.fdb

# Force transaction resolution
sb_gfix -force_shutdown immediate mydb.fdb
```

#### **Database State Management**

**Shutdown Operations**
```bash
# Graceful shutdown
sb_gfix -shutdown normal 60 mydb.fdb

# Forced shutdown
sb_gfix -shutdown forced mydb.fdb

# Immediate shutdown
sb_gfix -shutdown immediate mydb.fdb
```

**Online Operations**
```bash
# Bring database online
sb_gfix -online mydb.fdb

# Check database state
sb_gfix -database_state mydb.fdb
```

#### **Health Assessment (ScratchBird Enhancement)**

**Comprehensive Health Check**
```bash
# Complete health assessment
sb_gfix -health_check mydb.fdb

# Health check with recommendations
sb_gfix -health_check -recommendations mydb.fdb

# Performance health analysis
sb_gfix -health_check -performance mydb.fdb
```

---

### sb_gsec - Security Management Tool

**Purpose**: User account and security management for ScratchBird databases.

#### **User Management**

**Add Users**
```bash
# Basic user creation
sb_gsec -add testuser -pw password123

# User with full information
sb_gsec -add manager -pw secret -fname John -lname Smith -admin yes

# User with group assignment
sb_gsec -add clerk -pw pass123 -group finance_users
```

**Modify Users**
```bash
# Change password
sb_gsec -modify testuser -pw newpassword

# Update user information
sb_gsec -modify manager -fname Jane -mname Marie -lname Doe

# Grant admin privileges
sb_gsec -modify clerk -admin yes
```

**Delete Users**
```bash
# Delete user account
sb_gsec -delete testuser

# Delete with confirmation
sb_gsec -delete -confirm olduser
```

**Display Users**
```bash
# List all users
sb_gsec -display

# Display specific user
sb_gsec -display manager

# Display with detailed information
sb_gsec -display -detailed
```

#### **User Parameters**

**Basic Parameters**
```bash
-pw <password>         User password
-fname <name>          First name
-mname <name>          Middle name  
-lname <name>          Last name
-admin <yes|no>        Administrative privileges

# Parameter examples
sb_gsec -add analyst -pw secret123 -fname Mary -lname Johnson -admin no
sb_gsec -modify analyst -mname Elizabeth -admin yes
```

**Advanced Parameters (ScratchBird Enhancement)**
```bash
-uid <number>          User ID number
-gid <number>          Group ID number
-group <name>          Primary group name
-roles <list>          Assigned roles (comma-separated)
-home_schema <name>    Default home schema
-max_connections <n>   Maximum concurrent connections

# Advanced examples
sb_gsec -add developer -pw dev123 -group dev_team -roles developer,tester -home_schema development
sb_gsec -modify analyst -max_connections 5 -home_schema finance.analysis
```

#### **Connection Options**
```bash
-database <path>       Security database path
-user <username>       Administrator username
-password <password>   Administrator password
-role <role>          Administrative role
-trusted              Use trusted authentication

# Connection examples
sb_gsec -database /opt/scratchbird/security4.fdb -user sysdba -password masterkey -display
sb_gsec -trusted -role user_admin -add newuser -pw password
```

#### **Security Auditing (ScratchBird Enhancement)**

**Security Analysis**
```bash
# Comprehensive security audit
sb_gsec -security_audit

# Password policy analysis
sb_gsec -password_audit

# Access pattern analysis
sb_gsec -access_audit

# Failed login analysis
sb_gsec -failed_login_audit
```

**Role Management**
```bash
# List user roles
sb_gsec -list_roles username

# Assign roles to user
sb_gsec -assign_role developer,tester username

# Remove roles from user
sb_gsec -remove_role tester username

# Show role hierarchy
sb_gsec -role_hierarchy
```

#### **Advanced Security Features (ScratchBird Enhancement)**

**Password Policy Management**
```bash
# Set password policy
sb_gsec -set_password_policy -min_length 8 -require_numbers yes -require_symbols yes

# Show password policy
sb_gsec -show_password_policy

# Validate passwords against policy
sb_gsec -validate_passwords
```

**Multi-Factor Authentication**
```bash
# Enable MFA for user
sb_gsec -enable_mfa username

# Disable MFA for user  
sb_gsec -disable_mfa username

# Show MFA status
sb_gsec -mfa_status username
```

**Compliance Reporting**
```bash
# GDPR compliance check
sb_gsec -compliance_check gdpr

# HIPAA compliance check
sb_gsec -compliance_check hipaa

# SOX compliance check
sb_gsec -compliance_check sox

# Generate compliance report
sb_gsec -compliance_report all -output compliance.xml
```

---

### sb_nbackup - Incremental Backup Tool

**Purpose**: Incremental and differential backup system for large databases.

#### **Basic Operations**

**Full Backup (Level 0)**
```bash
# Create full backup
sb_nbackup -B 0 mydb.fdb full_backup.nb0

# Full backup with compression
sb_nbackup -B 0 -compression zip mydb.fdb compressed_full.nb0
```

**Incremental Backups**
```bash
# Level 1 incremental (depends on level 0)
sb_nbackup -B 1 mydb.fdb incremental_1.nb1

# Level 2 incremental (depends on level 1)
sb_nbackup -B 2 mydb.fdb incremental_2.nb2

# Multiple incremental levels
sb_nbackup -B 1 mydb.fdb monday.nb1
sb_nbackup -B 2 mydb.fdb tuesday.nb2
sb_nbackup -B 2 mydb.fdb wednesday.nb2
```

**Database Restore**
```bash
# Restore from full backup only
sb_nbackup -R restored.fdb full_backup.nb0

# Restore from full + incrementals
sb_nbackup -R restored.fdb full_backup.nb0 incremental_1.nb1 incremental_2.nb2

# Restore with specific page size
sb_nbackup -R -page_size 16384 restored.fdb backups/full.nb0 backups/inc1.nb1
```

#### **Database Locking**

**External Backup Support**
```bash
# Lock database for external backup
sb_nbackup -L mydb.fdb

# Perform external backup (filesystem level)
cp mydb.fdb /backup/external_backup.fdb

# Unlock database
sb_nbackup -N mydb.fdb
```

**Backup State Management**
```bash
# Show backup state
sb_nbackup -S mydb.fdb

# Check if database is locked
sb_nbackup -state mydb.fdb
```

#### **Advanced Options**

**Connection Parameters**
```bash
-user <username>       Database username (default: SYSDBA)
-password <password>   Database password
-trusted              Use trusted authentication
-fetch_password       Fetch password from environment

# Connection examples
sb_nbackup -B 0 -user backup_user -password secret mydb.fdb backup.nb0
sb_nbackup -R -trusted restored.fdb backup.nb0
```

**Performance Options**
```bash
-verbose              Verbose output with progress
-direct on|off        Direct I/O mode (bypass OS cache)
-compress <type>      Compression algorithm

# Performance examples
sb_nbackup -B 0 -verbose -direct on large_db.fdb fast_backup.nb0
sb_nbackup -B 1 -compress gzip -verbose mydb.fdb compressed_inc.nb1
```

#### **Hierarchical Schema Support (ScratchBird Enhancement)**

**Schema-Aware Backups**
```bash
# Preserve schema hierarchy information
sb_nbackup -B 0 -preserve_schema mydb.fdb schema_aware.nb0

# Schema-specific incremental backup
sb_nbackup -B 1 -schema finance.accounting mydb.fdb accounting_inc.nb1

# Include schema metadata in backup
sb_nbackup -B 0 -include_schema_metadata mydb.fdb metadata_backup.nb0
```

#### **Compression Options (ScratchBird Enhancement)**

**Compression Types**
```bash
# No compression
sb_nbackup -B 0 -compression none mydb.fdb uncompressed.nb0

# ZIP compression (compatible)
sb_nbackup -B 0 -compression zip mydb.fdb zip_compressed.nb0

# GZIP compression (better ratio)
sb_nbackup -B 0 -compression gzip mydb.fdb gzip_compressed.nb0

# LZ4 compression (fastest)
sb_nbackup -B 0 -compression lz4 mydb.fdb lz4_compressed.nb0
```

#### **Backup Strategy Examples**

**Weekly Full + Daily Incremental**
```bash
#!/bin/bash
# Weekly backup strategy script

DAY=$(date +%u)  # 1=Monday, 7=Sunday
DB_FILE="/opt/scratchbird/databases/production.fdb"
BACKUP_DIR="/backup/nbackup"

if [ $DAY -eq 1 ]; then
    # Monday: Full backup
    sb_nbackup -B 0 -verbose -compression gzip $DB_FILE $BACKUP_DIR/full_$(date +%Y%m%d).nb0
else
    # Tuesday-Sunday: Incremental backup
    sb_nbackup -B 1 -verbose -compression gzip $DB_FILE $BACKUP_DIR/inc_$(date +%Y%m%d).nb1
fi
```

**Monthly Archive Strategy**
```bash
#!/bin/bash
# Monthly archive strategy

MONTH=$(date +%Y%m)
DB_FILE="/opt/scratchbird/databases/archive.fdb"
ARCHIVE_DIR="/archive/monthly"

# Level 0: Monthly full
sb_nbackup -B 0 -compression gzip $DB_FILE $ARCHIVE_DIR/monthly_$MONTH.nb0

# Level 1: Weekly incrementals
WEEK=$(date +%V)
sb_nbackup -B 1 -compression gzip $DB_FILE $ARCHIVE_DIR/weekly_${MONTH}_$WEEK.nb1

# Level 2: Daily incrementals
DAY=$(date +%d)
sb_nbackup -B 2 -compression gzip $DB_FILE $ARCHIVE_DIR/daily_${MONTH}_$DAY.nb2
```

---

### Advanced Utilities

### sb_tracemgr - Trace Management Tool

**Purpose**: Management of database activity tracing and monitoring sessions.

#### **Basic Operations**

**Start Trace Session**
```bash
# Start basic trace session
sb_tracemgr -start -config trace.conf localhost:service_mgr

# Start named trace session
sb_tracemgr -start -name production_trace -config production_trace.conf localhost:service_mgr

# Start with output file
sb_tracemgr -start -config trace.conf -output trace_output.log localhost:service_mgr
```

**Manage Trace Sessions**
```bash
# List active trace sessions
sb_tracemgr -list localhost:service_mgr

# Stop trace session
sb_tracemgr -stop -sessionid 3 localhost:service_mgr

# Suspend trace session
sb_tracemgr -suspend -sessionid 3 localhost:service_mgr

# Resume trace session
sb_tracemgr -resume -sessionid 3 localhost:service_mgr
```

#### **Trace Configuration Integration**

**Using sbtrace.conf**
```bash
# Start trace with comprehensive configuration
sb_tracemgr -start -config /opt/scratchbird/conf/sbtrace.conf localhost:service_mgr

# Start schema-specific trace
sb_tracemgr -start -config schema_trace.conf -name finance_trace localhost:service_mgr
```

#### **Monitoring and Analysis**

**Real-time Monitoring**
```bash
# Monitor trace output in real-time
sb_tracemgr -start -config realtime.conf localhost:service_mgr | tail -f

# Monitor with filtering
sb_tracemgr -start -config filtered.conf localhost:service_mgr | grep "ERROR\|WARNING"
```

---

### sb_svcmgr - Service Manager Tool

**Purpose**: Remote service management and database administration.

#### **Database Operations**

**Backup and Restore via Services**
```bash
# Remote backup
sb_svcmgr -action_backup -dbname production.fdb -backup_file backup.fbk localhost:service_mgr

# Remote restore  
sb_svcmgr -action_restore -backup_file backup.fbk -dbname restored.fdb localhost:service_mgr

# Backup with service options
sb_svcmgr -action_backup -dbname mydb.fdb -backup_file mydb.fbk -verbose localhost:service_mgr
```

**Database Maintenance**
```bash
# Database validation via service
sb_svcmgr -action_validate -dbname mydb.fdb localhost:service_mgr

# Database repair via service
sb_svcmgr -action_repair -dbname corrupted.fdb localhost:service_mgr

# Database statistics via service
sb_svcmgr -action_db_stats -dbname mydb.fdb localhost:service_mgr
```

#### **User Management via Services**

**User Operations**
```bash
# List users via service
sb_svcmgr -action_get_users localhost:service_mgr

# Add user via service
sb_svcmgr -action_add_user -user_name newuser -user_password secret localhost:service_mgr

# Modify user via service
sb_svcmgr -action_modify_user -user_name existing -user_password newpass localhost:service_mgr

# Delete user via service
sb_svcmgr -action_delete_user -user_name olduser localhost:service_mgr
```

#### **Trace Management via Services**

**Remote Trace Control**
```bash
# Start trace via service
sb_svcmgr -action_trace_start -trace_config trace.conf localhost:service_mgr

# Stop trace via service
sb_svcmgr -action_trace_stop -trace_sessionid 3 localhost:service_mgr

# List traces via service
sb_svcmgr -action_trace_list localhost:service_mgr
```

---

### Specialized Utilities

### sb_guard - Process Monitor

**Purpose**: Monitor and automatically restart ScratchBird server processes.

#### **Basic Usage**
```bash
# Start server with monitoring
sb_guard scratchbird

# Run forever (restart on exit)
sb_guard -forever scratchbird

# Run once (no restart)
sb_guard -once scratchbird

# Ignore signals (let server handle)
sb_guard -signore scratchbird
```

#### **Service Integration**
```bash
# Systemd service integration
[Unit]
Description=ScratchBird Database Server
After=network.target

[Service]
Type=simple
User=scratchbird
ExecStart=/opt/scratchbird/bin/sb_guard -forever /opt/scratchbird/bin/scratchbird
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

---

### sb_lock_print - Lock Analysis Tool

**Purpose**: Analyze database locking and transaction information.

#### **Basic Lock Analysis**
```bash
# Show all lock information
sb_lock_print -all mydb.fdb

# Show active transactions
sb_lock_print -transactions mydb.fdb

# Show connection information
sb_lock_print -connections mydb.fdb

# Verbose lock details
sb_lock_print -verbose mydb.fdb
```

#### **Schema Lock Analysis (ScratchBird Enhancement)**
```bash
# Show schema access patterns
sb_lock_print -schemas mydb.fdb

# Show schema-specific locks
sb_lock_print -schema finance.accounting mydb.fdb

# Database link lock monitoring
sb_lock_print -database_links mydb.fdb
```

#### **Transaction Analysis**
```bash
# Show transaction isolation levels
sb_lock_print -isolation mydb.fdb

# Show deadlock information
sb_lock_print -deadlocks mydb.fdb

# Show lock wait statistics
sb_lock_print -wait_stats mydb.fdb
```

---

### sb_gssplit - File Splitting Tool

**Purpose**: Split large database files for distribution or storage management.

#### **File Splitting**
```bash
# Split large database into 100MB files
sb_gssplit -split large_db.fdb 100M part1.fdb part2.fdb part3.fdb

# Split with automatic naming
sb_gssplit -split -auto large_db.fdb 50M

# Split backup file
sb_gssplit -split backup.fbk 100M backup_part1.fbk backup_part2.fbk
```

#### **File Joining**
```bash
# Join split files back together
sb_gssplit -join restored_db.fdb part1.fdb part2.fdb part3.fdb

# Join with verification
sb_gssplit -join -verify complete_db.fdb part*.fdb
```

#### **Schema Preservation (ScratchBird Enhancement)**
```bash
# Preserve schema boundaries during split
sb_gssplit -split -preserve_schema large_db.fdb 100M

# Preserve database link references
sb_gssplit -split -preserve_links database_with_links.fdb 50M

# Split with compression
sb_gssplit -split -compression gzip large_db.fdb 100M
```

---

## Utility Integration and Scripting

### Automation Scripts

#### **Complete Database Maintenance Script**
```bash
#!/bin/bash
# ScratchBird Database Maintenance Script

DB_FILE="/opt/scratchbird/databases/production.fdb"
BACKUP_DIR="/backup/daily"
LOG_DIR="/var/log/scratchbird"
DATE=$(date +%Y%m%d_%H%M%S)

# Function to log messages
log_message() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') $1" >> $LOG_DIR/maintenance.log
}

# 1. Validate database
log_message "Starting database validation"
if sb_gfix -validate -user admin -password $DB_PASS $DB_FILE; then
    log_message "Database validation successful"
else
    log_message "Database validation failed - aborting"
    exit 1
fi

# 2. Create backup
log_message "Starting database backup"
if sb_gbak -backup -user admin -password $DB_PASS -compress $DB_FILE $BACKUP_DIR/backup_$DATE.fbk; then
    log_message "Database backup successful"
else
    log_message "Database backup failed"
    exit 1
fi

# 3. Collect statistics
log_message "Collecting database statistics"
sb_gstat -user admin -password $DB_PASS -all $DB_FILE > $LOG_DIR/stats_$DATE.txt

# 4. Sweep database
log_message "Starting database sweep"
sb_gfix -sweep -user admin -password $DB_PASS $DB_FILE

# 5. Security audit
log_message "Running security audit"
sb_gsec -security_audit -user admin -password $DB_PASS > $LOG_DIR/security_audit_$DATE.txt

log_message "Maintenance completed successfully"
```

#### **Schema-Aware Backup Script**
```bash
#!/bin/bash
# Schema-specific backup script for ScratchBird

SCHEMAS=("finance.accounting" "finance.budgeting" "hr.payroll" "sales.orders")
DB_FILE="/opt/scratchbird/databases/enterprise.fdb"
BACKUP_BASE="/backup/schemas"
DATE=$(date +%Y%m%d)

for schema in "${SCHEMAS[@]}"; do
    echo "Backing up schema: $schema"
    
    # Create schema-specific backup
    sb_gbak -backup -include_schema "$schema" -compression lz4 \
           -user admin -password $DB_PASS \
           $DB_FILE "$BACKUP_BASE/${schema//\./_}_$DATE.fbk"
    
    if [ $? -eq 0 ]; then
        echo "Schema $schema backup completed successfully"
    else
        echo "Schema $schema backup failed"
    fi
done
```

### Monitoring Integration

#### **Database Health Check Script**
```bash
#!/bin/bash
# Comprehensive database health monitoring

DB_FILE="/opt/scratchbird/databases/production.fdb"
ALERT_EMAIL="admin@company.com"
THRESHOLD_CONNECTIONS=80
THRESHOLD_CACHE_HIT=95

# Check database connectivity
if ! sb_isql -user monitor -password $MONITOR_PASS -quiet $DB_FILE -input /dev/null; then
    echo "CRITICAL: Database connection failed" | mail -s "DB Alert" $ALERT_EMAIL
    exit 1
fi

# Check connection count
CONNECTIONS=$(sb_lock_print -connections $DB_FILE | grep -c "Connection")
if [ $CONNECTIONS -gt $THRESHOLD_CONNECTIONS ]; then
    echo "WARNING: High connection count: $CONNECTIONS" | mail -s "DB Alert" $ALERT_EMAIL
fi

# Check cache hit ratio
CACHE_HIT=$(sb_gstat -header $DB_FILE | grep "Cache hit ratio" | awk '{print $4}' | sed 's/%//')
if [ $CACHE_HIT -lt $THRESHOLD_CACHE_HIT ]; then
    echo "WARNING: Low cache hit ratio: $CACHE_HIT%" | mail -s "DB Alert" $ALERT_EMAIL
fi

# Check for corrupted pages
if sb_gfix -validate -readonly $DB_FILE 2>&1 | grep -q "corruption\|error"; then
    echo "CRITICAL: Database corruption detected" | mail -s "DB Alert" $ALERT_EMAIL
fi

echo "Database health check completed at $(date)"
```

---

## Best Practices and Recommendations

### Performance Optimization

#### **Utility Performance Tips**
```bash
# Use parallel processing for large operations
sb_gbak -backup -parallel 4 -compression lz4 large_db.fdb backup.fbk
sb_gfix -validate -parallel 2 huge_db.fdb

# Use appropriate compression for your use case
sb_gbak -backup -compression lz4 speed_critical.fdb fast.fbk      # Speed
sb_gbak -backup -compression zstd storage_critical.fdb small.fbk  # Size

# Direct I/O for large databases
sb_nbackup -B 0 -direct on large_db.fdb backup.nb0

# Background operations for production
sb_gfix -sweep -background production.fdb
```

### Security Best Practices

#### **Secure Utility Usage**
```bash
# Use environment variables for passwords
export SCRATCHBIRD_PASSWORD=secret123
sb_gbak -backup -user admin -fetch_password mydb.fdb backup.fbk

# Use trusted authentication when possible
sb_gstat -trusted production.fdb

# Encrypt sensitive backups
sb_gbak -backup -encryption aes256 -password_file keys.txt sensitive.fdb encrypted.fbk

# Regular security audits
sb_gsec -security_audit -output security_report.xml
```

### Automation Best Practices

#### **Script Error Handling**
```bash
#!/bin/bash
# Robust utility scripting example

set -euo pipefail  # Exit on error, undefined vars, pipe failures

DB_FILE="$1"
BACKUP_FILE="$2"

# Validate inputs
if [ ! -f "$DB_FILE" ]; then
    echo "Error: Database file not found: $DB_FILE" >&2
    exit 1
fi

# Check available space
REQUIRED_SPACE=$(stat -c%s "$DB_FILE")
AVAILABLE_SPACE=$(df "$(dirname "$BACKUP_FILE")" | awk 'NR==2 {print $4*1024}')

if [ $REQUIRED_SPACE -gt $AVAILABLE_SPACE ]; then
    echo "Error: Insufficient disk space for backup" >&2
    exit 1
fi

# Perform backup with error checking
if sb_gbak -backup -user admin -fetch_password "$DB_FILE" "$BACKUP_FILE"; then
    echo "Backup completed successfully: $BACKUP_FILE"
else
    echo "Backup failed for database: $DB_FILE" >&2
    exit 1
fi
```

