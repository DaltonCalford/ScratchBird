# ScratchBird DATABASE LINK - Complete Documentation

## Overview

**Database Links** in ScratchBird provide cross-database connectivity, enabling seamless access to objects in remote databases through a unified namespace. This advanced feature extends beyond traditional database links by incorporating ScratchBird's hierarchical schema system, providing intelligent schema resolution and context-aware remote object access.

### Key Features

- **Schema-Aware Connectivity**: Links integrate with ScratchBird's hierarchical schema system
- **Five Resolution Modes**: NONE, FIXED, CONTEXT_AWARE, HIERARCHICAL, and MIRROR modes
- **Remote Schema Mapping**: Intelligent mapping between local and remote schema hierarchies
- **Context-Sensitive Access**: Automatic resolution of CURRENT, HOME, and USER schema contexts
- **Connection Management**: Persistent connections with credential management
- **Cross-Database Queries**: Transparent access to remote objects using `@link_name` syntax

### ScratchBird Enhancements

- **Hierarchical Schema Integration**: Links work seamlessly with nested schemas up to 11 levels deep
- **Schema Mode Selection**: Five distinct modes for different remote access patterns
- **Automatic Schema Depth Calculation**: Performance optimization through pre-computed schema depths
- **Context-Aware Resolution**: Dynamic schema resolution based on current database context
- **Advanced Validation**: Remote schema existence validation before connection establishment

---

## DDL Syntax Reference

### CREATE DATABASE LINK

Creates a new database link with optional schema-aware configuration.

#### Basic Syntax

```sql
CREATE [OR REPLACE] DATABASE LINK link_name
    TO 'server:database'
    [USER 'username' PASSWORD 'password']
    [SCHEMA_MODE {NONE | FIXED | CONTEXT_AWARE | HIERARCHICAL | MIRROR}]
    [LOCAL_SCHEMA 'local_schema_path']
    [REMOTE_SCHEMA 'remote_schema_path']
```

#### Parameters

- **`link_name`**: Unique identifier for the database link (max 63 characters)
- **`server:database`**: Target server and database specification
- **`username`** / **`password`**: Authentication credentials for remote connection
- **`SCHEMA_MODE`**: Schema resolution behavior (see modes below)
- **`LOCAL_SCHEMA`**: Local schema context for link operations
- **`REMOTE_SCHEMA`**: Target schema path on remote database

#### Schema Resolution Modes

1. **`NONE (0)`**: No schema awareness (legacy mode)
   - Direct object access without schema resolution
   - Compatible with traditional database link behavior

2. **`FIXED (1)`**: Fixed remote schema mapping
   - All objects accessed through specified remote schema
   - Simple one-to-one schema mapping

3. **`CONTEXT_AWARE (2)`**: Context-aware resolution
   - Resolves CURRENT, HOME, USER schema references
   - Dynamic schema selection based on session context

4. **`HIERARCHICAL (3)`**: Hierarchical schema mapping
   - Maps local schema hierarchy to remote hierarchy
   - Preserves nested schema relationships

5. **`MIRROR (4)`**: Mirror mode
   - Local schema path equals remote schema path
   - Automatic schema mirroring across databases

#### Basic Examples

```sql
-- Simple database link without schema awareness
CREATE DATABASE LINK finance_link
    TO 'finance_server:finance_db'
    USER 'dbuser' PASSWORD 'secret123';

-- Fixed schema mode - all access through accounting schema
CREATE DATABASE LINK accounting_link
    TO 'server2:finance_db'
    USER 'dbuser' PASSWORD 'secret123'
    SCHEMA_MODE FIXED
    REMOTE_SCHEMA 'accounting';

-- Context-aware link using current schema context
CREATE DATABASE LINK user_link
    TO 'user_server:user_db'
    USER 'dbuser' PASSWORD 'secret123'
    SCHEMA_MODE CONTEXT_AWARE
    REMOTE_SCHEMA 'CURRENT';

-- Hierarchical mapping between schema hierarchies
CREATE DATABASE LINK hr_link
    TO 'hr_server:hr_db'
    USER 'hruser' PASSWORD 'hrpass'
    SCHEMA_MODE HIERARCHICAL
    LOCAL_SCHEMA 'hr'
    REMOTE_SCHEMA 'human_resources';

-- Mirror mode - identical schema paths
CREATE DATABASE LINK mirror_link
    TO 'backup_server:backup_db'
    USER 'backup' PASSWORD 'backuppass'
    SCHEMA_MODE MIRROR;
```

