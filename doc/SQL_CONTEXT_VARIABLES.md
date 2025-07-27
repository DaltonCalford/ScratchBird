# ScratchBird Context Variables - Complete Reference Documentation

## Overview

**Context Variables** in ScratchBird provide access to system information, session state, and user-defined values within SQL statements and stored procedures. ScratchBird's context variable system extends the Firebird foundation with enhanced capabilities for hierarchical schemas, advanced security, and modern application development.

### Context Variable Categories

ScratchBird provides context variables in the following categories:

- **System Context Variables**: Database engine and system information
- **Session Context Variables**: Current session and connection information  
- **User Context Variables**: User-defined variables for application state
- **Transaction Context Variables**: Transaction-specific information
- **Security Context Variables**: Authentication and authorization information
- **Schema Context Variables**: Hierarchical schema navigation and information
- **ScratchBird Extensions**: Advanced context for modern features

### Context Namespaces

ScratchBird organizes context variables into logical namespaces:

| Namespace | Purpose | Scope | Examples |
|-----------|---------|-------|----------|
| **SYSTEM** | System information | Global | ENGINE_VERSION, DB_NAME |
| **USER_SESSION** | User-defined session data | Session | login_time, preferences |
| **USER_TRANSACTION** | User-defined transaction data | Transaction | operation_type, batch_id |
| **SECURITY** | Security and permissions | Session | current_privileges, role_list |
| **SCHEMA** | Schema hierarchy info | Session | current_path, schema_stack |
| **PERFORMANCE** | Performance monitoring | Session | query_count, cache_hits |

---

## System Context Variables

System context variables provide information about the database engine, current database, and system configuration.

### Database and Engine Information

#### Basic System Information
```sql
-- Database engine version
SELECT RDB$GET_CONTEXT('SYSTEM', 'ENGINE_VERSION') ;
-- Result: ScratchBird 0.6.0.1

-- Database name and path
SELECT RDB$GET_CONTEXT('SYSTEM', 'DB_NAME') ;
-- Result: /path/to/database.fdb

-- Server host name
SELECT RDB$GET_CONTEXT('SYSTEM', 'SERVER_NAME') ;
-- Result: scratchbird-server-01

-- Database file size information
SELECT RDB$GET_CONTEXT('SYSTEM', 'DB_SIZE_PAGES') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'PAGE_SIZE') ;

-- Calculate database size in MB
SELECT 
    CAST(RDB$GET_CONTEXT('SYSTEM', 'DB_SIZE_PAGES') AS INTEGER) * 
    CAST(RDB$GET_CONTEXT('SYSTEM', 'PAGE_SIZE') AS INTEGER) / 1048576 
    as database_size_mb
;
```

#### Transaction and Isolation Information
```sql
-- Current isolation level
SELECT RDB$GET_CONTEXT('SYSTEM', 'ISOLATION_LEVEL') ;
-- Result: READ_COMMITTED, SNAPSHOT, READ_UNCOMMITTED, etc.

-- Transaction information
SELECT RDB$GET_CONTEXT('SYSTEM', 'TRANSACTION_ID') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'SNAPSHOT_NUMBER') ;

-- Lock timeout setting
SELECT RDB$GET_CONTEXT('SYSTEM', 'LOCK_TIMEOUT') ;

-- Connection information
SELECT RDB$GET_CONTEXT('SYSTEM', 'CONNECTION_ID') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'SESSION_ID') ;
```

#### Client and Network Information
```sql
-- Client application information
SELECT RDB$GET_CONTEXT('SYSTEM', 'CLIENT_VERSION') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CLIENT_PROCESS') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CLIENT_PID') ;

-- Network connection information
SELECT RDB$GET_CONTEXT('SYSTEM', 'NETWORK_PROTOCOL') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CLIENT_ADDRESS') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CLIENT_HOST') ;

-- Display comprehensive connection information
SELECT 
    RDB$GET_CONTEXT('SYSTEM', 'CLIENT_VERSION') as client_version,
    RDB$GET_CONTEXT('SYSTEM', 'CLIENT_ADDRESS') as client_ip,
    RDB$GET_CONTEXT('SYSTEM', 'NETWORK_PROTOCOL') as protocol,
    RDB$GET_CONTEXT('SYSTEM', 'CONNECTION_ID') as connection_id
;
```

