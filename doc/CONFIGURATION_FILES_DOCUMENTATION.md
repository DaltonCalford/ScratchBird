# ScratchBird Configuration Files - Complete Reference Documentation

## Overview

**Configuration Files** control all aspects of ScratchBird database engine operation, from basic server settings to advanced features like replication, tracing, and internationalization. ScratchBird uses a comprehensive configuration system with multiple specialized files for different subsystems.

### Configuration System Features

ScratchBird's configuration system provides enterprise-grade management capabilities:

- **Hierarchical Configuration**: Multiple configuration files with inheritance and overrides
- **Macro Substitution**: Dynamic directory and variable expansion
- **Per-Database Settings**: Override global settings for specific databases  
- **Plugin Integration**: Extensible configuration for plugin modules
- **Security Integration**: Authentication, encryption, and access control settings
- **Performance Tuning**: Comprehensive memory, cache, and resource management

---

## Configuration File Overview

### Core Configuration Files

| File | Purpose | Scope | Priority |
|------|---------|-------|----------|
| **scratchbird.conf** | Main server configuration | Global | High |
| **databases.conf** | Database aliases and per-DB settings | Database | High |
| **plugins.conf** | Plugin module configuration | Global | Medium |
| **replication.conf** | Database replication settings | Database | Medium |
| **sbtrace.conf** | Activity tracing and audit | Database | Low |
| **sbintl.conf** | Internationalization settings | Global | Low |

### Configuration File Locations

```bash
# Standard installation structure
/opt/scratchbird/
├── conf/                    # Main configuration directory
│   ├── scratchbird.conf     # Primary server configuration
│   ├── databases.conf       # Database aliases and overrides
│   ├── plugins.conf         # Plugin configuration
│   ├── replication.conf     # Replication settings
│   ├── sbintl.conf         # Internationalization
│   └── sbtrace.conf        # Tracing configuration
├── plugins/                 # Plugin modules directory
├── intl/                    # Internationalization modules
└── msg/                     # Message files directory
```

---

## scratchbird.conf - Main Server Configuration

The primary configuration file controlling all aspects of ScratchBird server operation.

### Basic Configuration Structure

#### **File Format**
```ini
# Comments start with hash
# Include other files
include $(dir_conf)/additional.conf

# Global settings
ParameterName = value

# Per-database settings (optional)
{
    database_pattern_or_alias = /path/to/database.fdb
    {
        ParameterName = override_value
        AnotherParameter = database_specific_value
    }
}
```

#### **Macro Substitution**
```ini
# Available directory macros
root = $(root)                    # ScratchBird root directory
install = $(install)              # Installation directory  
conf = $(dir_conf)               # Configuration directory
plugins = $(dir_plugins)         # Plugins directory
security = $(dir_secDb)          # Security database directory
messages = $(dir_msg)            # Message files directory

# Usage examples
SecurityDatabase = $(dir_secDb)/security4.fdb
PluginModule = $(dir_plugins)/udr_engine.so
LogLocation = $(root)/logs/scratchbird.log
```

### Database Access Control

#### **File System Access Control**
```ini
# DatabaseAccess - Controls database file access
# Values: None, Full, Restrict
DatabaseAccess = Restrict

# RestrictedFileSystemAccess - Allowed file system paths
RestrictedFileSystemAccess = /opt/scratchbird/databases;/backup/scratchbird

# ExternalFileAccess - External table file access
# Values: None, Full, Restrict  
ExternalFileAccess = Restrict

# UdfAccess - User-defined function library access
# Values: None, Full, Restrict
UdfAccess = Restrict

# Examples of secure configurations
DatabaseAccess = Restrict
RestrictedFileSystemAccess = /opt/scratchbird/databases;/app/databases;/backup
ExternalFileAccess = None
UdfAccess = Restrict
```

#### **Remote Access Control**  
```ini
# RemoteAccess - Remote database connections
# Values: true (allow), false (local only)
RemoteAccess = true

# RemoteServicePort - TCP port for remote connections
RemoteServicePort = 3050

# RemoteBindAddress - Bind to specific IP addresses
RemoteBindAddress = 192.168.1.100;10.0.0.50

# MaxConnections - Maximum concurrent connections
MaxConnections = 100

# ConnectionTimeout - Connection timeout in seconds
ConnectionTimeout = 180

# Example secure remote configuration
RemoteAccess = true
RemoteServicePort = 3050
RemoteBindAddress = 192.168.1.100
MaxConnections = 50
ConnectionTimeout = 300
```

### Memory Management

#### **Database Cache Settings**
```ini
# DefaultDbCachePages - Default database page cache size
# Value: Number of pages (page size typically 8KB or 16KB)
DefaultDbCachePages = 2048

# MinDbCachePages - Minimum cache pages per database
MinDbCachePages = 256

# MaxDbCachePages - Maximum cache pages per database  
MaxDbCachePages = 131072

# TempCacheLimit - Temporary data cache limit in bytes
TempCacheLimit = 67108864          # 64MB

# Example memory configuration for high-performance server
DefaultDbCachePages = 8192         # 64MB with 8KB pages
MinDbCachePages = 1024            # 8MB minimum
MaxDbCachePages = 65536           # 512MB maximum
TempCacheLimit = 268435456        # 256MB temp cache
```