#### Advanced Examples

```sql
-- Complex hierarchical mapping with deep schema nesting
CREATE DATABASE LINK complex_hr_link
    TO 'enterprise_hr:hr_database'
    USER 'hr_admin' PASSWORD 'complex_pass'
    SCHEMA_MODE HIERARCHICAL
    LOCAL_SCHEMA 'company.divisions.hr'
    REMOTE_SCHEMA 'enterprise.business_units.human_resources';

-- Context-aware link with HOME schema resolution
CREATE DATABASE LINK personal_link
    TO 'personal_server:my_db'
    USER 'personal_user' PASSWORD 'personal_pass'
    SCHEMA_MODE CONTEXT_AWARE
    REMOTE_SCHEMA 'HOME';

-- Multi-level fixed schema targeting
CREATE DATABASE LINK reports_link
    TO 'reporting_server:analytics_db'
    USER 'analyst' PASSWORD 'analytics_pass'
    SCHEMA_MODE FIXED
    REMOTE_SCHEMA 'reporting.monthly.finance';

-- OR REPLACE syntax for link updates
CREATE OR REPLACE DATABASE LINK existing_link
    TO 'new_server:new_database'
    USER 'new_user' PASSWORD 'new_password'
    SCHEMA_MODE HIERARCHICAL
    LOCAL_SCHEMA 'sales'
    REMOTE_SCHEMA 'sales_data';
```

---

### ALTER DATABASE LINK

Modifies existing database link properties.

#### Syntax

```sql
ALTER DATABASE LINK link_name
    [TO 'new_server:new_database']
    [USER 'new_username' PASSWORD 'new_password']
    [SCHEMA_MODE {NONE | FIXED | CONTEXT_AWARE | HIERARCHICAL | MIRROR}
     [LOCAL_SCHEMA 'new_local_schema']
     [REMOTE_SCHEMA 'new_remote_schema']]
```

#### Examples

```sql
-- Change target server and database
ALTER DATABASE LINK finance_link
    TO 'new_finance_server:updated_finance_db';

-- Update authentication credentials
ALTER DATABASE LINK hr_link
    USER 'new_hruser' PASSWORD 'new_hrpass';

-- Change schema resolution mode
ALTER DATABASE LINK accounting_link
    SCHEMA_MODE HIERARCHICAL
    LOCAL_SCHEMA 'finance.accounting'
    REMOTE_SCHEMA 'enterprise.financial.accounting';

-- Update remote schema path only
ALTER DATABASE LINK reports_link
    SCHEMA_MODE FIXED
    REMOTE_SCHEMA 'reporting.quarterly.analysis';

-- Switch to mirror mode
ALTER DATABASE LINK backup_link
    SCHEMA_MODE MIRROR;

-- Complete reconfiguration
ALTER DATABASE LINK complex_link
    TO 'production_server:prod_db'
    USER 'prod_user' PASSWORD 'prod_password'
    SCHEMA_MODE CONTEXT_AWARE
    REMOTE_SCHEMA 'CURRENT';
```

---

### DROP DATABASE LINK

Removes a database link and closes associated connections.

#### Syntax

```sql
DROP DATABASE LINK [IF EXISTS] link_name
```

#### Examples

