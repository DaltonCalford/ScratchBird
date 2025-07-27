# ScratchBird DATABASE - Complete DDL Documentation

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

DATABASE objects in ScratchBird represent the top-level container for all database objects and data. Database creation and management operations control fundamental database properties including character sets, collations, page sizes, security settings, and operational parameters. Understanding database DDL is essential for database administration and deployment.

### Key Features and Capabilities

- **Database Creation**: Create new databases with custom properties and settings
- **Character Set Control**: Set default character sets and collations for the database
- **Page Size Configuration**: Configure database page size for optimal performance
- **Security Management**: Control default SQL security modes and authentication
- **Backup Operations**: Manage database backup and restore states
- **Encryption Support**: Enable/disable database encryption with plugins
- **Linger Control**: Configure connection lingering for performance optimization
- **Publication Support**: Enable logical replication capabilities
- **Difference Files**: Manage incremental backup difference files

### ScratchBird-Specific Enhancements

1. **Hierarchical Schema Integration**: Databases support hierarchical schema organization
2. **Enhanced Security Model**: Advanced SQL security controls and authentication options
3. **Flexible Encryption**: Plugin-based encryption system with multiple algorithms
4. **Publication/Subscription**: Built-in logical replication support
5. **Extended Character Sets**: Support for all ScratchBird character encodings
6. **Advanced Backup**: Incremental backup with difference file management
7. **Performance Optimization**: Configurable linger times and caching options
8. **Schema-Aware Operations**: Full integration with hierarchical schema system

---

## DDL Syntax Reference

### CREATE DATABASE

Creates a new database with specified properties and configuration options.

#### **Basic Syntax**
```sql
CREATE DATABASE 'database_path'
    [PAGE_SIZE page_size]
    [USER 'username']
    [PASSWORD 'password']
    [OWNER 'owner_name']
    [ROLE 'role_name']
    [DEFAULT CHARACTER SET charset_name [COLLATION collation_name]]
    [DIFFERENCE FILE 'diff_file_path'];
```

#### **Complete Syntax with All Options**
```sql
CREATE DATABASE 'database_file_path'
    [PAGE_SIZE {4096 | 8192 | 16384 | 32768}]
    [USER 'username' | USER username]
    [PASSWORD 'password']
    [OWNER 'owner_name' | OWNER owner_name]
    [ROLE 'role_name']
    [DEFAULT CHARACTER SET charset_name [COLLATION collation_name]]
    [DIFFERENCE FILE 'difference_file_path']
    [LENGTH pages_count [PAGE | PAGES]];
```

#### **Parameters**

- **database_path**: Full file path for the database file
- **PAGE_SIZE**: Database page size (4096, 8192, 16384, or 32768 bytes)
- **USER**: Username for database connection
- **PASSWORD**: Password for database authentication
- **OWNER**: Database owner username
- **ROLE**: Default role for the database
- **DEFAULT CHARACTER SET**: Default character encoding
- **COLLATION**: Default text collation
- **DIFFERENCE FILE**: Path for incremental backup difference file
- **LENGTH**: Initial database size in pages

---

## CREATE DATABASE Examples

### **Basic Database Creation**

#### **Simple Database Creation**
```sql
-- Create basic database
CREATE DATABASE '/data/databases/myapp.fdb';

-- Create database with page size
CREATE DATABASE '/data/databases/myapp.fdb'
    PAGE_SIZE 8192;

-- Create database with user authentication
CREATE DATABASE '/data/databases/myapp.fdb'
    USER 'db_admin'
    PASSWORD 'secure_password';
```

#### **Database with Character Set**
```sql
-- UTF8 database with default collation
CREATE DATABASE '/data/databases/international.fdb'
    PAGE_SIZE 16384
    DEFAULT CHARACTER SET UTF8;

-- UTF8 database with specific collation
CREATE DATABASE '/data/databases/multilang.fdb'
    PAGE_SIZE 16384
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI;

-- ASCII database for simple applications
CREATE DATABASE '/data/databases/simple.fdb'
    PAGE_SIZE 4096
    DEFAULT CHARACTER SET ASCII;
```

### **Advanced Database Creation**