### Configuration and Settings
```sql
-- Database configuration
SELECT RDB$GET_CONTEXT('SYSTEM', 'FORCED_WRITES') ;     -- ON/OFF
SELECT RDB$GET_CONTEXT('SYSTEM', 'SWEEP_INTERVAL') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'BACKUP_STATE') ;      -- Normal, stalled, merge

-- Security settings
SELECT RDB$GET_CONTEXT('SYSTEM', 'WIRE_ENCRYPTION') ;   -- TRUE/FALSE  
SELECT RDB$GET_CONTEXT('SYSTEM', 'WIRE_COMPRESSION') ;  -- TRUE/FALSE

-- Character set information
SELECT RDB$GET_CONTEXT('SYSTEM', 'CLIENT_CHARSET') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'DB_CHARSET') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CONNECTION_CHARSET') ;
```

---

## Session Context Variables

Session context variables track information about the current user session and connection.

### User and Authentication Information
```sql
-- Current user information
SELECT RDB$GET_CONTEXT('SYSTEM', 'CURRENT_USER') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CURRENT_ROLE') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'EFFECTIVE_USER') ;

-- Authentication method and trusted status
SELECT RDB$GET_CONTEXT('SYSTEM', 'AUTH_METHOD') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'TRUSTED_AUTH') ;

-- Session timing information
SELECT RDB$GET_CONTEXT('SYSTEM', 'SESSION_START') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'LAST_QUERY_TIME') ;

-- Calculate session duration
SELECT 
    RDB$GET_CONTEXT('SYSTEM', 'SESSION_START') as session_start,
    CURRENT_TIMESTAMP as current_time,
    DATEDIFF(MINUTE, 
        CAST(RDB$GET_CONTEXT('SYSTEM', 'SESSION_START') AS TIMESTAMP),
        CURRENT_TIMESTAMP
    ) as session_duration_minutes
;
```

### Connection Statistics
```sql
-- Query and statement statistics  
SELECT RDB$GET_CONTEXT('SYSTEM', 'QUERIES_EXECUTED') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'STATEMENTS_PREPARED') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'TRANSACTIONS_STARTED') ;

-- I/O and performance statistics
SELECT RDB$GET_CONTEXT('SYSTEM', 'PAGE_READS') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'PAGE_WRITES') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CACHE_HITS') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CACHE_MISSES') ;

-- Calculate cache hit ratio
SELECT 
    CAST(RDB$GET_CONTEXT('SYSTEM', 'CACHE_HITS') AS INTEGER) as cache_hits,
    CAST(RDB$GET_CONTEXT('SYSTEM', 'CACHE_MISSES') AS INTEGER) as cache_misses,
    CASE 
        WHEN (CAST(RDB$GET_CONTEXT('SYSTEM', 'CACHE_HITS') AS INTEGER) + 
              CAST(RDB$GET_CONTEXT('SYSTEM', 'CACHE_MISSES') AS INTEGER)) > 0
        THEN ROUND(
            CAST(RDB$GET_CONTEXT('SYSTEM', 'CACHE_HITS') AS INTEGER) * 100.0 / 
            (CAST(RDB$GET_CONTEXT('SYSTEM', 'CACHE_HITS') AS INTEGER) + 
             CAST(RDB$GET_CONTEXT('SYSTEM', 'CACHE_MISSES') AS INTEGER)), 2
        )
        ELSE 0
    END as cache_hit_ratio_percent
;
```

---

## User-Defined Context Variables

User-defined context variables allow applications to store custom information for session or transaction scope.

### Session-Scope User Variables

Session-scope variables persist for the entire database connection:

```sql
-- Set session variables
SELECT RDB$SET_CONTEXT('USER_SESSION', 'login_time', CURRENT_TIMESTAMP) ;
SELECT RDB$SET_CONTEXT('USER_SESSION', 'user_department', 'Engineering') ;
SELECT RDB$SET_CONTEXT('USER_SESSION', 'user_preferences', 'theme=dark;lang=en') ;
SELECT RDB$SET_CONTEXT('USER_SESSION', 'last_accessed_module', 'Reports') ;

-- Retrieve session variables
SELECT RDB$GET_CONTEXT('USER_SESSION', 'login_time') ;
SELECT RDB$GET_CONTEXT('USER_SESSION', 'user_department') ;

-- Application initialization
SELECT RDB$SET_CONTEXT('USER_SESSION', 'app_version', '2.1.3') ;
SELECT RDB$SET_CONTEXT('USER_SESSION', 'session_id', GEN_UUID()) ;
SELECT RDB$SET_CONTEXT('USER_SESSION', 'user_timezone', 'America/New_York') ;

-- User preference management
CREATE PROCEDURE set_user_preference(
    preference_name VARCHAR(50),
    preference_value VARCHAR(500)
)
AS
BEGIN
    RDB$SET_CONTEXT('USER_SESSION', 'pref_' || :preference_name, :preference_value);
END;

CREATE FUNCTION get_user_preference(
    preference_name VARCHAR(50)
) RETURNS VARCHAR(500)
AS
BEGIN
    RETURN RDB$GET_CONTEXT('USER_SESSION', 'pref_' || :preference_name);
END;

-- Usage
EXECUTE PROCEDURE set_user_preference('date_format', 'YYYY-MM-DD');
SELECT get_user_preference('date_format') ;
```

### Transaction-Scope User Variables

Transaction-scope variables are cleared when the transaction commits or rolls back:

```sql
-- Set transaction variables for batch operations
SELECT RDB$SET_CONTEXT('USER_TRANSACTION', 'operation_type', 'BULK_INSERT') ;
SELECT RDB$SET_CONTEXT('USER_TRANSACTION', 'batch_id', 'BATCH_2024_001') ;
SELECT RDB$SET_CONTEXT('USER_TRANSACTION', 'record_count', '0') ;

-- Increment counter during processing
UPDATE customers SET last_updated = CURRENT_TIMESTAMP WHERE region = 'WEST';
SELECT RDB$SET_CONTEXT('USER_TRANSACTION', 'record_count', 
    CAST(COALESCE(RDB$GET_CONTEXT('USER_TRANSACTION', 'record_count'), '0') AS INTEGER) + ROW_COUNT
) ;

-- Track transaction progress
CREATE TRIGGER tr_audit_insert_customers
ACTIVE AFTER INSERT ON customers
AS
BEGIN
    -- Track insertions in current transaction
    RDB$SET_CONTEXT('USER_TRANSACTION', 'customers_inserted',
        CAST(COALESCE(RDB$GET_CONTEXT('USER_TRANSACTION', 'customers_inserted'), '0') AS INTEGER) + 1);
END;

-- Retrieve transaction statistics
SELECT 
    RDB$GET_CONTEXT('USER_TRANSACTION', 'operation_type') as operation,
    RDB$GET_CONTEXT('USER_TRANSACTION', 'batch_id') as batch,
    RDB$GET_CONTEXT('USER_TRANSACTION', 'customers_inserted') as customers_added
;
```

### Advanced User Context Applications

#### Multi-Tenant Application Support
```sql
-- Set tenant context for multi-tenant applications
SELECT RDB$SET_CONTEXT('USER_SESSION', 'tenant_id', '12345') ;
SELECT RDB$SET_CONTEXT('USER_SESSION', 'tenant_name', 'Acme Corporation') ;
SELECT RDB$SET_CONTEXT('USER_SESSION', 'tenant_schema', 'tenant_12345') ;

-- Row-level security using context
CREATE TABLE customer_data (
    customer_id INTEGER,
    tenant_id INTEGER,
    customer_name VARCHAR(100),
    email VARCHAR(255)
);

-- View that automatically filters by tenant
CREATE VIEW tenant_customers AS
SELECT customer_id, customer_name, email
FROM customer_data
WHERE tenant_id = CAST(RDB$GET_CONTEXT('USER_SESSION', 'tenant_id') AS INTEGER);

-- Stored procedure with tenant awareness
CREATE PROCEDURE get_customer_count
RETURNS (customer_count INTEGER)
AS
BEGIN
    SELECT COUNT(*)
    FROM customer_data
    WHERE tenant_id = CAST(RDB$GET_CONTEXT('USER_SESSION', 'tenant_id') AS INTEGER)
    INTO :customer_count;
    
    SUSPEND;
END;
```