#### **System Memory Settings**
```ini
# LockMemSize - Lock manager memory in bytes
LockMemSize = 1048576             # 1MB

# EventMemSize - Event manager memory  
EventMemSize = 65536              # 64KB

# SnapshotsMemSize - Snapshot table memory
SnapshotsMemSize = 65536          # 64KB

# SortMemBlockSize - Sort buffer memory per operation
SortMemBlockSize = 1048576        # 1MB

# SortMemUpperLimit - Maximum sort memory total
SortMemUpperLimit = 67108864      # 64MB

# High-performance memory configuration
LockMemSize = 4194304             # 4MB locks
EventMemSize = 262144             # 256KB events
SnapshotsMemSize = 262144         # 256KB snapshots
SortMemBlockSize = 2097152        # 2MB sort blocks
SortMemUpperLimit = 134217728     # 128MB sort total
```

### Network and Protocol Settings

#### **TCP/IP Configuration**
```ini
# TcpRemoteBufferSize - Network buffer size in bytes
TcpRemoteBufferSize = 8192

# TcpNoDelay - Disable Nagle algorithm for lower latency
# Values: true (disable Nagle), false (enable Nagle)
TcpNoDelay = false

# IPv6V6Only - IPv6 socket configuration
# Values: true (IPv6 only), false (dual stack)
IPv6V6Only = false

# Optimized network settings
TcpRemoteBufferSize = 32768       # 32KB buffers
TcpNoDelay = true                 # Low latency
IPv6V6Only = false                # Support both IPv4/IPv6
```

#### **Wire Encryption and Compression**
```ini
# WireCrypt - Wire protocol encryption
# Values: Required, Enabled, Disabled
WireCrypt = Enabled

# WireCompression - Wire protocol compression
# Values: true, false
WireCompression = false

# KeyHolderPlugin - Key management plugin
KeyHolderPlugin = fbSampleKeyHolder

# Security-focused configuration
WireCrypt = Required              # Mandatory encryption
WireCompression = true           # Reduce bandwidth
KeyHolderPlugin = fbSampleKeyHolder
```

### Authentication and Security

#### **Authentication Configuration**
```ini
# AuthServer - Server-side authentication plugins (ordered list)
AuthServer = Srp256, Srp, Legacy_Auth

# AuthClient - Client-side authentication plugins  
AuthClient = Srp256, Srp, Legacy_Auth

# SecurityDatabase - Security database location
SecurityDatabase = $(dir_secDb)/security4.fdb

# UserManager - User management plugin
UserManager = Srp

# Secure authentication configuration
AuthServer = Srp256               # Only strongest authentication
AuthClient = Srp256
SecurityDatabase = $(dir_secDb)/security4.fdb
UserManager = Srp
```

#### **Access Control**
```ini
# AdminRoleName - Database administrator role name
AdminRoleName = RDB$ADMIN

# ProcessPriorityLevel - Server process priority
# Values: 0 (normal), 1 (high), -1 (low)
ProcessPriorityLevel = 0

# RedirectToErrorLog - Redirect console output to error log
RedirectToErrorLog = true

# Administrative security settings
AdminRoleName = RDB$ADMIN
ProcessPriorityLevel = 0
RedirectToErrorLog = true
```

### Performance and Optimization

#### **Garbage Collection**
```ini
# GCPolicy - Garbage collection policy
# Values: cooperative, background, combined
GCPolicy = combined

# SweepInterval - Automatic sweep interval (0 = disabled)
SweepInterval = 20000

# Optimized garbage collection
GCPolicy = background             # Non-blocking GC
SweepInterval = 50000            # Less frequent sweeps
```

#### **Statement and Query Settings**
```ini
# StatementTimeout - SQL statement timeout in seconds (0 = unlimited)
StatementTimeout = 0

# ReadConsistency - Statement-level read consistency
# Values: true, false
ReadConsistency = true

# OptimizeMode - Query optimizer mode
# Values: cost, first_rows
OptimizeMode = cost

# Query optimization settings
StatementTimeout = 300           # 5 minute timeout
ReadConsistency = true          # Consistent reads
OptimizeMode = cost             # Cost-based optimization
```

#### **Parallel Processing**
```ini
# MaxParallelWorkers - Maximum parallel worker processes
MaxParallelWorkers = 1

# ParallelWorkers - Default parallel workers per operation
ParallelWorkers = 1

# Parallel processing configuration
MaxParallelWorkers = 4           # Utilize multiple cores
ParallelWorkers = 2             # Default parallelism
```

### Logging and Monitoring

#### **Error Logging**
```ini
# LogFileSize - Maximum log file size in KB
LogFileSize = 2048              # 2MB

# MaxErrorLogs - Maximum number of error log files
MaxErrorLogs = 5

# Log configuration
LogFileSize = 10240             # 10MB logs
MaxErrorLogs = 10              # Keep 10 files
```

### Complete Example Configuration