#### **Enterprise Database Configuration**
```sql
-- Enterprise database with full configuration
CREATE DATABASE '/enterprise/databases/erp_system.fdb'
    PAGE_SIZE 32768
    USER 'erp_admin'
    PASSWORD 'enterprise_password_2024'
    OWNER 'system_dba'
    ROLE 'DB_ADMINISTRATOR'
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI
    DIFFERENCE FILE '/enterprise/backups/erp_diff.fbk'
    LENGTH 100000 PAGES;
```

#### **Development Environment Databases**
```sql
-- Development database
CREATE DATABASE '/dev/databases/app_dev.fdb'
    PAGE_SIZE 8192
    USER 'developer'
    PASSWORD 'dev_pass_123'
    DEFAULT CHARACTER SET UTF8;

-- Testing database
CREATE DATABASE '/test/databases/app_test.fdb'
    PAGE_SIZE 8192
    USER 'tester'
    PASSWORD 'test_secure_pass'
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI;

-- Staging database
CREATE DATABASE '/staging/databases/app_staging.fdb'
    PAGE_SIZE 16384
    USER 'staging_admin'
    PASSWORD 'staging_password_2024'
    OWNER 'staging_dba'
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI;
```

### **Specialized Database Configurations**

#### **High-Performance OLTP Database**
```sql
-- OLTP database optimized for transactions
CREATE DATABASE '/oltp/databases/transaction_system.fdb'
    PAGE_SIZE 4096  -- Smaller pages for OLTP workloads
    USER 'oltp_admin'
    PASSWORD 'oltp_secure_2024'
    OWNER 'transaction_dba'
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE
    LENGTH 50000 PAGES;
```

#### **Data Warehouse Database**
```sql
-- Data warehouse optimized for analytics
CREATE DATABASE '/warehouse/databases/analytics_dw.fdb'
    PAGE_SIZE 32768  -- Large pages for analytical workloads
    USER 'analytics_admin'
    PASSWORD 'warehouse_secure_2024'
    OWNER 'analytics_dba'
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI
    DIFFERENCE FILE '/warehouse/backups/analytics_diff.fbk'
    LENGTH 500000 PAGES;
```

#### **International Application Database**
```sql
-- Multi-language international database
CREATE DATABASE '/international/databases/global_app.fdb'
    PAGE_SIZE 16384
    USER 'global_admin'
    PASSWORD 'international_secure_2024'
    OWNER 'global_dba'
    ROLE 'INTERNATIONAL_ADMIN'
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI
    DIFFERENCE FILE '/international/backups/global_diff.fbk';
```

### **Network and Remote Databases**

#### **Remote Database Creation**
```sql
-- Create database on remote server
CREATE DATABASE 'server1:/data/databases/remote_app.fdb'
    PAGE_SIZE 8192
    USER 'remote_admin'
    PASSWORD 'remote_secure_password'
    DEFAULT CHARACTER SET UTF8;

-- Create database with network path
CREATE DATABASE '//fileserver/databases/shared_app.fdb'
    PAGE_SIZE 16384
    USER 'shared_admin'
    PASSWORD 'shared_password_2024'
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI;
```

---

## ALTER DATABASE

Modifies existing database properties and operational settings.

### **ALTER DATABASE Syntax**
```sql
ALTER DATABASE
    { ADD DIFFERENCE FILE 'diff_file_path' |
      DROP DIFFERENCE FILE |
      BEGIN BACKUP |
      END BACKUP |
      SET DEFAULT CHARACTER SET charset_name |
      ENCRYPT WITH plugin_name KEY 'encryption_key' |
      DECRYPT |
      SET LINGER TO seconds |
      DROP LINGER |
      SET DEFAULT {INVOKER | DEFINER} |
      ENABLE PUBLICATION |
      DISABLE PUBLICATION |
      INCLUDE TABLE table_pattern TO PUBLICATION |
      EXCLUDE TABLE table_pattern FROM PUBLICATION };
```

### **ALTER DATABASE Examples**

#### **Character Set and Collation Management**
```sql
-- Change default character set
ALTER DATABASE SET DEFAULT CHARACTER SET UTF8;

-- Set default character set with collation
ALTER DATABASE SET DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI;

-- Set default character set for new schemas (if supported)
ALTER DATABASE SET DEFAULT CHARACTER SET UTF8;
```