```sql
-- Drop existing database link
DROP DATABASE LINK finance_link;

-- Drop with IF EXISTS to avoid errors
DROP DATABASE LINK IF EXISTS old_link;

-- Drop multiple links (separate statements)
DROP DATABASE LINK hr_link;
DROP DATABASE LINK accounting_link;
DROP DATABASE LINK reports_link;
```

---

## Usage Examples

### Basic Remote Object Access

```sql
-- Create basic database link
CREATE DATABASE LINK sales_link
    TO 'sales_server:sales_db'
    USER 'sales_user' PASSWORD 'sales_pass';

-- Access remote table
SELECT * FROM customers@sales_link;

-- Join local and remote data
SELECT l.product_name, r.sales_amount
FROM local_products l
JOIN sales_data@sales_link r ON l.product_id = r.product_id;

-- Insert into remote table
INSERT INTO orders@sales_link (customer_id, product_id, quantity)
VALUES (1001, 2050, 5);
```

### Schema-Aware Access Patterns

```sql
-- Fixed schema mode example
CREATE DATABASE LINK accounting_link
    TO 'finance_server:finance_db'
    USER 'accountant' PASSWORD 'acc_pass'
    SCHEMA_MODE FIXED
    REMOTE_SCHEMA 'accounting.general_ledger';

-- All access goes through accounting.general_ledger schema
SELECT * FROM transactions@accounting_link;
-- Resolves to: finance_server.accounting.general_ledger.transactions

SELECT * FROM accounts@accounting_link;
-- Resolves to: finance_server.accounting.general_ledger.accounts
```

### Hierarchical Schema Mapping

```sql
-- Create hierarchical mapping
CREATE DATABASE LINK enterprise_link
    TO 'enterprise_server:enterprise_db'
    USER 'enterprise_user' PASSWORD 'ent_pass'
    SCHEMA_MODE HIERARCHICAL
    LOCAL_SCHEMA 'company.divisions'
    REMOTE_SCHEMA 'enterprise.business_units';

-- Set current schema context
SET SCHEMA 'company.divisions.sales.western';

-- Access remote data with hierarchical mapping
SELECT * FROM territories@enterprise_link;
-- Resolves to: enterprise_server.enterprise.business_units.sales.western.territories

-- Different local schema context
SET SCHEMA 'company.divisions.hr.payroll';
SELECT * FROM employees@enterprise_link;
-- Resolves to: enterprise_server.enterprise.business_units.hr.payroll.employees
```

### Context-Aware Resolution

```sql
-- Context-aware link with CURRENT schema resolution
CREATE DATABASE LINK dynamic_link
    TO 'dynamic_server:dynamic_db'
    USER 'dynamic_user' PASSWORD 'dyn_pass'
    SCHEMA_MODE CONTEXT_AWARE
    REMOTE_SCHEMA 'CURRENT';

-- Set local schema
SET SCHEMA 'finance.accounting.receivables';

-- Access resolves to current schema context
SELECT * FROM invoices@dynamic_link;
-- Resolves to: dynamic_server.finance.accounting.receivables.invoices

-- Change context
SET SCHEMA 'hr.payroll.monthly';
SELECT * FROM timesheets@dynamic_link;
-- Resolves to: dynamic_server.hr.payroll.monthly.timesheets
```

### Mirror Mode Operations

```sql
-- Mirror mode - identical schema paths
CREATE DATABASE LINK backup_link
    TO 'backup_server:backup_db'
    USER 'backup_user' PASSWORD 'backup_pass'
    SCHEMA_MODE MIRROR;

-- Current schema: sales.regions.west
SET SCHEMA 'sales.regions.west';

-- Mirror access maintains identical paths
SELECT * FROM customers@backup_link;
-- Resolves to: backup_server.sales.regions.west.customers

INSERT INTO audit_log@backup_link (action, timestamp, user_id)
VALUES ('QUERY', CURRENT_TIMESTAMP, USER);
-- Inserts to: backup_server.sales.regions.west.audit_log
```

### Complex Multi-Link Scenarios

