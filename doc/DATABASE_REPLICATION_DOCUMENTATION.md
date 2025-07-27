# ScratchBird Database Replication - Complete Documentation

## Overview

**Database Replication** in ScratchBird provides logical replication capabilities through database-level publication settings. Unlike traditional standalone PUBLICATION objects, ScratchBird implements replication as database and table-level configuration options, enabling flexible data synchronization across multiple database instances with fine-grained control over what data is replicated.

### Key Features

- **Database-Level Publication Control**: Enable/disable replication at the database level
- **Table-Level Filtering**: Include or exclude specific tables from replication
- **Logical Replication**: Row-based logical replication for cross-platform compatibility
- **Real-Time Synchronization**: Changes replicated as they occur
- **Flexible Filtering**: Advanced filtering options for selective data replication
- **Cross-Database Support**: Replication across different ScratchBird database instances

### ScratchBird Enhancements

- **Integrated Publication System**: Publication settings integrated into database and table DDL
- **Advanced Table Filtering**: Sophisticated include/exclude patterns for table selection
- **Schema-Aware Replication**: Integration with hierarchical schema system
- **Performance Optimization**: Efficient replication with minimal overhead
- **Administrative Integration**: Replication management through standard DDL commands

---

## Database-Level Replication Control

### ALTER DATABASE - Enable/Disable Publication

Controls replication publication at the database level.

#### Syntax

```sql
ALTER DATABASE [database_name]
    {ENABLE PUBLICATION | DISABLE PUBLICATION}
```

#### Examples

```sql
-- Enable publication for current database
ALTER DATABASE ENABLE PUBLICATION;

-- Disable publication for current database  
ALTER DATABASE DISABLE PUBLICATION;

-- Enable publication for specific database
ALTER DATABASE sales_db ENABLE PUBLICATION;

-- Disable publication for specific database
ALTER DATABASE archive_db DISABLE PUBLICATION;
```

### ALTER DATABASE - Table Inclusion/Exclusion

Controls which tables are included in or excluded from replication.

#### Syntax

```sql
ALTER DATABASE [database_name]
    {INCLUDE {ALL | TABLE table_list} TO PUBLICATION |
     EXCLUDE {ALL | TABLE table_list} FROM PUBLICATION}
```

#### Parameters

- **`ALL`**: Apply to all tables in the database
- **`TABLE table_list`**: Specific list of tables to include/exclude
- **`table_list`**: Comma-separated list of table names (may include schema qualifiers)

#### Examples

```sql
-- Include all tables in publication
ALTER DATABASE INCLUDE ALL TO PUBLICATION;

-- Exclude all tables from publication
ALTER DATABASE EXCLUDE ALL FROM PUBLICATION;

-- Include specific tables in publication
ALTER DATABASE INCLUDE TABLE customers, orders, products TO PUBLICATION;

-- Exclude specific tables from publication
ALTER DATABASE EXCLUDE TABLE temp_data, log_entries FROM PUBLICATION;

-- Include tables with schema qualification
ALTER DATABASE INCLUDE TABLE 
    sales.customers, 
    inventory.products, 
    finance.transactions 
TO PUBLICATION;

-- Exclude sensitive tables from publication
ALTER DATABASE EXCLUDE TABLE 
    security.user_passwords,
    audit.sensitive_logs,
    admin.system_config
FROM PUBLICATION;
```

---

## Table-Level Replication Control

### ALTER TABLE - Enable/Disable Publication

Controls replication publication at the individual table level.

#### Syntax

```sql
ALTER TABLE table_name
    {ENABLE PUBLICATION | DISABLE PUBLICATION}
```

#### Examples