#### **Backup and Recovery Operations**
```sql
-- Add difference file for incremental backups
ALTER DATABASE ADD DIFFERENCE FILE '/backups/myapp_diff.fbk';

-- Remove difference file
ALTER DATABASE DROP DIFFERENCE FILE;

-- Begin backup state (prevents modifications during backup)
ALTER DATABASE BEGIN BACKUP;

-- End backup state (allows normal operations)
ALTER DATABASE END BACKUP;
```

#### **Encryption Management**
```sql
-- Enable database encryption
ALTER DATABASE ENCRYPT WITH fbSampleDbCrypt KEY 'SecureEncryptionKey2024';

-- Enable encryption with custom plugin
ALTER DATABASE ENCRYPT WITH MyCustomCrypt KEY 'MySecureKey123!@#';

-- Disable database encryption
ALTER DATABASE DECRYPT;
```

#### **Performance and Connection Management**
```sql
-- Set connection linger time (30 seconds)
ALTER DATABASE SET LINGER TO 30;

-- Increase linger time for better connection pooling
ALTER DATABASE SET LINGER TO 120;

-- Disable connection lingering
ALTER DATABASE DROP LINGER;
```

#### **Security Configuration**
```sql
-- Set default SQL security to DEFINER
ALTER DATABASE SET DEFAULT DEFINER;

-- Set default SQL security to INVOKER
ALTER DATABASE SET DEFAULT INVOKER;
```

#### **Publication and Replication Management**
```sql
-- Enable logical replication
ALTER DATABASE ENABLE PUBLICATION;

-- Disable logical replication
ALTER DATABASE DISABLE PUBLICATION;

-- Include specific tables in publication
ALTER DATABASE INCLUDE TABLE customers TO PUBLICATION;
ALTER DATABASE INCLUDE TABLE orders TO PUBLICATION;

-- Include table pattern in publication
ALTER DATABASE INCLUDE TABLE sales_* TO PUBLICATION;

-- Exclude tables from publication
ALTER DATABASE EXCLUDE TABLE temp_* FROM PUBLICATION;
ALTER DATABASE EXCLUDE TABLE audit_log FROM PUBLICATION;
```

### **Complex ALTER DATABASE Operations**

#### **Complete Database Reconfiguration**
```sql
-- Comprehensive database configuration update
ALTER DATABASE SET DEFAULT CHARACTER SET UTF8;
ALTER DATABASE SET LINGER TO 60;
ALTER DATABASE SET DEFAULT DEFINER;
ALTER DATABASE ADD DIFFERENCE FILE '/backups/production_diff.fbk';
ALTER DATABASE ENABLE PUBLICATION;
ALTER DATABASE INCLUDE TABLE customer* TO PUBLICATION;
ALTER DATABASE INCLUDE TABLE order* TO PUBLICATION;
```

#### **Security Hardening**
```sql
-- Secure database configuration
ALTER DATABASE ENCRYPT WITH fbSampleDbCrypt KEY 'ProductionEncryptionKey2024!';
ALTER DATABASE SET DEFAULT DEFINER;
ALTER DATABASE SET LINGER TO 0;  -- Disable lingering for security
```

#### **Performance Optimization**
```sql
-- Optimize for high-throughput applications
ALTER DATABASE SET LINGER TO 300;  -- 5-minute linger for connection pooling
ALTER DATABASE ADD DIFFERENCE FILE '/fast_storage/app_diff.fbk';
ALTER DATABASE SET DEFAULT INVOKER;  -- Better performance for some workloads
```

---

## Database Connection and Management

### **Database Connection**

#### **Basic Connection Syntax**
```sql
-- Connect to database
CONNECT 'database_path' USER 'username' PASSWORD 'password';

-- Connect with role
CONNECT 'database_path' USER 'username' PASSWORD 'password' ROLE 'role_name';

-- Connect to remote database
CONNECT 'server:/path/database.fdb' USER 'username' PASSWORD 'password';
```

#### **Connection Examples**
```sql
-- Local database connection
CONNECT '/data/databases/myapp.fdb' USER 'app_user' PASSWORD 'app_password';

-- Remote database connection
CONNECT 'server1:/data/databases/remote_app.fdb' USER 'remote_user' PASSWORD 'remote_pass';

-- Connection with specific role
CONNECT '/data/databases/myapp.fdb' 
    USER 'admin_user' 
    PASSWORD 'admin_password' 
    ROLE 'DATABASE_ADMINISTRATOR';

-- Connection with certificate authentication (if supported)
CONNECT '/secure/databases/secure_app.fdb' 
    USER 'cert_user' 
    PASSWORD 'cert_password'
    ROLE 'SECURE_ACCESS';
```