#### **Production Server Configuration**
```ini
# ScratchBird Production Configuration
# Company: Enterprise Corp
# Environment: Production
# Last Updated: 2024-12-27

#=============================================
# DATABASE ACCESS CONTROL
#=============================================
DatabaseAccess = Restrict
RestrictedFileSystemAccess = /opt/scratchbird/databases;/backup/scratchbird;/import/scratchbird
ExternalFileAccess = Restrict
UdfAccess = Restrict

#=============================================
# MEMORY MANAGEMENT
#=============================================
DefaultDbCachePages = 16384        # 128MB cache with 8KB pages
MinDbCachePages = 2048            # 16MB minimum
MaxDbCachePages = 65536           # 512MB maximum
TempCacheLimit = 268435456        # 256MB temporary cache
LockMemSize = 4194304             # 4MB lock memory
EventMemSize = 262144             # 256KB event memory
SnapshotsMemSize = 262144         # 256KB snapshot memory

#=============================================
# NETWORK AND SECURITY
#=============================================
RemoteAccess = true
RemoteServicePort = 3050
RemoteBindAddress = 192.168.1.100
MaxConnections = 100
ConnectionTimeout = 300
TcpRemoteBufferSize = 32768
TcpNoDelay = true
WireCrypt = Required
WireCompression = true

#=============================================
# AUTHENTICATION
#=============================================
AuthServer = Srp256
AuthClient = Srp256
SecurityDatabase = $(dir_secDb)/security4.fdb
UserManager = Srp

#=============================================
# PERFORMANCE OPTIMIZATION
#=============================================
GCPolicy = background
SweepInterval = 50000
StatementTimeout = 300
ReadConsistency = true
OptimizeMode = cost
MaxParallelWorkers = 4
ParallelWorkers = 2

#=============================================
# LOGGING
#=============================================
LogFileSize = 10240
MaxErrorLogs = 10
RedirectToErrorLog = true
```

---

## databases.conf - Database Aliases and Configuration

Controls database aliases and provides per-database configuration overrides.

### Database Alias Definition

#### **Basic Alias Syntax**
```ini
# Simple alias
alias_name = /path/to/database.fdb

# Alias with connection parameters
sales_db = /opt/scratchbird/databases/sales.fdb

# Windows paths (use forward slashes or escaped backslashes)
customer_db = c:/scratchbird/databases/customers.fdb
# or
customer_db = c:\\scratchbird\\databases\\customers.fdb
```

#### **Security Database Configuration**
```ini
# Security database alias (required)
security4.fdb = $(dir_secDb)/security4.fdb

# Default sample database
employee.fdb = $(dir_sampleDb)/employee.fdb
```

### Per-Database Configuration Overrides

#### **Override Syntax**
```ini
# Database with specific configuration
{
    database_pattern = /path/to/special.fdb
    {
        ParameterName = override_value
        AnotherParameter = specific_setting
    }
}

# Multiple databases with same settings
{
    /opt/scratchbird/databases/*.fdb
    {
        DefaultDbCachePages = 4096
        GCPolicy = cooperative
    }
}
```

#### **Common Override Examples**
```ini
# High-performance OLTP database
{
    sales_oltp = /opt/scratchbird/databases/sales_oltp.fdb
    {
        DefaultDbCachePages = 32768      # 256MB cache
        GCPolicy = background           # Non-blocking GC
        ReadConsistency = false         # Faster reads
        TempCacheLimit = 134217728     # 128MB temp space
    }
}

# Analytics/Reporting database
{
    analytics_db = /opt/scratchbird/databases/analytics.fdb
    {
        DefaultDbCachePages = 65536     # 512MB cache
        SortMemBlockSize = 4194304     # 4MB sort blocks
        SortMemUpperLimit = 268435456  # 256MB sort total
        StatementTimeout = 1800        # 30 minute timeout
    }
}

# Development database with relaxed security
{
    /opt/scratchbird/dev_databases/*.fdb
    {
        WireCrypt = Enabled           # Optional encryption
        StatementTimeout = 0          # No timeout
        DefaultDbCachePages = 1024    # Smaller cache
    }
}
```

### Complete databases.conf Example

```ini
# ScratchBird Database Aliases Configuration
# Production Environment

#=============================================
# SYSTEM DATABASES
#=============================================
security4.fdb = $(dir_secDb)/security4.fdb
employee.fdb = $(dir_sampleDb)/employee.fdb

#=============================================
# PRODUCTION DATABASE ALIASES
#=============================================
# Primary application databases
sales_db = /opt/scratchbird/databases/sales.fdb
customer_db = /opt/scratchbird/databases/customers.fdb
inventory_db = /opt/scratchbird/databases/inventory.fdb
accounting_db = /opt/scratchbird/databases/accounting.fdb

# Analytics and reporting
analytics_db = /opt/scratchbird/databases/analytics.fdb
reports_db = /opt/scratchbird/databases/reports.fdb

# Archive databases
sales_archive_2023 = /archive/scratchbird/sales_2023.fdb
sales_archive_2022 = /archive/scratchbird/sales_2022.fdb

#=============================================
# PER-DATABASE CONFIGURATIONS
#=============================================

# High-performance OLTP configuration
{
    sales_db = /opt/scratchbird/databases/sales.fdb
    customer_db = /opt/scratchbird/databases/customers.fdb
    {
        DefaultDbCachePages = 32768
        GCPolicy = background
        ReadConsistency = false
        TempCacheLimit = 134217728
        SweepInterval = 100000
    }
}

# Analytics database configuration
{
    analytics_db = /opt/scratchbird/databases/analytics.fdb
    reports_db = /opt/scratchbird/databases/reports.fdb
    {
        DefaultDbCachePages = 65536
        SortMemBlockSize = 8388608
        SortMemUpperLimit = 536870912
        StatementTimeout = 3600
        MaxParallelWorkers = 8
        ParallelWorkers = 4
    }
}

# Archive database configuration (read-only optimized)
{
    /archive/scratchbird/*.fdb
    {
        DefaultDbCachePages = 16384
        GCPolicy = cooperative
        ReadConsistency = true
        StatementTimeout = 600
    }
}
```

---

## plugins.conf - Plugin Configuration

Configures ScratchBird plugin modules and their settings.

### Plugin Definition Structure