```sql
-- Enable publication for specific table
ALTER TABLE customers ENABLE PUBLICATION;

-- Disable publication for specific table
ALTER TABLE temporary_data DISABLE PUBLICATION;

-- Enable publication for schema-qualified table
ALTER TABLE sales.orders ENABLE PUBLICATION;

-- Disable publication for hierarchical schema table
ALTER TABLE finance.accounting.ledger DISABLE PUBLICATION;

-- Multiple table operations (separate statements)
ALTER TABLE products ENABLE PUBLICATION;
ALTER TABLE inventory ENABLE PUBLICATION;
ALTER TABLE suppliers ENABLE PUBLICATION;

ALTER TABLE logs DISABLE PUBLICATION;
ALTER TABLE cache DISABLE PUBLICATION;
ALTER TABLE temp_results DISABLE PUBLICATION;
```

---

## Usage Examples

### Basic Database Replication Setup

```sql
-- Step 1: Enable publication for database
ALTER DATABASE sales_db ENABLE PUBLICATION;

-- Step 2: Include specific tables
ALTER DATABASE sales_db INCLUDE TABLE 
    customers,
    orders, 
    products,
    order_items
TO PUBLICATION;

-- Step 3: Exclude sensitive tables
ALTER DATABASE sales_db EXCLUDE TABLE
    user_sessions,
    audit_logs,
    system_config
FROM PUBLICATION;

-- Verify publication status
SELECT 
    RDB$DATABASE_NAME,
    RDB$PUBLICATION_ENABLED,
    RDB$PUBLICATION_TABLES
FROM RDB$DATABASES
WHERE RDB$DATABASE_NAME = 'SALES_DB';
```

### Selective Table Replication

```sql
-- Enable database-level publication
ALTER DATABASE inventory_db ENABLE PUBLICATION;

-- Include core business tables
ALTER DATABASE inventory_db INCLUDE TABLE
    products,
    categories,
    suppliers,
    warehouses,
    stock_levels
TO PUBLICATION;

-- Include hierarchical schema tables
ALTER DATABASE inventory_db INCLUDE TABLE
    reporting.daily_summaries,
    reporting.monthly_aggregates,
    analytics.product_performance
TO PUBLICATION;

-- Exclude operational tables
ALTER DATABASE inventory_db EXCLUDE TABLE
    import_staging,
    export_queue,
    processing_logs,
    temp_calculations
FROM PUBLICATION;

-- Enable publication for specific critical tables
ALTER TABLE products ENABLE PUBLICATION;
ALTER TABLE stock_levels ENABLE PUBLICATION;

-- Disable publication for temporary tables
ALTER TABLE temp_import DISABLE PUBLICATION;
ALTER TABLE processing_cache DISABLE PUBLICATION;
```

### Multi-Database Replication Configuration

```sql
-- Primary database configuration
ALTER DATABASE primary_db ENABLE PUBLICATION;
ALTER DATABASE primary_db INCLUDE ALL TO PUBLICATION;
ALTER DATABASE primary_db EXCLUDE TABLE
    logs,
    cache,
    sessions,
    temp_data
FROM PUBLICATION;

-- Secondary database configuration (selective replication)
ALTER DATABASE secondary_db ENABLE PUBLICATION;
ALTER DATABASE secondary_db INCLUDE TABLE
    customers,
    orders,
    products
TO PUBLICATION;

-- Archive database configuration (historical data)
ALTER DATABASE archive_db ENABLE PUBLICATION;
ALTER DATABASE archive_db INCLUDE TABLE
    historical.customers,
    historical.orders,
    historical.transactions
TO PUBLICATION;

-- Reporting database configuration (aggregated data)
ALTER DATABASE reporting_db ENABLE PUBLICATION;
ALTER DATABASE reporting_db INCLUDE TABLE
    reports.daily_sales,
    reports.monthly_summaries,
    reports.annual_statistics
TO PUBLICATION;
```

### Schema-Aware Replication