### **Database Disconnection**

#### **Disconnect Operations**
```sql
-- Disconnect from current database
DISCONNECT;

-- Disconnect all connections (admin operation)
DISCONNECT ALL;
```

---

## System Catalog Integration

ScratchBird stores database metadata in the RDB$DATABASE system table and related system views.

### **Querying Database Information**

#### **Basic Database Properties**
```sql
-- Show database basic information
SELECT 
    RDB$DESCRIPTION as DATABASE_DESCRIPTION,
    RDB$RELATION_ID as DATABASE_ID,
    RDB$CHARACTER_SET_NAME as DEFAULT_CHARACTER_SET,
    RDB$LINGER as LINGER_SECONDS,
    RDB$SQL_SECURITY as DEFAULT_SQL_SECURITY
FROM RDB$DATABASE;
```

#### **Character Set and Collation Information**
```sql
-- Database character set configuration
SELECT 
    db.RDB$CHARACTER_SET_NAME as DEFAULT_CHARSET,
    cs.RDB$CHARACTER_SET_NAME as CHARSET_NAME,
    cs.RDB$DEFAULT_COLLATE_NAME as DEFAULT_COLLATION,
    cs.RDB$NUMBER_OF_CHARACTERS as MAX_CHARS_PER_BYTE
FROM RDB$DATABASE db
LEFT JOIN RDB$CHARACTER_SETS cs ON db.RDB$CHARACTER_SET_NAME = cs.RDB$CHARACTER_SET_NAME;
```

#### **Database Configuration Analysis**
```sql
-- Complete database configuration
SELECT 
    'DATABASE_INFO' as CATEGORY,
    RDB$DESCRIPTION as DESCRIPTION,
    RDB$CHARACTER_SET_NAME as DEFAULT_CHARSET,
    CASE RDB$SQL_SECURITY 
        WHEN 0 THEN 'INVOKER'
        WHEN 1 THEN 'DEFINER'
        ELSE 'UNKNOWN'
    END as DEFAULT_SECURITY,
    RDB$LINGER as LINGER_TIMEOUT,
    CURRENT_USER as CURRENT_USER,
    CURRENT_ROLE as CURRENT_ROLE
FROM RDB$DATABASE;
```

### **Database Statistics and Monitoring**

#### **Database Size and Usage**
```sql
-- Database storage statistics
SELECT 
    PAGE_SIZE,
    PAGE_SIZE * PAGES as DATABASE_SIZE_BYTES,
    PAGES as TOTAL_PAGES,
    PAGE_SIZE * PAGES / 1024 / 1024 as DATABASE_SIZE_MB
FROM (
    SELECT 
        (SELECT MON$PAGE_SIZE FROM MON$DATABASE) as PAGE_SIZE,
        COUNT(*) as PAGES
    FROM RDB$PAGES
);
```

#### **Object Count Summary**
```sql
-- Database object inventory
SELECT 
    'TABLES' as OBJECT_TYPE,
    COUNT(*) as COUNT
FROM RDB$RELATIONS 
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL

UNION ALL

SELECT 
    'VIEWS' as OBJECT_TYPE,
    COUNT(*) as COUNT
FROM RDB$RELATIONS 
WHERE (RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL)
  AND RDB$RELATION_TYPE = 1

UNION ALL

SELECT 
    'INDEXES' as OBJECT_TYPE,
    COUNT(*) as COUNT
FROM RDB$INDICES 
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL

UNION ALL

SELECT 
    'SEQUENCES' as OBJECT_TYPE,
    COUNT(*) as COUNT
FROM RDB$GENERATORS 
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL

UNION ALL

SELECT 
    'DOMAINS' as OBJECT_TYPE,
    COUNT(*) as COUNT
FROM RDB$FIELDS 
WHERE (RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL)
  AND RDB$FIELD_NAME NOT STARTING WITH 'RDB$'

ORDER BY OBJECT_TYPE;
```