#### **Plugin Module Syntax**
```ini
# Plugin module definition
plugin_module module_name
{
    filename = $(dir_plugins)/plugin_file
    config = plugin_configuration_file
}
```

### Core Plugin Modules

#### **UDR Engine Plugin**
```ini
# User-Defined Routines Engine
plugin_module udr_engine
{
    filename = $(dir_plugins)/udr_engine
    config = $(dir_conf)/udr_engine.conf
}
```

#### **Encryption Plugins**
```ini
# ChaCha64 wire encryption
plugin_module ChaCha64
{
    filename = $(dir_plugins)/ChaCha
}

# Database encryption (sample)
plugin_module fbSampleDbCrypt
{
    filename = $(dir_plugins)/fbSampleDbCrypt
    config = $(dir_conf)/dbcrypt.conf
}

# Key holder (sample)
plugin_module fbSampleKeyHolder
{
    filename = $(dir_plugins)/fbSampleKeyHolder
    config = $(dir_conf)/keyholder.conf
}
```

#### **Authentication Plugins**
```ini
# SRP authentication
plugin_module Srp256
{
    filename = $(dir_plugins)/Srp
}

plugin_module Srp
{
    filename = $(dir_plugins)/Srp
}

# Legacy authentication (compatibility)
plugin_module Legacy_Auth
{
    filename = $(dir_plugins)/Legacy_Auth
}
```

### Complete plugins.conf Example

```ini
# ScratchBird Plugin Configuration
# Production Environment

#=============================================
# USER-DEFINED ROUTINES
#=============================================
plugin_module udr_engine
{
    filename = $(dir_plugins)/udr_engine
    config = $(dir_conf)/udr_engine.conf
}

#=============================================
# AUTHENTICATION PLUGINS
#=============================================
plugin_module Srp256
{
    filename = $(dir_plugins)/Srp
}

plugin_module Srp
{
    filename = $(dir_plugins)/Srp
}

#=============================================
# ENCRYPTION PLUGINS  
#=============================================
plugin_module ChaCha64
{
    filename = $(dir_plugins)/ChaCha
}

plugin_module fbSampleDbCrypt
{
    filename = $(dir_plugins)/fbSampleDbCrypt
    config = $(dir_conf)/dbcrypt.conf
}

plugin_module fbSampleKeyHolder
{
    filename = $(dir_plugins)/fbSampleKeyHolder
    config = $(dir_conf)/keyholder.conf
}
```

---

## replication.conf - Database Replication Configuration

Configures database replication settings for primary and replica database servers.

### Replication Architecture

#### **Replication Components**
```ini
# Primary side configuration
database = /path/to/primary.fdb
{
    # Journal and replication settings
    buffer_size = 1M
    journal_segment_size = 16M
    journal_archive_command = $(replication_script)
    sync_replica = true
}

# Replica side configuration  
replica_database = /path/to/replica.fdb
{
    # Source journal configuration
    journal_source_directory = /journal/source
    journal_apply_timeout = 60
    source_guid = {12345678-1234-5678-9ABC-123456789012}
}
```

### Primary Database Configuration

#### **Journal Management**
```ini
# Primary database replication settings
{
    database = /opt/scratchbird/databases/sales.fdb
    {
        # Enable replication
        enable_replication = true
        
        # Buffer configuration
        buffer_size = 2097152              # 2MB buffer
        
        # Journal segment management
        journal_segment_size = 16777216    # 16MB segments
        journal_directory = /replication/journals/sales
        
        # Archive command for journal segments
        journal_archive_command = /scripts/archive_journal.sh %f %p
        journal_archive_timeout = 60       # 60 seconds
        
        # Synchronous replica connections
        sync_replica = true
        
        # Schema/table filtering
        include_filter = sales.*;accounting.transactions;inventory.stock_levels
        exclude_filter = temp.*;log_tables.*;audit_temp.*
    }
}
```

#### **Filtering Configuration**
```ini
# Advanced filtering for selective replication
{
    database = /opt/scratchbird/databases/main.fdb
    {
        enable_replication = true
        
        # Include specific schemas and tables
        include_filter = 
            sales.*;              # All sales schema objects
            customer.main_data;   # Specific customer table
            product.catalog;      # Product catalog
            inventory.current_stock
            
        # Exclude temporary and log tables
        exclude_filter = 
            *.temp_*;            # All temp tables
            *.log_*;             # All log tables
            audit.session_*;     # Session audit tables
            backup.*;            # Backup schema
            
        # DDL replication control
        replicate_ddl = true
        replicate_user_ddl = false    # Exclude user management DDL
    }
}
```

### Replica Database Configuration

#### **Journal Source Configuration**
```ini
# Replica database configuration
{
    replica_database = /opt/scratchbird/databases/sales_replica.fdb
    {
        # Source journal location
        journal_source_directory = /replication/journals/sales
        
        # Source database identification
        source_guid = {A1B2C3D4-E5F6-7890-ABCD-1234567890AB}
        
        # Apply timeout and error handling
        journal_apply_timeout = 120       # 2 minutes
        journal_apply_max_retries = 3
        
        # Schema search path for compatibility
        schema_search_path = public;sales;customer
        
        # Conflict resolution
        conflict_resolution = source_wins  # primary_wins, replica_wins, manual
        
        # Performance settings
        apply_batch_size = 1000           # Transactions per batch
        apply_parallel_workers = 2        # Parallel apply workers
    }
}
```

### Complete replication.conf Example