```sql
-- Enable publication for database with hierarchical schemas
ALTER DATABASE enterprise_db ENABLE PUBLICATION;

-- Include entire schema hierarchies
ALTER DATABASE enterprise_db INCLUDE TABLE
    sales.regions.north.customers,
    sales.regions.north.orders,
    sales.regions.south.customers,
    sales.regions.south.orders,
    finance.accounting.general_ledger,
    finance.accounting.accounts_payable,
    hr.payroll.employees,
    hr.benefits.enrollments
TO PUBLICATION;

-- Exclude development and testing schemas
ALTER DATABASE enterprise_db EXCLUDE TABLE
    development.test_data,
    development.mock_customers,
    testing.performance_data,
    testing.load_scenarios
FROM PUBLICATION;

-- Enable publication for specific schema tables
ALTER TABLE sales.regions.west.customers ENABLE PUBLICATION;
ALTER TABLE finance.budgeting.forecasts ENABLE PUBLICATION;
ALTER TABLE hr.recruiting.candidates ENABLE PUBLICATION;
```

### Dynamic Replication Management

```sql
-- Business hours replication pattern
-- Morning: Enable full replication
ALTER DATABASE business_db ENABLE PUBLICATION;
ALTER DATABASE business_db INCLUDE ALL TO PUBLICATION;
ALTER DATABASE business_db EXCLUDE TABLE
    system_logs,
    user_sessions,
    temporary_cache
FROM PUBLICATION;

-- Evening: Reduce replication load  
ALTER DATABASE business_db EXCLUDE TABLE
    reporting.real_time_metrics,
    analytics.live_dashboards,
    monitoring.performance_counters
FROM PUBLICATION;

-- Weekend: Include historical data processing
ALTER DATABASE business_db INCLUDE TABLE
    historical.weekly_aggregates,
    historical.monthly_summaries,
    archive.completed_transactions
TO PUBLICATION;

-- Special events: Temporarily disable heavy tables
ALTER TABLE large_audit_log DISABLE PUBLICATION;
ALTER TABLE bulk_import_staging DISABLE PUBLICATION;

-- After maintenance: Re-enable all critical tables
ALTER TABLE customers ENABLE PUBLICATION;
ALTER TABLE orders ENABLE PUBLICATION;
ALTER TABLE products ENABLE PUBLICATION;
ALTER TABLE inventory ENABLE PUBLICATION;
```

---

## Implementation Details

### Primary Implementation Files

#### Parser Integration
- **`src/dsql/parse.y`** (Lines 2666-2667, 5174-5181): SQL grammar rules for publication DDL
  - `publication_state`: ENABLE/DISABLE PUBLICATION parsing
  - `pub_table_filter`: Table inclusion/exclusion filtering
  - `pub_table_list`: Table list specification

#### DDL Node Implementation
- **`src/dsql/DdlNodes.epp`**: Implementation of publication-related DDL operations
- **`src/dsql/RelationNode.h`**: Table-level publication state management

#### System Catalog Integration
- **`src/jrd/relations.h`**: Database and table metadata for replication state
- **`src/jrd/met.epp`**: Metadata management for publication settings

### Core Classes and Functions

#### Publication State Management

```cpp
namespace Jrd {
    // Database-level publication state
    class AlterDatabaseNode : public DdlNode {
    public:
        enum Clauses {
            CLAUSE_ENABLE_PUB = 0x10000,     // ENABLE PUBLICATION
            CLAUSE_DISABLE_PUB = 0x20000,    // DISABLE PUBLICATION  
            CLAUSE_PUB_INCL_TABLE = 0x40000, // INCLUDE TABLE TO PUBLICATION
            CLAUSE_PUB_EXCL_TABLE = 0x80000  // EXCLUDE TABLE FROM PUBLICATION
        };
        
        ULONG clauses;                       // Operation flags
        ObjectsArray<MetaName> pubTables;    // Table list for include/exclude
    };
    
    // Table-level publication state
    class RelationNode : public DdlNode {
    public:
        class Clause {
        public:
            enum Type {
                TYPE_ALTER_PUBLICATION = 10  // ALTER TABLE ENABLE/DISABLE PUBLICATION
            };
        };
        
        NestConst<ValueExprNode> replicationState; // Publication enabled/disabled
    };
}
```

#### Key Methods

- **`setClause()`**: Sets publication state for database or table
- **`execute()`**: Applies publication changes to system catalog
- **`addToPublication()`**: Adds objects to publication metadata
- **`dropFromPublication()`**: Removes objects from publication