#### Audit Trail with Context
```sql
-- Set audit context
SELECT RDB$SET_CONTEXT('USER_SESSION', 'audit_source', 'WEB_APPLICATION') ;
SELECT RDB$SET_CONTEXT('USER_SESSION', 'audit_module', 'ORDER_MANAGEMENT') ;

-- Audit trigger using context variables
CREATE TRIGGER tr_orders_audit
ACTIVE AFTER INSERT OR UPDATE OR DELETE ON orders
AS
DECLARE VARIABLE operation_type VARCHAR(10);
DECLARE VARIABLE audit_source VARCHAR(50);
DECLARE VARIABLE audit_module VARCHAR(50);
BEGIN
    operation_type = CASE 
        WHEN INSERTING THEN 'INSERT'
        WHEN UPDATING THEN 'UPDATE'
        WHEN DELETING THEN 'DELETE'
    END;
    
    audit_source = COALESCE(RDB$GET_CONTEXT('USER_SESSION', 'audit_source'), 'UNKNOWN');
    audit_module = COALESCE(RDB$GET_CONTEXT('USER_SESSION', 'audit_module'), 'UNKNOWN');
    
    INSERT INTO audit_log (
        table_name,
        operation_type,
        record_id,
        user_name,
        audit_source,
        audit_module,
        audit_timestamp
    ) VALUES (
        'ORDERS',
        :operation_type,
        COALESCE(NEW.order_id, OLD.order_id),
        CURRENT_USER,
        :audit_source,
        :audit_module,
        CURRENT_TIMESTAMP
    );
END;
```

---

## Schema Context Variables (ScratchBird Enhancement)

ScratchBird's hierarchical schema system provides specialized context variables for schema navigation and management.

### Current Schema Information
```sql
-- Current schema context
SELECT RDB$GET_CONTEXT('SYSTEM', 'CURRENT_SCHEMA') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'HOME_SCHEMA') ;
SELECT RDB$GET_CONTEXT('SYSTEM', 'DEFAULT_SCHEMA') ;

-- Schema hierarchy information
SELECT RDB$GET_CONTEXT('SCHEMA', 'CURRENT_PATH') ;
-- Result: finance.accounting.reports

SELECT RDB$GET_CONTEXT('SCHEMA', 'SCHEMA_DEPTH') ;
-- Result: 3

SELECT RDB$GET_CONTEXT('SCHEMA', 'PARENT_SCHEMA') ;
-- Result: finance.accounting

-- Schema navigation stack
SELECT RDB$GET_CONTEXT('SCHEMA', 'SCHEMA_STACK') ;
-- Result: public,finance,finance.accounting,finance.accounting.reports
```

### Schema History and Navigation
```sql
-- Set schema navigation history
SELECT RDB$SET_CONTEXT('USER_SESSION', 'previous_schema', 
    RDB$GET_CONTEXT('SYSTEM', 'CURRENT_SCHEMA')) ;

-- Schema switching with history
CREATE PROCEDURE switch_to_schema(schema_name VARCHAR(255))
AS
DECLARE VARIABLE current_schema VARCHAR(255);
BEGIN
    -- Save current schema
    current_schema = RDB$GET_CONTEXT('SYSTEM', 'CURRENT_SCHEMA');
    RDB$SET_CONTEXT('USER_SESSION', 'previous_schema', :current_schema);
    
    -- Switch to new schema
    EXECUTE STATEMENT 'SET SCHEMA ' || :schema_name;
    
    -- Log schema change
    RDB$SET_CONTEXT('USER_SESSION', 'schema_change_time', CURRENT_TIMESTAMP);
END;

-- Return to previous schema
CREATE PROCEDURE return_to_previous_schema
AS
DECLARE VARIABLE previous_schema VARCHAR(255);
BEGIN
    previous_schema = RDB$GET_CONTEXT('USER_SESSION', 'previous_schema');
    
    IF (previous_schema IS NOT NULL) THEN
    BEGIN
        EXECUTE STATEMENT 'SET SCHEMA ' || :previous_schema;
        RDB$SET_CONTEXT('USER_SESSION', 'previous_schema', NULL);
    END
END;
```