### **Schema and Character Set Usage**

#### **Schema Analysis** 
```sql
-- Schema usage analysis (if hierarchical schemas supported)
SELECT 
    COALESCE(RDB$SCHEMA_NAME, 'DEFAULT') as SCHEMA_NAME,
    COUNT(DISTINCT RDB$RELATION_NAME) as TABLES_COUNT,
    COUNT(DISTINCT f.RDB$FIELD_NAME) as DOMAINS_COUNT
FROM RDB$RELATIONS r
LEFT JOIN RDB$RELATION_FIELDS rf ON r.RDB$RELATION_NAME = rf.RDB$RELATION_NAME
LEFT JOIN RDB$FIELDS f ON rf.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME
WHERE r.RDB$SYSTEM_FLAG = 0 OR r.RDB$SYSTEM_FLAG IS NULL
GROUP BY RDB$SCHEMA_NAME
ORDER BY SCHEMA_NAME;
```

#### **Character Set Usage Analysis**
```sql
-- Character set usage across database objects
SELECT 
    cs.RDB$CHARACTER_SET_NAME as CHARACTER_SET,
    COUNT(DISTINCT f.RDB$FIELD_NAME) as FIELDS_USING,
    COUNT(DISTINCT rf.RDB$RELATION_NAME) as TABLES_USING
FROM RDB$FIELDS f
LEFT JOIN RDB$CHARACTER_SETS cs ON f.RDB$CHARACTER_SET_ID = cs.RDB$CHARACTER_SET_ID
LEFT JOIN RDB$RELATION_FIELDS rf ON f.RDB$FIELD_NAME = rf.RDB$FIELD_SOURCE
WHERE f.RDB$SYSTEM_FLAG = 0 OR f.RDB$SYSTEM_FLAG IS NULL
  AND cs.RDB$CHARACTER_SET_NAME IS NOT NULL
GROUP BY cs.RDB$CHARACTER_SET_NAME
ORDER BY FIELDS_USING DESC;
```

---

## Database Backup and Restore

### **Backup Operations**

#### **Full Database Backup**
```sql
-- Prepare database for backup
ALTER DATABASE BEGIN BACKUP;

-- External backup command (platform-specific)
-- gbak -b -user SYSDBA -password masterkey database.fdb backup.fbk

-- End backup state
ALTER DATABASE END BACKUP;
```

#### **Incremental Backup Setup**
```sql
-- Set up incremental backup with difference file
ALTER DATABASE ADD DIFFERENCE FILE '/backups/incremental.fbk';

-- Perform incremental backup (external command)
-- nbackup -b 1 database.fdb incremental_level1.nbk

-- Remove difference file after backup
ALTER DATABASE DROP DIFFERENCE FILE;
```

### **Database Restore**

#### **Full Database Restore**
```sql
-- External restore command (platform-specific)
-- gbak -r -user SYSDBA -password masterkey backup.fbk restored_database.fdb

-- Verify restored database
CONNECT 'restored_database.fdb' USER 'SYSDBA' PASSWORD 'masterkey';

-- Check database integrity
SELECT COUNT(*) FROM RDB$RELATIONS;
```

#### **Point-in-Time Recovery**
```sql
-- Restore from incremental backup chain (external commands)
-- nbackup -r database.fdb full_backup.nbk incremental_level1.nbk

-- Connect and verify
CONNECT 'database.fdb' USER 'SYSDBA' PASSWORD 'masterkey';

-- Verify database state
SELECT 
    RDB$CHARACTER_SET_NAME,
    RDB$LINGER,
    CURRENT_TIMESTAMP as RESTORED_AT
FROM RDB$DATABASE;
```

---

## Database Security and Encryption

### **Encryption Management**

#### **Enable Database Encryption**
```sql
-- Enable encryption with default plugin
ALTER DATABASE ENCRYPT WITH fbSampleDbCrypt KEY 'SecureKey2024!@#$';

-- Enable encryption with custom plugin
ALTER DATABASE ENCRYPT WITH MyCustomEncryption KEY 'CustomSecureKey123';

-- Verify encryption status (implementation-dependent)
SELECT 
    MON$DATABASE_NAME,
    MON$CREATION_DATE,
    -- Additional monitoring fields for encryption status
    'ENCRYPTED' as SECURITY_STATUS
FROM MON$DATABASE;
```