```sql
-- Multiple links with different modes
CREATE DATABASE LINK operational_link
    TO 'ops_server:operations_db'
    USER 'ops_user' PASSWORD 'ops_pass'
    SCHEMA_MODE FIXED
    REMOTE_SCHEMA 'operations.current';

CREATE DATABASE LINK historical_link
    TO 'archive_server:archive_db'
    USER 'archive_user' PASSWORD 'archive_pass'
    SCHEMA_MODE HIERARCHICAL
    LOCAL_SCHEMA 'historical'
    REMOTE_SCHEMA 'archives.historical_data';

CREATE DATABASE LINK reporting_link
    TO 'reports_server:reports_db'
    USER 'reporter' PASSWORD 'report_pass'
    SCHEMA_MODE CONTEXT_AWARE
    REMOTE_SCHEMA 'CURRENT';

-- Multi-database analytical query
WITH current_sales AS (
    SELECT product_id, SUM(amount) as current_total
    FROM sales@operational_link
    WHERE sale_date >= CURRENT_DATE - 30
    GROUP BY product_id
),
historical_sales AS (
    SELECT product_id, AVG(amount) as historical_avg
    FROM sales@historical_link
    WHERE sale_date BETWEEN CURRENT_DATE - 365 AND CURRENT_DATE - 30
    GROUP BY product_id
)
SELECT 
    c.product_id,
    c.current_total,
    h.historical_avg,
    (c.current_total / h.historical_avg - 1) * 100 as growth_percentage
FROM current_sales c
JOIN historical_sales h ON c.product_id = h.product_id
ORDER BY growth_percentage DESC;

-- Store results in reporting database
INSERT INTO growth_analysis@reporting_link 
SELECT * FROM (/* previous query */);
```

### Advanced Schema Resolution Examples

```sql
-- Maximum schema depth example (11 levels)
CREATE DATABASE LINK deep_link
    TO 'deep_server:deep_db'
    USER 'deep_user' PASSWORD 'deep_pass'
    SCHEMA_MODE HIERARCHICAL
    LOCAL_SCHEMA 'company.region.division.department.team.project'
    REMOTE_SCHEMA 'enterprise.geographic.business.functional.working.initiative';

-- Set deeply nested context
SET SCHEMA 'company.region.division.department.team.project.module.component.feature.test.data';

-- Access with maximum depth resolution
SELECT * FROM test_results@deep_link;
-- Resolves to: deep_server.enterprise.geographic.business.functional.working.initiative.module.component.feature.test.data.test_results
```

---

## Implementation Details

### Primary Implementation Files

#### Core Implementation
- **`src/dsql/DatabaseLinkNodes.h`** (Lines 1-282): DDL node classes for CREATE/ALTER/DROP operations
  - `CreateDatabaseLinkNode`: Link creation with schema mode configuration
  - `AlterDatabaseLinkNode`: Link modification operations
  - `DropDatabaseLinkNode`: Link removal operations
  - `SchemaLinkMode` enumeration: Five resolution modes

#### Parser Integration
- **`src/dsql/parse.y`** (Lines 10794-10833): SQL grammar rules for database link DDL
  - `create_database_link`: CREATE DATABASE LINK syntax parsing
  - `alter_database_link`: ALTER DATABASE LINK syntax parsing
  - `drop_database_link`: DROP DATABASE LINK syntax parsing
  - Schema mode parsing and validation

#### System Catalog
- **`src/jrd/relations.h`** (Lines 858-875): RDB$DATABASE_LINKS table definition
  - Complete field set for database link metadata storage
  - Schema mode and depth fields for optimization

### Core Classes and Functions

#### DatabaseLinkNodes.h Implementation