### Schema Permissions and Access
```sql
-- Check schema access permissions
SELECT RDB$GET_CONTEXT('SCHEMA', 'ACCESSIBLE_SCHEMAS') ;
-- Result: public,finance.accounting,finance.accounting.reports,hr

-- Check specific schema permissions
SELECT RDB$GET_CONTEXT('SCHEMA', 'SCHEMA_PERMISSIONS') ;
-- Result: SELECT:finance.*,INSERT:finance.accounting.reports,UPDATE:finance.accounting.reports

-- Schema access validation
CREATE FUNCTION can_access_schema(schema_name VARCHAR(255))
RETURNS BOOLEAN
AS
DECLARE VARIABLE accessible_schemas VARCHAR(2000);
BEGIN
    accessible_schemas = RDB$GET_CONTEXT('SCHEMA', 'ACCESSIBLE_SCHEMAS');
    
    IF (POSITION(',' || :schema_name || ',' IN ',' || :accessible_schemas || ',') > 0) THEN
        RETURN TRUE;
    ELSE
        RETURN FALSE;
END;

-- Usage
SELECT can_access_schema('finance.accounting.reports') ;
```

---

## Security Context Variables

Security context variables provide information about authentication, authorization, and access control.

### Authentication and Role Information
```sql
-- Extended user and role information
SELECT RDB$GET_CONTEXT('SECURITY', 'ACTIVE_ROLES') ;
-- Result: manager,analyst,reports_viewer

SELECT RDB$GET_CONTEXT('SECURITY', 'GRANTED_ROLES') ;
-- Result: manager,analyst,reports_viewer,developer

SELECT RDB$GET_CONTEXT('SECURITY', 'DEFAULT_ROLE') ;
-- Result: analyst

-- Authentication details
SELECT RDB$GET_CONTEXT('SECURITY', 'AUTH_METHOD') ;
-- Result: NATIVE, WINDOWS, LDAP, etc.

SELECT RDB$GET_CONTEXT('SECURITY', 'AUTH_PLUGIN') ;
-- Result: ScratchBird_UserManager, Win_Sspi, etc.

SELECT RDB$GET_CONTEXT('SECURITY', 'TRUSTED_AUTH') ;
-- Result: TRUE/FALSE
```

### System Privileges
```sql
-- Check system privileges
SELECT RDB$GET_CONTEXT('SECURITY', 'SYSTEM_PRIVILEGES') ;
-- Result: USER_MANAGEMENT,BACKUP_DATABASE,ALTER_DATABASE

-- Individual privilege checking
SELECT RDB$GET_CONTEXT('SECURITY', 'HAS_USER_MANAGEMENT') ;
SELECT RDB$GET_CONTEXT('SECURITY', 'HAS_BACKUP_DATABASE') ;
SELECT RDB$GET_CONTEXT('SECURITY', 'HAS_ALTER_DATABASE') ;

-- Create security checking function
CREATE FUNCTION has_system_privilege(privilege_name VARCHAR(50))
RETURNS BOOLEAN
AS
DECLARE VARIABLE privileges VARCHAR(1000);
BEGIN
    privileges = RDB$GET_CONTEXT('SECURITY', 'SYSTEM_PRIVILEGES');
    
    IF (POSITION(UPPER(:privilege_name) IN UPPER(:privileges)) > 0) THEN
        RETURN TRUE;
    ELSE
        RETURN FALSE;
END;

-- Usage in application logic
IF (has_system_privilege('USER_MANAGEMENT')) THEN
BEGIN
    -- Allow user management operations
    ...
END
```

### Access Control Lists (ACL)
```sql
-- Table and object permissions
SELECT RDB$GET_CONTEXT('SECURITY', 'TABLE_PRIVILEGES') ;
-- Result: customers:SELECT,INSERT,UPDATE;orders:SELECT;products:SELECT

SELECT RDB$GET_CONTEXT('SECURITY', 'PROCEDURE_PRIVILEGES') ;
-- Result: get_customer_orders:EXECUTE;update_inventory:EXECUTE

SELECT RDB$GET_CONTEXT('SECURITY', 'FUNCTION_PRIVILEGES') ;
-- Result: calculate_discount:EXECUTE;format_address:EXECUTE

-- Database link permissions
SELECT RDB$GET_CONTEXT('SECURITY', 'LINK_PRIVILEGES') ;
-- Result: finance_link:CONNECT;reporting_link:CONNECT,QUERY

-- Check specific object access
CREATE FUNCTION can_access_table(table_name VARCHAR(63), access_type VARCHAR(20))
RETURNS BOOLEAN
AS
DECLARE VARIABLE table_privs VARCHAR(2000);
DECLARE VARIABLE search_pattern VARCHAR(100);
BEGIN
    table_privs = RDB$GET_CONTEXT('SECURITY', 'TABLE_PRIVILEGES');
    search_pattern = UPPER(:table_name) || ':' || UPPER(:access_type);
    
    IF (POSITION(:search_pattern IN UPPER(:table_privs)) > 0) THEN
        RETURN TRUE;
    ELSE
        RETURN FALSE;
END;
```