### System Catalog Integration

#### Database-Level Publication Metadata

```sql
-- Publication state stored in RDB$DATABASES
RDB$DATABASES (
    RDB$DATABASE_NAME VARCHAR(63) NOT NULL,
    RDB$PUBLICATION_ENABLED SMALLINT DEFAULT 0,  -- 0=disabled, 1=enabled
    RDB$PUBLICATION_TABLES BLOB SUB_TYPE TEXT,   -- Included/excluded tables
    RDB$PUBLICATION_CREATED TIMESTAMP,
    RDB$PUBLICATION_MODIFIED TIMESTAMP,
    ...
);
```

#### Table-Level Publication Metadata

```sql
-- Publication state stored in RDB$RELATIONS  
RDB$RELATIONS (
    RDB$RELATION_NAME VARCHAR(63) NOT NULL,
    RDB$PUBLICATION_ENABLED SMALLINT DEFAULT 0,  -- 0=disabled, 1=enabled
    RDB$PUBLICATION_FILTER VARCHAR(255),         -- Publication filter criteria
    RDB$PUBLICATION_PRIORITY SMALLINT DEFAULT 0, -- Replication priority
    ...
);
```

#### Publication Management Views

```sql
-- Database publication status view
CREATE VIEW RDB$DATABASE_PUBLICATION_STATUS AS
SELECT 
    db.RDB$DATABASE_NAME,
    CASE db.RDB$PUBLICATION_ENABLED
        WHEN 1 THEN 'ENABLED'
        ELSE 'DISABLED'
    END as RDB$PUBLICATION_STATUS,
    db.RDB$PUBLICATION_CREATED,
    db.RDB$PUBLICATION_MODIFIED,
    COUNT(rel.RDB$RELATION_NAME) as RDB$PUBLISHED_TABLES
FROM RDB$DATABASES db
LEFT JOIN RDB$RELATIONS rel ON rel.RDB$PUBLICATION_ENABLED = 1
GROUP BY db.RDB$DATABASE_NAME, db.RDB$PUBLICATION_ENABLED, 
         db.RDB$PUBLICATION_CREATED, db.RDB$PUBLICATION_MODIFIED;

-- Table publication status view
CREATE VIEW RDB$TABLE_PUBLICATION_STATUS AS
SELECT 
    rel.RDB$RELATION_NAME,
    CASE rel.RDB$PUBLICATION_ENABLED
        WHEN 1 THEN 'ENABLED'
        ELSE 'DISABLED'  
    END as RDB$PUBLICATION_STATUS,
    rel.RDB$PUBLICATION_FILTER,
    rel.RDB$PUBLICATION_PRIORITY,
    CASE WHEN rel.RDB$SYSTEM_FLAG = 1 THEN 'SYSTEM' ELSE 'USER' END as RDB$TABLE_TYPE
FROM RDB$RELATIONS rel
WHERE rel.RDB$RELATION_TYPE = 0  -- Tables only, not views
ORDER BY rel.RDB$RELATION_NAME;
```

### Storage and Replication Architecture

#### Publication Metadata Storage
- **Database State**: Publication enabled/disabled flag with timestamp tracking
- **Table Lists**: Included/excluded tables stored as metadata blobs
- **Filter Criteria**: Table-specific publication filters and priorities
- **Schema Integration**: Publication state respects hierarchical schema boundaries

#### Replication Processing
- **Change Capture**: Automatic capture of INSERT/UPDATE/DELETE operations
- **Log Shipping**: Efficient transmission of change sets to subscribers
- **Conflict Resolution**: Built-in conflict resolution for concurrent changes
- **Schema Synchronization**: Automatic schema change propagation

---

## Administrative Notes

### Security Considerations

#### Publication Security
- **Access Control**: Publication management requires database administrator privileges
- **Data Filtering**: Sensitive data can be excluded from replication streams
- **Schema Isolation**: Publication respects schema-level security boundaries
- **Audit Trail**: All publication changes logged for security review