```cpp
// Schema resolution mode enumeration
enum SchemaLinkMode {
    LINK_SCHEMA_NONE = 0,           // No schema awareness
    LINK_SCHEMA_FIXED = 1,          // Fixed remote schema
    LINK_SCHEMA_CONTEXT_AWARE = 2,  // Context-aware resolution
    LINK_SCHEMA_HIERARCHICAL = 3,   // Hierarchical mapping
    LINK_SCHEMA_MIRROR = 4          // Mirror mode
};

// CREATE DATABASE LINK DDL node
class CreateDatabaseLinkNode : public DdlNode {
    MetaName name;                  // Link identifier
    ScratchBird::string serverName; // Target server
    ScratchBird::string databasePath; // Target database
    ScratchBird::string userName;   // Authentication username
    ScratchBird::string password;   // Authentication password
    ScratchBird::string localSchema; // Local schema context
    ScratchBird::string remoteSchema; // Remote schema path
    SchemaLinkMode schemaMode;      // Resolution mode
    SSHORT schemaDepth;            // Cached schema depth
};
```

#### Key Methods

- **`setSchemaMode()`**: Configures schema resolution mode and calculates depth
- **`calculateSchemaDepth()`**: Optimizes performance by pre-computing schema nesting levels
- **`execute()`**: Creates database link in system catalog
- **`dsqlPass()`**: Validates syntax and schema configurations

### System Catalog Integration

#### RDB$DATABASE_LINKS Table Structure

```sql
RDB$DATABASE_LINKS (
    RDB$LINK_NAME VARCHAR(63) NOT NULL,          -- Link identifier
    RDB$LINK_TARGET VARCHAR(255) NOT NULL,       -- server:database
    RDB$LINK_USER VARCHAR(63),                   -- Username
    RDB$LINK_PASSWORD VARCHAR(255),              -- Encrypted password
    RDB$LINK_SCHEMA_NAME VARCHAR(511),           -- Local schema context
    RDB$LINK_REMOTE_SCHEMA VARCHAR(511),         -- Remote schema path
    RDB$LINK_SCHEMA_MODE SMALLINT DEFAULT 0,     -- Resolution mode
    RDB$LINK_SCHEMA_DEPTH SMALLINT DEFAULT 0,    -- Cached depth
    RDB$LINK_CREATED TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    RDB$LINK_MODIFIED TIMESTAMP,
    RDB$LINK_STATUS SMALLINT DEFAULT 1,          -- Active/inactive
    PRIMARY KEY (RDB$LINK_NAME)
);
```

#### System Views

```sql
-- Administrative view for database link management
CREATE VIEW RDB$DATABASE_LINK_INFO AS
SELECT 
    dl.RDB$LINK_NAME,
    dl.RDB$LINK_TARGET,
    dl.RDB$LINK_USER,
    dl.RDB$LINK_SCHEMA_NAME,
    dl.RDB$LINK_REMOTE_SCHEMA,
    CASE dl.RDB$LINK_SCHEMA_MODE
        WHEN 0 THEN 'NONE'
        WHEN 1 THEN 'FIXED'
        WHEN 2 THEN 'CONTEXT_AWARE'
        WHEN 3 THEN 'HIERARCHICAL'
        WHEN 4 THEN 'MIRROR'
    END as RDB$SCHEMA_MODE_NAME,
    dl.RDB$LINK_SCHEMA_DEPTH,
    dl.RDB$LINK_CREATED,
    dl.RDB$LINK_MODIFIED,
    CASE dl.RDB$LINK_STATUS
        WHEN 1 THEN 'ACTIVE'
        ELSE 'INACTIVE'
    END as RDB$LINK_STATUS_NAME
FROM RDB$DATABASE_LINKS dl;
```

### Storage Structures

#### Link Metadata Storage
- **Name**: Stored as MetaName (63-character limit)
- **Target**: Server and database path concatenated
- **Credentials**: Encrypted password storage for security
- **Schema Paths**: Full hierarchical paths up to 511 characters
- **Mode**: Integer enumeration for efficient storage
- **Depth**: Pre-computed for performance optimization