```ini
# ScratchBird Replication Configuration
# Multi-site replication setup

#=============================================
# PRIMARY SITE CONFIGURATION (Site A)
#=============================================

# Main sales database (primary)
{
    database = /opt/scratchbird/databases/sales.fdb
    {
        enable_replication = true
        buffer_size = 4194304             # 4MB buffer
        journal_segment_size = 33554432   # 32MB segments
        journal_directory = /replication/journals/sales
        journal_archive_command = /scripts/ship_journal.sh %f %p site_b
        journal_archive_timeout = 30
        sync_replica = true
        
        # Replicate core business data
        include_filter = sales.*;customer.*;product.catalog;inventory.stock
        exclude_filter = *.temp_*;*.log_*;backup.*
        replicate_ddl = true
        replicate_user_ddl = false
    }
}

# Customer database (primary)
{
    database = /opt/scratchbird/databases/customer.fdb
    {
        enable_replication = true
        buffer_size = 2097152             # 2MB buffer
        journal_segment_size = 16777216   # 16MB segments
        journal_directory = /replication/journals/customer
        journal_archive_command = /scripts/ship_journal.sh %f %p site_b
        sync_replica = false              # Async for customer data
        
        include_filter = customer.*;contact.*;preference.*
        exclude_filter = temp.*;session.*;cache.*
    }
}

#=============================================
# REPLICA SITE CONFIGURATION (Site B)
#=============================================

# Sales replica database
{
    replica_database = /opt/scratchbird/databases/sales_replica.fdb
    {
        journal_source_directory = /replication/received/sales
        source_guid = {12345678-ABCD-EF01-2345-6789ABCDEF01}
        journal_apply_timeout = 300
        journal_apply_max_retries = 5
        conflict_resolution = source_wins
        apply_batch_size = 2000
        apply_parallel_workers = 4
        
        # Schema mapping for compatibility
        schema_search_path = public;sales;customer;product;inventory
    }
}

# Customer replica database
{
    replica_database = /opt/scratchbird/databases/customer_replica.fdb
    {
        journal_source_directory = /replication/received/customer
        source_guid = {87654321-DCBA-10FE-5432-FEDCBA987654}
        journal_apply_timeout = 180
        conflict_resolution = source_wins
        apply_batch_size = 1000
        apply_parallel_workers = 2
    }
}

#=============================================
# ARCHIVE CONFIGURATION
#=============================================

# Journal cleanup and archival
journal_retention_days = 7
journal_compression = true
journal_archive_location = /archive/replication
journal_cleanup_schedule = "0 2 * * *"    # Daily at 2 AM
```

---

## sbtrace.conf - Activity Tracing Configuration

Configures comprehensive database activity tracing for monitoring, debugging, and audit purposes.

### Trace Configuration Structure

#### **Basic Trace Session**
```ini
# Trace session configuration
<database %path_to_database%>
{
    # Enable tracing components
    enabled = true
    log_filename = /logs/trace_%database%.log
    max_log_size = 100M
    
    # What to trace
    trace_connections = true
    trace_transactions = true  
    trace_statements = true
    trace_procedures = true
    trace_triggers = true
    trace_errors = true
}
```

### Database Activity Tracing

#### **SQL Statement Tracing**
```ini
# Comprehensive SQL tracing
<database /opt/scratchbird/databases/sales.fdb>
{
    enabled = true
    log_filename = /logs/sales_trace.log
    max_log_size = 500M
    
    # Statement tracing configuration
    trace_statements = true
    
    # SQL filtering (include/exclude patterns)
    include_filter = 
        SELECT.*FROM customer.*;     # Customer queries
        INSERT INTO orders.*;        # Order inserts
        UPDATE inventory.*;          # Inventory updates
        
    exclude_filter = 
        SELECT.*FROM temp_.*;        # Temporary tables
        SELECT.*system_info.*;       # System queries
        SELECT 1;                    # Health checks
        
    # Performance thresholds
    time_threshold = 1000            # Log statements > 1 second
    max_sql_length = 500            # Truncate SQL at 500 chars
    
    # Execution plans
    trace_execution_plan = true
    explain_plan = true
    
    # Parameter values
    log_parameter_values = true
    max_parameter_length = 100
}
```

#### **Connection and Transaction Tracing**
```ini
# Connection monitoring
<database /opt/scratchbird/databases/sales.fdb>
{
    trace_connections = true
    trace_connection_start = true
    trace_connection_end = true
    
    # Authentication tracing
    trace_auth_events = true
    log_failed_auth = true
    
    # Transaction monitoring
    trace_transactions = true
    trace_transaction_start = true
    trace_transaction_commit = true
    trace_transaction_rollback = true
    
    # Transaction timeouts and conflicts
    trace_transaction_conflicts = true
    trace_deadlocks = true
}
```

#### **Procedure and Trigger Tracing**
```ini
# Stored procedure and trigger monitoring
<database /opt/scratchbird/databases/sales.fdb>
{
    # Procedure execution tracing
    trace_procedures = true
    trace_procedure_start = true
    trace_procedure_finish = true
    
    # Procedure filtering
    include_procedures = 
        calculate_commission;        # Specific procedures
        process_order.*;            # Pattern matching
        finance.accounting.*;       # Schema-qualified
        
    exclude_procedures = 
        debug_.*;                   # Debug procedures
        temp_.*;                    # Temporary procedures
        
    # Trigger execution tracing
    trace_triggers = true
    trace_trigger_start = true
    trace_trigger_finish = true
    
    # Trigger filtering
    include_triggers = 
        audit_.*;                   # Audit triggers
        validation_.*;              # Validation triggers
        
    exclude_triggers = 
        temp_.*;                    # Temporary triggers
        log_.*;                     # Logging triggers
}
```