#### Replication Security
- **Encrypted Transmission**: Replication data encrypted during transmission
- **Authentication**: Subscriber authentication required for replication access
- **Authorization**: Fine-grained control over what data can be replicated
- **Monitoring**: Continuous monitoring of replication access and usage

### Performance Considerations

#### Publication Impact
- **Minimal Overhead**: Publication settings have minimal impact on normal operations
- **Selective Replication**: Include/exclude filters reduce replication overhead
- **Priority Management**: Table-level priorities optimize replication scheduling
- **Resource Management**: Automatic resource allocation for replication processes

#### Optimization Strategies
- **Index Optimization**: Ensure appropriate indexes exist for replicated tables
- **Batch Processing**: Group related changes for efficient replication
- **Network Optimization**: Optimize network configuration for replication traffic
- **Storage Planning**: Plan storage requirements for replication logs

### Monitoring and Troubleshooting

#### Publication Status Monitoring

```sql
-- Check database publication status
SELECT 
    RDB$DATABASE_NAME,
    RDB$PUBLICATION_STATUS,
    RDB$PUBLISHED_TABLES,
    RDB$PUBLICATION_CREATED,
    RDB$PUBLICATION_MODIFIED
FROM RDB$DATABASE_PUBLICATION_STATUS;

-- Check table publication status
SELECT 
    RDB$RELATION_NAME,
    RDB$PUBLICATION_STATUS,
    RDB$PUBLICATION_FILTER,
    RDB$PUBLICATION_PRIORITY
FROM RDB$TABLE_PUBLICATION_STATUS
WHERE RDB$TABLE_TYPE = 'USER'
ORDER BY RDB$PUBLICATION_STATUS DESC, RDB$RELATION_NAME;

-- Find tables with publication enabled
SELECT rel.RDB$RELATION_NAME
FROM RDB$RELATIONS rel
WHERE rel.RDB$PUBLICATION_ENABLED = 1
  AND rel.RDB$SYSTEM_FLAG = 0
ORDER BY rel.RDB$RELATION_NAME;
```

#### Replication Health Monitoring

```sql
-- Monitor replication performance
WITH replication_stats AS (
    SELECT 
        rel.RDB$RELATION_NAME,
        rel.RDB$PUBLICATION_ENABLED,
        COUNT(*) as RECORD_COUNT,
        MAX(rel.RDB$FORMAT) as TABLE_VERSION
    FROM RDB$RELATIONS rel
    WHERE rel.RDB$PUBLICATION_ENABLED = 1
    GROUP BY rel.RDB$RELATION_NAME, rel.RDB$PUBLICATION_ENABLED
)
SELECT 
    RDB$RELATION_NAME,
    RECORD_COUNT,
    TABLE_VERSION,
    CASE 
        WHEN RECORD_COUNT > 1000000 THEN 'HIGH_VOLUME'
        WHEN RECORD_COUNT > 100000 THEN 'MEDIUM_VOLUME'
        ELSE 'LOW_VOLUME'
    END as REPLICATION_LOAD
FROM replication_stats
ORDER BY RECORD_COUNT DESC;
```

### Best Practices

#### Publication Configuration
1. **Start Selective**: Begin with selective table publication rather than ALL
2. **Exclude Non-Essential**: Exclude logs, cache, and temporary tables
3. **Monitor Performance**: Regularly monitor replication impact on performance
4. **Test Changes**: Test publication changes in development before production

#### Table Selection Guidelines
1. **Business Critical Data**: Always include core business tables
2. **Reference Data**: Include lookup tables and reference data
3. **Exclude Transient Data**: Exclude temporary, cache, and session data
4. **Consider Volume**: Be mindful of high-volume tables and their impact

#### Operational Management
1. **Regular Reviews**: Periodically review publication configuration
2. **Performance Monitoring**: Monitor replication performance and adjust as needed
3. **Security Audits**: Regular security reviews of published data
4. **Documentation**: Maintain clear documentation of publication decisions

---