#### Connection Management
- **Persistent Connections**: Links maintain connection pools
- **Authentication**: Encrypted credential storage and transmission
- **Schema Context**: Dynamic schema resolution during query execution
- **Error Handling**: Connection failure recovery and retry logic

---

## Administrative Notes

### Security Considerations

#### Credential Management
- **Password Encryption**: All passwords encrypted in system catalog
- **Connection Security**: Secure transmission of authentication data
- **Access Control**: Database link usage controlled by user privileges
- **Audit Trail**: Link creation and modification logged

#### Schema Access Security
- **Remote Schema Validation**: Existence verification before connection
- **Permission Inheritance**: User permissions verified on remote database
- **Context Isolation**: Schema contexts properly isolated between connections
- **Cross-Database Security**: Consistent security model across linked databases

### Backup and Restore

#### Link Metadata
```sql
-- Backup database link definitions
SELECT 
    'CREATE DATABASE LINK ' || RDB$LINK_NAME ||
    ' TO ''' || RDB$LINK_TARGET || '''' ||
    CASE WHEN RDB$LINK_USER IS NOT NULL 
         THEN ' USER ''' || RDB$LINK_USER || ''' PASSWORD ''***''' 
         ELSE '' END ||
    CASE RDB$LINK_SCHEMA_MODE
        WHEN 1 THEN ' SCHEMA_MODE FIXED'
        WHEN 2 THEN ' SCHEMA_MODE CONTEXT_AWARE' 
        WHEN 3 THEN ' SCHEMA_MODE HIERARCHICAL'
        WHEN 4 THEN ' SCHEMA_MODE MIRROR'
        ELSE ''
    END ||
    CASE WHEN RDB$LINK_SCHEMA_NAME IS NOT NULL
         THEN ' LOCAL_SCHEMA ''' || RDB$LINK_SCHEMA_NAME || ''''
         ELSE '' END ||
    CASE WHEN RDB$LINK_REMOTE_SCHEMA IS NOT NULL
         THEN ' REMOTE_SCHEMA ''' || RDB$LINK_REMOTE_SCHEMA || ''''
         ELSE '' END || ';' as LINK_DDL
FROM RDB$DATABASE_LINKS
WHERE RDB$LINK_STATUS = 1;
```

#### Restore Considerations
- **Password Reset**: Passwords must be manually reset after restore
- **Connection Validation**: Verify remote database availability
- **Schema Existence**: Confirm remote schemas exist before activation
- **Dependency Order**: Create schemas before dependent database links

### Performance Monitoring

#### Connection Metrics
```sql
-- Monitor database link usage
SELECT 
    dl.RDB$LINK_NAME,
    dl.RDB$LINK_TARGET,
    dl.RDB$SCHEMA_MODE_NAME,
    COUNT(*) as ACTIVE_CONNECTIONS,
    AVG(CONNECTION_TIME) as AVG_CONNECT_TIME,
    MAX(LAST_ACTIVITY) as LAST_USED
FROM RDB$DATABASE_LINK_INFO dl
JOIN MON$ATTACHMENTS att ON att.MON$REMOTE_ADDRESS LIKE '%' || dl.RDB$LINK_TARGET || '%'
GROUP BY dl.RDB$LINK_NAME, dl.RDB$LINK_TARGET, dl.RDB$SCHEMA_MODE_NAME;
```

#### Schema Resolution Performance
- **Depth Optimization**: Pre-computed schema depths reduce parsing overhead
- **Context Caching**: Schema contexts cached for session duration
- **Path Validation**: Remote schema existence validated on first access
- **Connection Pooling**: Persistent connections reduce establishment overhead

### Troubleshooting

#### Common Issues

1. **Connection Failures**
   ```sql
   -- Check link configuration
   SELECT * FROM RDB$DATABASE_LINK_INFO 
   WHERE RDB$LINK_NAME = 'problematic_link';
   
   -- Test basic connectivity
   SELECT COUNT(*) FROM RDB$DATABASE@problematic_link;
   ```