---

## Performance and Monitoring Context Variables

ScratchBird provides context variables for performance monitoring and optimization.

### Query and Statement Statistics
```sql
-- Query execution statistics
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'QUERIES_EXECUTED') ;
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'QUERIES_CACHED') ;
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'QUERY_CACHE_HITS') ;

-- Statement preparation statistics
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'STATEMENTS_PREPARED') ;
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'STATEMENTS_CACHED') ;

-- Calculate query cache efficiency
SELECT 
    CAST(RDB$GET_CONTEXT('PERFORMANCE', 'QUERY_CACHE_HITS') AS INTEGER) as cache_hits,
    CAST(RDB$GET_CONTEXT('PERFORMANCE', 'QUERIES_EXECUTED') AS INTEGER) as total_queries,
    CASE 
        WHEN CAST(RDB$GET_CONTEXT('PERFORMANCE', 'QUERIES_EXECUTED') AS INTEGER) > 0 THEN
            ROUND(
                CAST(RDB$GET_CONTEXT('PERFORMANCE', 'QUERY_CACHE_HITS') AS INTEGER) * 100.0 / 
                CAST(RDB$GET_CONTEXT('PERFORMANCE', 'QUERIES_EXECUTED') AS INTEGER), 2
            )
        ELSE 0
    END as cache_hit_ratio_percent
;
```

### Resource Usage Statistics
```sql
-- Memory usage
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'MEMORY_USED') ;
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'MEMORY_ALLOCATED') ;
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'PAGE_CACHE_SIZE') ;

-- I/O statistics
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'LOGICAL_READS') ;
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'PHYSICAL_READS') ;
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'PAGE_WRITES') ;

-- Lock statistics
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'LOCK_WAITS') ;
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'LOCK_TIMEOUTS') ;
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'DEADLOCKS') ;

-- Network statistics
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'BYTES_SENT') ;
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'BYTES_RECEIVED') ;
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'PACKETS_SENT') ;
SELECT RDB$GET_CONTEXT('PERFORMANCE', 'PACKETS_RECEIVED') ;
```

### Session Performance Monitoring
```sql
-- Create session performance tracking
SELECT RDB$SET_CONTEXT('USER_SESSION', 'session_start_time', CURRENT_TIMESTAMP) ;
SELECT RDB$SET_CONTEXT('USER_SESSION', 'initial_query_count', 
    RDB$GET_CONTEXT('PERFORMANCE', 'QUERIES_EXECUTED')) ;

-- Track query execution
CREATE TRIGGER tr_query_tracking
ACTIVE BEFORE SELECT OR INSERT OR UPDATE OR DELETE ON customers
AS
BEGIN
    -- Increment session query counter
    RDB$SET_CONTEXT('USER_SESSION', 'session_queries',
        CAST(COALESCE(RDB$GET_CONTEXT('USER_SESSION', 'session_queries'), '0') AS INTEGER) + 1);
END;

-- Get session performance summary
CREATE PROCEDURE get_session_performance
RETURNS (
    session_duration_minutes INTEGER,
    queries_executed INTEGER,
    session_queries INTEGER,
    avg_queries_per_minute DECIMAL(10,2)
)
AS
DECLARE VARIABLE start_time TIMESTAMP;
DECLARE VARIABLE current_queries INTEGER;
DECLARE VARIABLE initial_queries INTEGER;
BEGIN
    start_time = CAST(RDB$GET_CONTEXT('USER_SESSION', 'session_start_time') AS TIMESTAMP);
    current_queries = CAST(RDB$GET_CONTEXT('PERFORMANCE', 'QUERIES_EXECUTED') AS INTEGER);
    initial_queries = CAST(RDB$GET_CONTEXT('USER_SESSION', 'initial_query_count') AS INTEGER);
    
    session_duration_minutes = DATEDIFF(MINUTE, :start_time, CURRENT_TIMESTAMP);
    queries_executed = :current_queries - :initial_queries;
    session_queries = CAST(RDB$GET_CONTEXT('USER_SESSION', 'session_queries') AS INTEGER);
    
    IF (:session_duration_minutes > 0) THEN
        avg_queries_per_minute = :queries_executed * 1.0 / :session_duration_minutes;
    ELSE
        avg_queries_per_minute = 0;
    
    SUSPEND;
END;
```