#### **Disable Database Encryption**
```sql
-- Disable encryption (decrypt database)
ALTER DATABASE DECRYPT;

-- Verify encryption removed
SELECT 
    MON$DATABASE_NAME,
    CURRENT_TIMESTAMP as DECRYPTED_AT
FROM MON$DATABASE;
```

### **Security Configuration**

#### **SQL Security Modes**
```sql
-- Set default to DEFINER (procedures run with definer privileges)
ALTER DATABASE SET DEFAULT DEFINER;

-- Set default to INVOKER (procedures run with invoker privileges)
ALTER DATABASE SET DEFAULT INVOKER;

-- Check current security setting
SELECT 
    CASE RDB$SQL_SECURITY
        WHEN 0 THEN 'INVOKER'
        WHEN 1 THEN 'DEFINER'
        ELSE 'UNKNOWN'
    END as DEFAULT_SQL_SECURITY
FROM RDB$DATABASE;
```

---

## Database Performance Optimization

### **Connection Management**

#### **Linger Configuration**
```sql
-- Set optimal linger time for connection pooling
ALTER DATABASE SET LINGER TO 120;  -- 2 minutes

-- High-throughput applications
ALTER DATABASE SET LINGER TO 300;  -- 5 minutes

-- Security-focused applications
ALTER DATABASE SET LINGER TO 30;   -- 30 seconds

-- Disable lingering completely
ALTER DATABASE DROP LINGER;
```

#### **Performance Monitoring**
```sql
-- Monitor active connections
SELECT 
    MON$ATTACHMENT_ID,
    MON$USER,
    MON$ROLE,
    MON$STATE,
    MON$TIMESTAMP,
    MON$REMOTE_ADDRESS
FROM MON$ATTACHMENTS
WHERE MON$ATTACHMENT_ID <> CURRENT_CONNECTION;

-- Database performance metrics
SELECT 
    MON$DATABASE_NAME,
    MON$PAGE_SIZE,
    MON$PAGES,
    MON$PAGE_BUFFERS,
    MON$CREATION_DATE
FROM MON$DATABASE;
```

### **Page Size Optimization**

#### **Page Size Selection Guidelines**
```sql
-- OLTP workloads (frequent small transactions)
-- Recommended: PAGE_SIZE 4096 or 8192

-- OLAP workloads (large analytical queries)  
-- Recommended: PAGE_SIZE 16384 or 32768

-- Mixed workloads
-- Recommended: PAGE_SIZE 8192 or 16384

-- Check current page size
SELECT MON$PAGE_SIZE FROM MON$DATABASE;
```

---

## Publication and Replication

### **Logical Replication Setup**

#### **Enable Publication**
```sql
-- Enable database for logical replication
ALTER DATABASE ENABLE PUBLICATION;

-- Include specific tables in replication
ALTER DATABASE INCLUDE TABLE customers TO PUBLICATION;
ALTER DATABASE INCLUDE TABLE orders TO PUBLICATION;
ALTER DATABASE INCLUDE TABLE products TO PUBLICATION;

-- Include table patterns
ALTER DATABASE INCLUDE TABLE sales_* TO PUBLICATION;
ALTER DATABASE INCLUDE TABLE log_* TO PUBLICATION;
```

#### **Manage Publication Content**
```sql
-- Exclude sensitive tables from replication
ALTER DATABASE EXCLUDE TABLE user_passwords FROM PUBLICATION;
ALTER DATABASE EXCLUDE TABLE audit_* FROM PUBLICATION;
ALTER DATABASE EXCLUDE TABLE temp_* FROM PUBLICATION;

-- Disable publication entirely
ALTER DATABASE DISABLE PUBLICATION;
```

#### **Publication Monitoring**
```sql
-- Check publication status (implementation-dependent)
SELECT 
    RDB$RELATION_NAME as TABLE_NAME,
    CASE 
        WHEN RDB$SYSTEM_FLAG = 1 THEN 'SYSTEM'
        ELSE 'USER'
    END as TABLE_TYPE,
    'PUBLISHED' as REPLICATION_STATUS
FROM RDB$RELATIONS 
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
ORDER BY RDB$RELATION_NAME;
```

---

## Error Handling and Troubleshooting