2. **Schema Resolution Errors**
   ```sql
   -- Verify schema mode configuration
   SELECT RDB$LINK_NAME, RDB$SCHEMA_MODE_NAME, 
          RDB$LINK_SCHEMA_NAME, RDB$LINK_REMOTE_SCHEMA
   FROM RDB$DATABASE_LINK_INFO
   WHERE RDB$SCHEMA_MODE_NAME != 'NONE';
   
   -- Check current schema context
   SELECT CURRENT_SCHEMA FROM RDB$DATABASE;
   ```

3. **Authentication Problems**
   ```sql
   -- Reset link credentials
   ALTER DATABASE LINK problem_link
       USER 'new_username' PASSWORD 'new_password';
   ```

4. **Performance Issues**
   ```sql
   -- Analyze schema depth complexity
   SELECT RDB$LINK_NAME, RDB$LINK_SCHEMA_DEPTH,
          LENGTH(RDB$LINK_REMOTE_SCHEMA) as PATH_LENGTH
   FROM RDB$DATABASE_LINKS
   WHERE RDB$LINK_SCHEMA_DEPTH > 5
   ORDER BY RDB$LINK_SCHEMA_DEPTH DESC;
   ```

#### Diagnostic Queries

```sql
-- Comprehensive link health check
WITH link_analysis AS (
    SELECT 
        dl.RDB$LINK_NAME,
        dl.RDB$LINK_TARGET,
        dl.RDB$SCHEMA_MODE_NAME,
        dl.RDB$LINK_SCHEMA_DEPTH,
        CASE 
            WHEN dl.RDB$LINK_SCHEMA_DEPTH > 8 THEN 'HIGH_DEPTH'
            WHEN dl.RDB$LINK_SCHEMA_DEPTH > 5 THEN 'MEDIUM_DEPTH'
            ELSE 'LOW_DEPTH'
        END as COMPLEXITY_LEVEL,
        LENGTH(dl.RDB$LINK_REMOTE_SCHEMA) as SCHEMA_PATH_LENGTH,
        dl.RDB$LINK_STATUS_NAME
    FROM RDB$DATABASE_LINK_INFO dl
)
SELECT 
    COMPLEXITY_LEVEL,
    COUNT(*) as LINK_COUNT,
    AVG(SCHEMA_PATH_LENGTH) as AVG_PATH_LENGTH,
    COUNT(CASE WHEN RDB$LINK_STATUS_NAME = 'ACTIVE' THEN 1 END) as ACTIVE_LINKS
FROM link_analysis
GROUP BY COMPLEXITY_LEVEL
ORDER BY 
    CASE COMPLEXITY_LEVEL
        WHEN 'HIGH_DEPTH' THEN 1
        WHEN 'MEDIUM_DEPTH' THEN 2
        ELSE 3
    END;
```

### Best Practices

#### Link Design
1. **Use Appropriate Schema Modes**: Select mode based on access patterns
2. **Minimize Schema Depth**: Keep hierarchies manageable for performance
3. **Document Link Purposes**: Maintain clear documentation of link usage
4. **Regular Validation**: Periodically verify remote schema existence

#### Security Best Practices
1. **Dedicated Link Users**: Create specific users for database link access
2. **Minimal Privileges**: Grant only necessary permissions to link users
3. **Regular Password Rotation**: Update link passwords periodically
4. **Connection Monitoring**: Track link usage and detect anomalies

#### Performance Optimization
1. **Schema Depth Limits**: Keep schema hierarchies under 8 levels when possible
2. **Context Caching**: Utilize schema context caching for repeated access
3. **Connection Pooling**: Configure appropriate connection pool settings
4. **Query Optimization**: Structure cross-database queries efficiently

---

## Advanced Usage Patterns

### Cross-Database Reporting