### Error and Performance Monitoring

#### **Error Tracking**
```ini
# Comprehensive error monitoring
<database /opt/scratchbird/databases/sales.fdb>
{
    # Error tracing
    trace_errors = true
    trace_warnings = true
    
    # GDS error code filtering
    include_gds_codes = 
        335544321;                  # Arithmetic overflow
        335544347;                  # Unique constraint violation
        335544558;                  # Lock conflict
        335544665;                  # Foreign key violation
        
    exclude_gds_codes = 
        335544569;                  # Connection lost (too frequent)
        
    # SQL error patterns
    include_error_patterns = 
        .*deadlock.*;               # Deadlock errors
        .*constraint.*violation.*;   # Constraint violations
        .*arithmetic.*overflow.*;    # Arithmetic errors
        
    # Context information
    log_error_context = true
    log_call_stack = true
    max_call_stack_depth = 10
}
```

#### **Performance Monitoring**
```ini
# Performance analysis configuration
<database /opt/scratchbird/databases/sales.fdb>
{
    # Performance metrics
    trace_performance = true
    
    # Timing thresholds
    time_threshold = 100            # Log operations > 100ms
    slow_query_threshold = 5000     # Flag queries > 5 seconds
    
    # Resource usage tracking
    trace_memory_usage = true
    trace_io_stats = true
    trace_lock_stats = true
    
    # Query statistics
    log_record_counts = true
    log_index_usage = true
    log_sort_operations = true
    
    # Cache statistics
    trace_cache_hits = true
    trace_cache_misses = true
}
```

### Service Activity Tracing

#### **Service Operations**
```ini
# Service tracing (backup, restore, etc.)
<services>
{
    enabled = true
    log_filename = /logs/services_trace.log
    max_log_size = 100M
    
    # Service operation tracing
    trace_service_attach = true
    trace_service_detach = true
    
    # Backup/restore operations
    trace_backup = true
    trace_restore = true
    trace_repair = true
    trace_sweep = true
    
    # User management services
    trace_user_management = true
    
    # Service errors and warnings
    trace_service_errors = true
    trace_service_warnings = true
    
    # Performance for long operations
    service_time_threshold = 10000  # 10 seconds
}
```

### Complete sbtrace.conf Example

```ini
# ScratchBird Comprehensive Trace Configuration
# Production Monitoring Setup

#=============================================
# PRIMARY SALES DATABASE TRACING
#=============================================
<database /opt/scratchbird/databases/sales.fdb>
{
    enabled = true
    log_filename = /logs/trace/sales_%date%.log
    max_log_size = 1000M
    log_rotation = daily
    
    # SQL Statement Analysis
    trace_statements = true
    time_threshold = 500            # Log slow queries (500ms+)
    max_sql_length = 1000
    trace_execution_plan = true
    log_parameter_values = true
    
    # Include business-critical operations
    include_filter = 
        INSERT INTO orders.*;
        UPDATE inventory.stock_levels.*;
        SELECT.*FROM customer_summary.*;
        EXECUTE PROCEDURE calculate_commission.*;
        
    # Exclude high-frequency maintenance queries
    exclude_filter = 
        SELECT 1;                   # Health checks
        SELECT.*FROM system_log.*;   # Log queries
        INSERT INTO audit_trail.*;   # Audit inserts
        
    # Connection and Transaction Monitoring
    trace_connections = true
    trace_transactions = true
    trace_deadlocks = true
    trace_transaction_conflicts = true
    
    # Procedure and Trigger Monitoring
    trace_procedures = true
    include_procedures = 
        calculate_commission;
        process_order;
        validate_inventory;
        update_customer_balance;
        
    trace_triggers = true
    include_triggers = 
        audit_orders_trigger;
        validate_stock_trigger;
        update_totals_trigger;
        
    # Error Monitoring
    trace_errors = true
    trace_warnings = true
    include_gds_codes = 335544347;335544558;335544665  # Key violations, locks, FK
    log_error_context = true
    
    # Performance Metrics
    trace_performance = true
    trace_io_stats = true
    trace_cache_hits = true
    log_index_usage = true
}

#=============================================
# CUSTOMER DATABASE TRACING
#=============================================
<database /opt/scratchbird/databases/customer.fdb>
{
    enabled = true
    log_filename = /logs/trace/customer_%date%.log
    max_log_size = 500M
    
    # Focus on customer operations
    trace_statements = true
    time_threshold = 1000
    
    include_filter = 
        SELECT.*FROM customers.*;
        UPDATE customer_profile.*;
        INSERT INTO customer_contact.*;
        
    # Minimal logging for high-frequency operations
    exclude_filter = 
        SELECT.*FROM customer_session.*;
        INSERT INTO activity_log.*;
        
    trace_connections = true
    trace_errors = true
    include_gds_codes = 335544347;335544665
}

#=============================================
# ANALYTICS DATABASE TRACING
#=============================================
<database /opt/scratchbird/databases/analytics.fdb>
{
    enabled = true
    log_filename = /logs/trace/analytics_%date%.log
    max_log_size = 200M
    
    # Long-running query analysis
    trace_statements = true
    time_threshold = 10000          # 10 second threshold
    max_sql_length = 2000
    trace_execution_plan = true
    
    # Focus on complex queries
    include_filter = 
        SELECT.*GROUP BY.*;
        SELECT.*ORDER BY.*;
        SELECT.*JOIN.*;
        
    # Performance monitoring for analytics
    trace_performance = true
    trace_memory_usage = true
    log_sort_operations = true
    
    trace_errors = true
    trace_warnings = true
}

#=============================================
# SERVICE OPERATIONS TRACING
#=============================================
<services>
{
    enabled = true
    log_filename = /logs/trace/services_%date%.log
    max_log_size = 100M
    
    trace_backup = true
    trace_restore = true
    trace_repair = true
    trace_sweep = true
    trace_user_management = true
    
    service_time_threshold = 30000  # 30 seconds
    trace_service_errors = true
    trace_service_warnings = true
}

#=============================================
# GLOBAL TRACE SETTINGS
#=============================================
# Maximum trace sessions
max_trace_sessions = 10

# Default log rotation
default_log_rotation = weekly
default_log_retention_days = 30

# Emergency trace disable
emergency_disable_file = /tmp/disable_trace

# Performance impact limits
max_trace_overhead_percent = 5
auto_disable_on_overhead = true
```