---

## Advanced Context Variable Applications

### Application State Management
```sql
-- Multi-step wizard state management
CREATE PROCEDURE wizard_start(wizard_type VARCHAR(50))
AS
BEGIN
    RDB$SET_CONTEXT('USER_SESSION', 'wizard_active', :wizard_type);
    RDB$SET_CONTEXT('USER_SESSION', 'wizard_step', '1');
    RDB$SET_CONTEXT('USER_SESSION', 'wizard_start_time', CURRENT_TIMESTAMP);
END;

CREATE PROCEDURE wizard_next_step
AS
DECLARE VARIABLE current_step INTEGER;
BEGIN
    current_step = CAST(RDB$GET_CONTEXT('USER_SESSION', 'wizard_step') AS INTEGER);
    RDB$SET_CONTEXT('USER_SESSION', 'wizard_step', :current_step + 1);
END;

CREATE PROCEDURE wizard_complete
AS
BEGIN
    RDB$SET_CONTEXT('USER_SESSION', 'wizard_active', NULL);
    RDB$SET_CONTEXT('USER_SESSION', 'wizard_step', NULL);
    RDB$SET_CONTEXT('USER_SESSION', 'wizard_complete_time', CURRENT_TIMESTAMP);
END;

-- Check wizard state
SELECT 
    RDB$GET_CONTEXT('USER_SESSION', 'wizard_active') as wizard_type,
    RDB$GET_CONTEXT('USER_SESSION', 'wizard_step') as current_step
;
```

### Feature Flag System
```sql
-- Feature flag management using context variables
CREATE PROCEDURE enable_feature(feature_name VARCHAR(100))
AS
BEGIN
    RDB$SET_CONTEXT('USER_SESSION', 'feature_' || :feature_name, 'ENABLED');
END;

CREATE PROCEDURE disable_feature(feature_name VARCHAR(100))
AS
BEGIN
    RDB$SET_CONTEXT('USER_SESSION', 'feature_' || :feature_name, 'DISABLED');
END;

CREATE FUNCTION is_feature_enabled(feature_name VARCHAR(100))
RETURNS BOOLEAN
AS
BEGIN
    RETURN (RDB$GET_CONTEXT('USER_SESSION', 'feature_' || :feature_name) = 'ENABLED');
END;

-- Usage in application logic
IF (is_feature_enabled('NEW_REPORTING_DASHBOARD')) THEN
BEGIN
    -- Show new dashboard
    ...
END
ELSE
BEGIN
    -- Show legacy dashboard
    ...
END
```

### Dynamic Configuration
```sql
-- Dynamic configuration using context variables
CREATE PROCEDURE set_config(config_key VARCHAR(100), config_value VARCHAR(500))
AS
BEGIN
    RDB$SET_CONTEXT('USER_SESSION', 'config_' || :config_key, :config_value);
END;

CREATE FUNCTION get_config(config_key VARCHAR(100), default_value VARCHAR(500))
RETURNS VARCHAR(500)
AS
BEGIN
    RETURN COALESCE(RDB$GET_CONTEXT('USER_SESSION', 'config_' || :config_key), :default_value);
END;

-- Application configuration
EXECUTE PROCEDURE set_config('max_results_per_page', '50');
EXECUTE PROCEDURE set_config('date_format', 'MM/DD/YYYY');
EXECUTE PROCEDURE set_config('currency_symbol', '$');

-- Use configuration in queries
SELECT 
    customer_name,
    order_date,
    order_total || ' ' || get_config('currency_symbol', '$') as formatted_total
FROM orders
ROWS CAST(get_config('max_results_per_page', '25') AS INTEGER);
```

---

## Context Variable Performance and Best Practices