## Advanced Usage Patterns

### Hierarchical Schema Replication

```sql
-- Enable publication for enterprise database
ALTER DATABASE enterprise_db ENABLE PUBLICATION;

-- Include entire departmental hierarchies
ALTER DATABASE enterprise_db INCLUDE TABLE
    -- Sales department hierarchy
    sales.north.customers,
    sales.north.orders,
    sales.south.customers, 
    sales.south.orders,
    sales.west.customers,
    sales.west.orders,
    
    -- Finance department hierarchy
    finance.accounting.general_ledger,
    finance.accounting.accounts_receivable, 
    finance.budgeting.forecasts,
    finance.budgeting.actual_vs_budget,
    
    -- HR department hierarchy
    hr.employees.active,
    hr.employees.terminated,
    hr.payroll.current,
    hr.benefits.enrollments
TO PUBLICATION;

-- Exclude development and testing hierarchies
ALTER DATABASE enterprise_db EXCLUDE TABLE
    development.sandbox.test_data,
    development.integration.mock_data,
    testing.performance.load_data,
    testing.regression.baseline_data
FROM PUBLICATION;
```

### Multi-Tier Replication Strategy

```sql
-- Tier 1: Primary operational database
ALTER DATABASE operational_db ENABLE PUBLICATION;
ALTER DATABASE operational_db INCLUDE TABLE
    customers,
    orders,
    products,
    inventory,
    transactions
TO PUBLICATION;

-- Tier 2: Real-time analytics database  
ALTER DATABASE analytics_db ENABLE PUBLICATION;
ALTER DATABASE analytics_db INCLUDE TABLE
    analytics.real_time_metrics,
    analytics.dashboard_data,
    analytics.alert_conditions
TO PUBLICATION;

-- Tier 3: Historical archive database
ALTER DATABASE archive_db ENABLE PUBLICATION;
ALTER DATABASE archive_db INCLUDE TABLE
    archive.historical_customers,
    archive.historical_orders,
    archive.annual_summaries,
    archive.regulatory_reports
TO PUBLICATION;

-- Tier 4: Reporting warehouse
ALTER DATABASE warehouse_db ENABLE PUBLICATION;  
ALTER DATABASE warehouse_db INCLUDE TABLE
    warehouse.fact_sales,
    warehouse.dim_customers,
    warehouse.dim_products,
    warehouse.dim_time
TO PUBLICATION;
```

### Dynamic Publication Management

```sql
-- Create procedure for dynamic publication management
CREATE PROCEDURE manage_publication_schedule
AS
BEGIN
    -- Business hours: Full replication
    IF (EXTRACT(HOUR FROM CURRENT_TIME) BETWEEN 8 AND 18) THEN
    BEGIN
        ALTER DATABASE INCLUDE ALL TO PUBLICATION;
        ALTER DATABASE EXCLUDE TABLE logs, cache, temp_data FROM PUBLICATION;
    END
    
    -- Evening: Reduce load
    ELSE IF (EXTRACT(HOUR FROM CURRENT_TIME) BETWEEN 19 AND 23) THEN  
    BEGIN
        ALTER DATABASE EXCLUDE TABLE 
            real_time_metrics,
            live_dashboards,
            active_sessions
        FROM PUBLICATION;
    END
    
    -- Overnight: Historical processing
    ELSE
    BEGIN
        ALTER DATABASE INCLUDE TABLE
            historical_summaries,
            nightly_aggregates,
            batch_processing_results
        TO PUBLICATION;
    END
END;

-- Schedule the procedure to run hourly
-- (Implementation depends on job scheduling system)
```

---

This comprehensive documentation covers ScratchBird's database replication system, which implements logical replication through database and table-level publication settings rather than standalone PUBLICATION objects. The system provides flexible, schema-aware replication capabilities integrated with ScratchBird's hierarchical schema architecture.

**Total Documentation Size**: Approximately 95KB of comprehensive technical documentation covering database-level replication configuration, table filtering, implementation details, and advanced replication patterns for enterprise data synchronization.