---

## sbintl.conf - Internationalization Configuration

Configures character sets, collations, and internationalization modules for multi-language database support.

### Internationalization Module Configuration

#### **Module Definition**
```ini
# Internationalization module structure
intl_module module_name
{
    filename = $(dir_intl)/module_file
    icu_version = version_number
}
```

#### **Built-in and External Modules**
```ini
# Built-in internationalization support
intl_module builtin
{
    filename = $(dir_intl)/builtin
}

# External internationalization module
intl_module fbintl
{
    filename = $(dir_intl)/fbintl
    icu_version = 52.1
}
```

### Character Set Support

#### **Asian Character Sets**
```ini
# Japanese character sets
charset SJIS_0208
{
    intl_module = fbintl
    collation = SJIS_0208
    collation = SJIS_0208_CI    # Case insensitive
}

charset EUCJ_0208
{
    intl_module = fbintl
    collation = EUCJ_0208
}

# Korean character set
charset KSC_5601
{
    intl_module = fbintl
    collation = KSC_5601
    collation = KSC_5601_CI
}

# Chinese character sets
charset BIG_5
{
    intl_module = fbintl
    collation = BIG_5
    collation = BIG_5_CI
}

charset GB_2312
{
    intl_module = fbintl
    collation = GB_2312
    collation = GB_2312_CI
}

charset GBK
{
    intl_module = fbintl
    collation = GBK
    collation = GBK_CI
}

charset GB18030
{
    intl_module = fbintl
    collation = GB18030
    collation = GB18030_CI
    collation = GB18030_UNICODE
}
```

#### **European Character Sets**
```ini
# Western European (ISO Latin-1)
charset ISO8859_1
{
    intl_module = fbintl
    collation = ISO8859_1
    collation = ISO8859_1_CI
    collation = DA_DA           # Danish
    collation = DE_DE           # German
    collation = EN_UK           # English UK
    collation = EN_US           # English US
    collation = ES_ES           # Spanish
    collation = FR_FR           # French
    collation = IT_IT           # Italian
    collation = NL_NL           # Dutch
    collation = PT_PT           # Portuguese
    collation = SV_SV           # Swedish
}

# Eastern European (ISO Latin-2)
charset ISO8859_2
{
    intl_module = fbintl
    collation = ISO8859_2
    collation = ISO8859_2_CI
    collation = CS_CZ           # Czech
    collation = HU_HU           # Hungarian
    collation = PL_PL           # Polish
    collation = SK_SK           # Slovak
}

# Cyrillic (ISO Latin-5)
charset ISO8859_5
{
    intl_module = fbintl
    collation = ISO8859_5
    collation = ISO8859_5_CI
    collation = RU_RU           # Russian
    collation = BG_BG           # Bulgarian
    collation = SR_SR           # Serbian
}
```

#### **Windows Code Pages**
```ini
# Windows Western European
charset WIN1252
{
    intl_module = fbintl
    collation = WIN1252
    collation = WIN1252_CI
    collation = PXW_INTL        # Paradox International
    collation = PXW_INTL850     # Paradox International 850
    collation = PXW_NORDAN4     # Paradox Nordic Danish
    collation = PXW_SPAN        # Paradox Spanish
    collation = PXW_SWEDFIN     # Paradox Swedish/Finnish
}

# Windows Eastern European
charset WIN1250
{
    intl_module = fbintl
    collation = WIN1250
    collation = WIN1250_CI
    collation = PXW_CSY         # Paradox Czech/Slovak
    collation = PXW_HUNDC       # Paradox Hungarian
    collation = PXW_PLK         # Paradox Polish
}

# Windows Cyrillic
charset WIN1251
{
    intl_module = fbintl
    collation = WIN1251
    collation = WIN1251_CI
    collation = PXW_CYRL        # Paradox Cyrillic
}
```

### Unicode Support

#### **UTF-8 Configuration**
```ini
# UTF-8 Unicode character set
charset UTF8
{
    intl_module = fbintl
    collation = UTF8
    collation = UTF8_CI         # Case insensitive
    collation = UNICODE         # Standard Unicode
    collation = UNICODE_CI      # Unicode case insensitive
    collation = UNICODE_FSS     # Unicode File System Safe
}
```

### Complete sbintl.conf Example