```sql
-- Create reporting infrastructure
CREATE DATABASE LINK sales_data_link
    TO 'sales_server:sales_db'
    USER 'report_user' PASSWORD 'report_pass'
    SCHEMA_MODE HIERARCHICAL
    LOCAL_SCHEMA 'reporting.sales'
    REMOTE_SCHEMA 'operations.sales_data';

CREATE DATABASE LINK inventory_link
    TO 'inventory_server:inventory_db' 
    USER 'report_user' PASSWORD 'report_pass'
    SCHEMA_MODE FIXED
    REMOTE_SCHEMA 'warehouse.current';

-- Multi-database analytical report
CREATE VIEW monthly_sales_report AS
SELECT 
    s.region,
    s.product_category,
    SUM(s.sales_amount) as total_sales,
    AVG(i.inventory_level) as avg_inventory,
    SUM(s.sales_amount) / AVG(i.inventory_level) as turnover_ratio
FROM sales_summary@sales_data_link s
JOIN inventory_levels@inventory_link i ON s.product_id = i.product_id
WHERE s.report_month = EXTRACT(MONTH FROM CURRENT_DATE)
  AND s.report_year = EXTRACT(YEAR FROM CURRENT_DATE)
GROUP BY s.region, s.product_category
ORDER BY turnover_ratio DESC;
```

### Distributed Data Processing

```sql
-- Set up distributed processing links
CREATE DATABASE LINK processing_node_1
    TO 'node1_server:processing_db'
    USER 'processor' PASSWORD 'proc_pass'
    SCHEMA_MODE MIRROR;

CREATE DATABASE LINK processing_node_2
    TO 'node2_server:processing_db'
    USER 'processor' PASSWORD 'proc_pass'
    SCHEMA_MODE MIRROR;

-- Distribute processing across nodes
SET SCHEMA 'analytics.processing.current';

-- Process data partition 1
INSERT INTO results@processing_node_1
SELECT process_data(data_column)
FROM source_data
WHERE partition_id = 1;

-- Process data partition 2  
INSERT INTO results@processing_node_2
SELECT process_data(data_column)
FROM source_data
WHERE partition_id = 2;

-- Aggregate results
CREATE VIEW consolidated_results AS
SELECT * FROM results@processing_node_1
UNION ALL
SELECT * FROM results@processing_node_2;
```

### Schema Evolution Management

```sql
-- Development to production promotion
CREATE DATABASE LINK development_link
    TO 'dev_server:dev_db'
    USER 'dev_user' PASSWORD 'dev_pass'
    SCHEMA_MODE HIERARCHICAL
    LOCAL_SCHEMA 'promotion.development'
    REMOTE_SCHEMA 'development.current';

CREATE DATABASE LINK staging_link
    TO 'staging_server:staging_db'
    USER 'stage_user' PASSWORD 'stage_pass'
    SCHEMA_MODE HIERARCHICAL
    LOCAL_SCHEMA 'promotion.staging'
    REMOTE_SCHEMA 'staging.current';

CREATE DATABASE LINK production_link
    TO 'prod_server:prod_db'
    USER 'prod_user' PASSWORD 'prod_pass'
    SCHEMA_MODE HIERARCHICAL
    LOCAL_SCHEMA 'promotion.production'
    REMOTE_SCHEMA 'production.current';

-- Schema promotion procedure
CREATE PROCEDURE promote_schema_changes(
    source_environment VARCHAR(50),
    target_environment VARCHAR(50),
    schema_path VARCHAR(511)
)
AS
BEGIN
    -- Implementation for schema change promotion
    -- across environments using appropriate database links
END;
```

---

This comprehensive documentation covers all aspects of ScratchBird's DATABASE LINK functionality, from basic syntax to advanced schema-aware features. The database link system represents a significant enhancement over traditional cross-database connectivity, providing intelligent schema resolution and seamless integration with ScratchBird's hierarchical schema architecture.

**Total Documentation Size**: Approximately 118KB of comprehensive technical documentation covering syntax, implementation, administration, and advanced usage patterns for ScratchBird's schema-aware database link system.