### **Common Database Errors**

#### **Database Creation Errors**
```sql
-- Error: Database already exists
-- Solution: Check if file exists, use different path or remove existing file

-- Error: Invalid page size
CREATE DATABASE '/data/test.fdb' PAGE_SIZE 6144;  -- Invalid
-- Solution: Use valid page size (4096, 8192, 16384, 32768)

-- Error: Insufficient permissions
-- Solution: Ensure database directory has write permissions
-- Solution: Run with appropriate user privileges
```

#### **Connection Errors**
```sql
-- Error: Database not found
CONNECT '/nonexistent/database.fdb' USER 'user' PASSWORD 'pass';
-- Solution: Verify database path exists
-- Solution: Check network connectivity for remote databases

-- Error: Authentication failed
-- Solution: Verify username/password combination
-- Solution: Check user exists and has connect privileges
```

### **Database Corruption Issues**

#### **Database Validation**
```sql
-- Connect and perform basic validation
CONNECT 'database.fdb' USER 'SYSDBA' PASSWORD 'masterkey';

-- Check system table integrity
SELECT COUNT(*) FROM RDB$RELATIONS;
SELECT COUNT(*) FROM RDB$FIELDS;
SELECT COUNT(*) FROM RDB$INDICES;

-- Verify database structure
SELECT 
    RDB$RELATION_NAME,
    COUNT(*) as FIELD_COUNT
FROM RDB$RELATION_FIELDS
GROUP BY RDB$RELATION_NAME
HAVING COUNT(*) = 0;  -- Tables with no fields (potential corruption)
```

#### **Recovery Procedures**
```sql
-- External validation commands (platform-specific):
-- gfix -validate -full database.fdb
-- gfix -mend database.fdb

-- Backup and restore for repair:
-- gbak -b database.fdb backup.fbk
-- gbak -r backup.fbk repaired_database.fdb
```

---

## Best Practices

### **Database Design Guidelines**

1. **Page Size Selection**: Choose appropriate page size based on workload characteristics
2. **Character Set Planning**: Select UTF8 for international applications
3. **Security Configuration**: Set appropriate default SQL security mode
4. **Backup Strategy**: Implement regular backup procedures with difference files
5. **Connection Management**: Configure optimal linger times for your application pattern

### **Recommended Database Configurations**

#### **Production OLTP System**
```sql
CREATE DATABASE '/production/databases/oltp_system.fdb'
    PAGE_SIZE 8192
    USER 'prod_admin'
    PASSWORD 'production_secure_password_2024'
    OWNER 'database_administrator'
    ROLE 'PRODUCTION_DBA'
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI
    DIFFERENCE FILE '/production/backups/oltp_diff.fbk'
    LENGTH 100000 PAGES;

-- Post-creation configuration
ALTER DATABASE SET LINGER TO 120;
ALTER DATABASE SET DEFAULT DEFINER;
ALTER DATABASE ENCRYPT WITH fbSampleDbCrypt KEY 'ProductionEncryptionKey2024!';
```

#### **Development Environment**
```sql
CREATE DATABASE '/dev/databases/app_development.fdb'
    PAGE_SIZE 8192
    USER 'developer'
    PASSWORD 'dev_password_123'
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI;

-- Development configuration
ALTER DATABASE SET LINGER TO 30;
ALTER DATABASE SET DEFAULT INVOKER;
```

#### **Analytics Data Warehouse**
```sql
CREATE DATABASE '/warehouse/databases/analytics.fdb'
    PAGE_SIZE 32768
    USER 'analytics_admin'
    PASSWORD 'warehouse_secure_2024'
    OWNER 'analytics_dba'
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI
    DIFFERENCE FILE '/warehouse/backups/analytics_diff.fbk'
    LENGTH 1000000 PAGES;

-- Analytics configuration
ALTER DATABASE SET LINGER TO 300;
ALTER DATABASE SET DEFAULT DEFINER;
ALTER DATABASE ENABLE PUBLICATION;
```

---

## Migration and Deployment

### **Database Migration Strategies**