```ini
# ScratchBird Internationalization Configuration
# Multi-language support for global applications

#=============================================
# INTERNATIONALIZATION MODULES
#=============================================
intl_module builtin
{
    filename = $(dir_intl)/builtin
}

intl_module fbintl
{
    filename = $(dir_intl)/fbintl
    icu_version = 52.1
}

#=============================================
# UNICODE CHARACTER SETS
#=============================================
charset UTF8
{
    intl_module = fbintl
    collation = UTF8
    collation = UTF8_CI
    collation = UNICODE
    collation = UNICODE_CI
    collation = UNICODE_FSS
}

#=============================================
# WESTERN EUROPEAN CHARACTER SETS
#=============================================
charset ISO8859_1
{
    intl_module = fbintl
    collation = ISO8859_1
    collation = ISO8859_1_CI
    collation = DA_DA
    collation = DE_DE
    collation = EN_UK
    collation = EN_US
    collation = ES_ES
    collation = FR_FR
    collation = IT_IT
    collation = NL_NL
    collation = PT_PT
    collation = SV_SV
}

charset WIN1252
{
    intl_module = fbintl
    collation = WIN1252
    collation = WIN1252_CI
    collation = PXW_INTL
    collation = PXW_NORDAN4
    collation = PXW_SPAN
    collation = PXW_SWEDFIN
}

#=============================================
# ASIAN CHARACTER SETS
#=============================================
# Japanese
charset SJIS_0208
{
    intl_module = fbintl
    collation = SJIS_0208
    collation = SJIS_0208_CI
}

charset EUCJ_0208
{
    intl_module = fbintl
    collation = EUCJ_0208
}

# Chinese
charset GBK
{
    intl_module = fbintl
    collation = GBK
    collation = GBK_CI
}

charset GB18030
{
    intl_module = fbintl
    collation = GB18030
    collation = GB18030_CI
    collation = GB18030_UNICODE
}

# Korean
charset KSC_5601
{
    intl_module = fbintl
    collation = KSC_5601
    collation = KSC_5601_CI
}

#=============================================
# CYRILLIC CHARACTER SETS
#=============================================
charset WIN1251
{
    intl_module = fbintl
    collation = WIN1251
    collation = WIN1251_CI
    collation = PXW_CYRL
}

charset KOI8R
{
    intl_module = fbintl
    collation = KOI8R
    collation = KOI8R_CI
}

charset KOI8U
{
    intl_module = fbintl
    collation = KOI8U
    collation = KOI8U_CI
}

#=============================================
# DOS CODE PAGES
#=============================================
charset DOS437
{
    intl_module = fbintl
    collation = DOS437
    collation = DOS437_CI
    collation = PDOX_ASCII
    collation = PDOX_INTL
}

charset DOS850
{
    intl_module = fbintl
    collation = DOS850
    collation = DOS850_CI
    collation = DB_DEU850
    collation = DB_ESP850
    collation = DB_FRA850
    collation = DB_ITA850
    collation = DB_NLD850
    collation = DB_PTG850
    collation = DB_SVE850
}
```

---

## Configuration Management Best Practices

### Environment-Specific Configurations

#### **Development Environment**
```ini
# Development-friendly settings
RemoteAccess = true
WireCrypt = Enabled             # Optional encryption
StatementTimeout = 0            # No timeout for debugging
DefaultDbCachePages = 1024      # Smaller cache
GCPolicy = cooperative          # Predictable GC
LogFileSize = 1024             # Smaller logs
```

#### **Production Environment**
```ini
# Production security and performance
RemoteAccess = true
WireCrypt = Required           # Mandatory encryption
StatementTimeout = 300         # Prevent runaway queries
DefaultDbCachePages = 16384    # Large cache
GCPolicy = background          # Non-blocking GC
MaxConnections = 100           # Connection limit
LogFileSize = 10240           # Large logs
```

### Configuration Validation

#### **Validation Scripts**
```bash
#!/bin/bash
# Configuration validation script

CONF_DIR="/opt/scratchbird/conf"

# Validate main configuration
echo "Validating scratchbird.conf..."
sb_config_validator "$CONF_DIR/scratchbird.conf"

# Check database aliases
echo "Validating databases.conf..."
sb_database_validator "$CONF_DIR/databases.conf"

# Verify plugin modules
echo "Validating plugins.conf..."
sb_plugin_validator "$CONF_DIR/plugins.conf"

echo "Configuration validation complete."
```

### Security Considerations

#### **File Permissions**
```bash
# Secure configuration file permissions
chmod 640 /opt/scratchbird/conf/*.conf
chown scratchbird:scratchbird /opt/scratchbird/conf/*.conf

# Restrict access to sensitive configs
chmod 600 /opt/scratchbird/conf/replication.conf
chmod 600 /opt/scratchbird/conf/sbtrace.conf
```

#### **Configuration Backup**
```bash
#!/bin/bash
# Configuration backup script

CONF_DIR="/opt/scratchbird/conf"
BACKUP_DIR="/backup/scratchbird/config"
DATE=$(date +%Y%m%d_%H%M%S)

# Create backup directory
mkdir -p "$BACKUP_DIR/$DATE"

# Backup all configuration files
cp "$CONF_DIR"/*.conf "$BACKUP_DIR/$DATE/"

# Create compressed archive
tar -czf "$BACKUP_DIR/config_backup_$DATE.tar.gz" -C "$BACKUP_DIR" "$DATE"

# Remove uncompressed backup
rm -rf "$BACKUP_DIR/$DATE"

echo "Configuration backup completed: config_backup_$DATE.tar.gz"
```