### Performance Considerations
```sql
-- Efficient context variable usage
-- Good: Cache frequently accessed values
CREATE PROCEDURE cache_user_info
AS
DECLARE VARIABLE user_id INTEGER;
DECLARE VARIABLE user_name VARCHAR(100);
DECLARE VARIABLE user_dept VARCHAR(100);
BEGIN
    SELECT user_id, user_name, department
    FROM users 
    WHERE username = CURRENT_USER
    INTO :user_id, :user_name, :user_dept;
    
    -- Cache for session
    RDB$SET_CONTEXT('USER_SESSION', 'cached_user_id', :user_id);
    RDB$SET_CONTEXT('USER_SESSION', 'cached_user_name', :user_name);
    RDB$SET_CONTEXT('USER_SESSION', 'cached_user_dept', :user_dept);
END;

-- Use cached values instead of repeated queries
SELECT 
    order_id,
    order_date,
    RDB$GET_CONTEXT('USER_SESSION', 'cached_user_name') as order_processor
FROM orders;

-- Efficient batch processing with context
CREATE PROCEDURE process_batch(batch_type VARCHAR(50))
AS
DECLARE VARIABLE batch_id VARCHAR(100);
DECLARE VARIABLE start_time TIMESTAMP;
DECLARE VARIABLE record_count INTEGER = 0;
BEGIN
    batch_id = 'BATCH_' || :batch_type || '_' || CURRENT_TIMESTAMP;
    start_time = CURRENT_TIMESTAMP;
    
    -- Set batch context
    RDB$SET_CONTEXT('USER_TRANSACTION', 'batch_id', :batch_id);
    RDB$SET_CONTEXT('USER_TRANSACTION', 'batch_type', :batch_type);
    RDB$SET_CONTEXT('USER_TRANSACTION', 'batch_start', :start_time);
    
    -- Process records...
    FOR SELECT customer_id FROM customers WHERE needs_processing = 'Y'
        INTO customer_id
    DO
    BEGIN
        -- Process customer
        UPDATE customers SET last_processed = CURRENT_TIMESTAMP 
        WHERE customer_id = :customer_id;
        
        record_count = record_count + 1;
        
        -- Update progress periodically
        IF (MOD(record_count, 100) = 0) THEN
            RDB$SET_CONTEXT('USER_TRANSACTION', 'records_processed', :record_count);
    END
    
    -- Final batch statistics
    RDB$SET_CONTEXT('USER_TRANSACTION', 'batch_end', CURRENT_TIMESTAMP);
    RDB$SET_CONTEXT('USER_TRANSACTION', 'total_records', :record_count);
END;
```

### Security Best Practices
```sql
-- Secure context variable usage
-- Don't store sensitive information in context variables
-- Bad: 
-- RDB$SET_CONTEXT('USER_SESSION', 'password', 'secret123');

-- Good: Store non-sensitive identifiers
RDB$SET_CONTEXT('USER_SESSION', 'security_token_hash', hash_value);

-- Validate context before use
CREATE FUNCTION get_secure_context(namespace VARCHAR(80), variable VARCHAR(80))
RETURNS VARCHAR(255)
AS
DECLARE VARIABLE result VARCHAR(255);
BEGIN
    -- Only allow access to certain namespaces
    IF (:namespace NOT IN ('USER_SESSION', 'USER_TRANSACTION', 'PERFORMANCE')) THEN
        EXCEPTION invalid_namespace;
    
    result = RDB$GET_CONTEXT(:namespace, :variable);
    
    -- Log context access for auditing
    INSERT INTO context_access_log (
        username, namespace, variable_name, access_time
    ) VALUES (
        CURRENT_USER, :namespace, :variable, CURRENT_TIMESTAMP
    );
    
    RETURN :result;
END;
```

### Context Variable Cleanup
```sql
-- Clean up session context variables
CREATE PROCEDURE cleanup_session_context
AS
BEGIN
    -- Clear temporary session variables
    RDB$SET_CONTEXT('USER_SESSION', 'temp_calculation', NULL);
    RDB$SET_CONTEXT('USER_SESSION', 'wizard_active', NULL);
    RDB$SET_CONTEXT('USER_SESSION', 'wizard_step', NULL);
    RDB$SET_CONTEXT('USER_SESSION', 'batch_progress', NULL);
END;

-- Automatic cleanup on session end
CREATE TRIGGER tr_session_cleanup
ACTIVE ON DISCONNECT
AS
BEGIN
    -- Clean up any session-specific data
    DELETE FROM user_session_data 
    WHERE session_id = RDB$GET_CONTEXT('SYSTEM', 'SESSION_ID');
END;
```

---