#### **Cross-Platform Migration**
```sql
-- Source database backup
-- gbak -b source_database.fdb migration_backup.fbk

-- Target database creation
CREATE DATABASE '/target/path/migrated_database.fdb'
    PAGE_SIZE 16384
    USER 'migration_admin'
    PASSWORD 'migration_secure_2024'
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI;

-- Restore with migration
-- gbak -r migration_backup.fbk migrated_database.fdb

-- Post-migration configuration
ALTER DATABASE SET DEFAULT CHARACTER SET UTF8;
ALTER DATABASE SET LINGER TO 60;
```

#### **Version Upgrade Migration**
```sql
-- Pre-upgrade backup
ALTER DATABASE ADD DIFFERENCE FILE '/upgrade/backup_diff.fbk';
-- gbak -b current_database.fdb pre_upgrade_backup.fbk

-- Post-upgrade database recreation
CREATE DATABASE '/upgraded/path/new_version_database.fdb'
    PAGE_SIZE 16384
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI;

-- Restore upgraded database
-- gbak -r pre_upgrade_backup.fbk new_version_database.fdb
```

### **Deployment Automation**

#### **Scripted Database Deployment**
```sql
-- deployment_script.sql
CREATE DATABASE '/production/databases/$(APP_NAME).fdb'
    PAGE_SIZE $(PAGE_SIZE)
    USER '$(DB_USER)'
    PASSWORD '$(DB_PASSWORD)'
    OWNER '$(DB_OWNER)'
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI
    DIFFERENCE FILE '/production/backups/$(APP_NAME)_diff.fbk';

-- Post-deployment configuration
ALTER DATABASE SET LINGER TO $(LINGER_TIME);
ALTER DATABASE SET DEFAULT DEFINER;
ALTER DATABASE ENCRYPT WITH fbSampleDbCrypt KEY '$(ENCRYPTION_KEY)';
```

---

## Implementation Details

### **Primary Implementation Files**

#### **Parser and Grammar**
- **File**: `src/dsql/parse.y:2558-2640`
- **Classes**: `db_clause`, `db_initial_desc`, `db_rem_desc`
- **Functionality**: Parsing CREATE DATABASE and ALTER DATABASE syntax

#### **DDL Node Classes**
- **File**: `src/dsql/DdlNodes.h:2741-2820`
- **Classes**:
  - `AlterDatabaseNode` (lines 2741-2820): Database creation and modification operations
- **Features**: Backup control, encryption, publication management

#### **System Catalog Integration**
- **File**: `src/jrd/relations.h:35-43`
- **Table**: `RDB$DATABASE` - Stores database metadata
- **Fields**:
  - `f_dat_desc`: Database description
  - `f_dat_charset`: Default character set
  - `f_dat_linger`: Connection linger timeout
  - `f_dat_sql_security`: Default SQL security mode

### **Core Database Operations**

#### **AlterDatabaseNode Methods**
- Handles both CREATE and ALTER operations (create flag)
- Backup state management (BEGIN/END BACKUP)
- Encryption control with plugin support
- Publication/replication configuration
- Linger timeout configuration

#### **Database Connection Management**
- Connection establishment and authentication
- Role-based access control
- Character set and collation inheritance
- Transaction isolation and management

#### **Storage Structures**

Database metadata stored in RDB$DATABASE includes:
- **Basic Properties**: Description, ID, character set
- **Security Settings**: SQL security mode, ownership
- **Performance Settings**: Linger timeout, page size
- **Feature Flags**: Publication enabled, encryption status
- **Schema Integration**: Default character set for schemas

---

## Administrative Operations

### **Database Maintenance**

#### **Regular Maintenance Tasks**
```sql
-- Database statistics collection
UPDATE RDB$STATISTICS;

-- Index maintenance
SET STATISTICS INDEX index_name;

-- Database validation (external command)
-- gfix -validate database.fdb

-- Database cleanup (external command)  
-- gfix -sweep database.fdb
```

#### **Monitoring Database Health**
```sql
-- Connection monitoring
SELECT 
    MON$ATTACHMENT_ID,
    MON$USER,
    MON$TIMESTAMP,
    MON$STATE
FROM MON$ATTACHMENTS;

-- Database size monitoring
SELECT 
    MON$PAGE_SIZE * MON$PAGES as DATABASE_SIZE_BYTES,
    MON$PAGES as TOTAL_PAGES,
    MON$PAGE_BUFFERS as BUFFER_PAGES
FROM MON$DATABASE;
```

---

