# ScratchBird VIEW - Complete Documentation

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

Views in ScratchBird are virtual tables that present data from one or more underlying tables through a stored SQL query. They provide data abstraction, security, and simplified access to complex data relationships while supporting advanced features like hierarchical schema integration and database link references.

### Key Features and Capabilities

- **Virtual Table Functionality**: Query-based data presentation without physical storage
- **Hierarchical Schema Support**: Full support for 3-level qualified names (`schema.subschema.view`)
- **Updatable Views**: Support for INSERT, UPDATE, DELETE operations on qualifying views
- **Security Layer**: Column and row-level security through view definitions
- **Database Link Integration**: Views can reference tables via database links
- **Complex Query Support**: Joins, aggregations, window functions, and subqueries

### ScratchBird-Specific Enhancements

1. **Schema-Aware Views**: Views can be created in hierarchical schemas up to 11 levels deep
2. **Cross-Schema References**: Views can reference objects across different schema hierarchies
3. **Database Link Views**: Views can incorporate data from remote databases via links
4. **Enhanced Security Model**: Integration with schema-based security system
5. **Advanced Metadata**: Extended view metadata for complex query optimization

---

## DDL Syntax Reference

### CREATE VIEW

Creates a new view with a specified SELECT statement that defines the view's data.

#### **Basic Syntax**
```sql
CREATE [OR ALTER] VIEW [IF NOT EXISTS] [schema_path.]view_name
[(column_list)]
AS select_statement
[WITH CHECK OPTION]
```

#### **Complete Syntax**
```sql
CREATE [OR ALTER] VIEW [IF NOT EXISTS] qualified_view_name
[(column_name [, column_name] ...)]
AS select_statement
[WITH CHECK OPTION]
```

#### **Parameter Descriptions**

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `OR ALTER` | keyword | Create or modify existing view | CREATE only |
| `IF NOT EXISTS` | keyword | Skip creation if view exists | FALSE |
| `qualified_view_name` | identifier | Hierarchical view name (up to 3 levels) | Required |
| `column_list` | list | Optional column name specification | Derived from SELECT |
| `select_statement` | query | Defining SELECT statement | Required |
| `WITH CHECK OPTION` | option | Enforce view constraints on updates | No constraint |

#### **Examples**

##### **Basic View Creation**
```sql
-- Simple view in default schema
CREATE VIEW active_customers AS
SELECT customer_id, name, email, created_date
FROM customers
WHERE is_active = TRUE;
```

##### **Hierarchical Schema View**
```sql
-- View in hierarchical schema with column aliases
CREATE VIEW finance.reporting.monthly_sales_summary
(month_year, total_revenue, order_count, avg_order_value)
AS
SELECT 
    DATE_TRUNC('month', order_date) as month_year,
    SUM(total_amount) as total_revenue,
    COUNT(*) as order_count,
    AVG(total_amount) as avg_order_value
FROM sales.orders.order_headers
WHERE order_date >= CURRENT_DATE - INTERVAL '12 months'
GROUP BY DATE_TRUNC('month', order_date);
```

##### **Complex Join View**
```sql
-- Multi-table join view with hierarchical references
CREATE VIEW crm.analytics.customer_order_history AS
SELECT 
    c.customer_id,
    c.name as customer_name,
    c.email,
    oh.order_id,
    oh.order_date,
    oh.status as order_status,
    oh.total_amount,
    od.product_code,
    od.quantity,
    od.unit_price,
    od.line_total,
    p.product_name,
    p.category
FROM crm.customers.customer_profiles c
JOIN sales.orders.order_headers oh ON c.customer_id = oh.customer_id
JOIN sales.orders.order_details od ON oh.order_id = od.order_id  
JOIN inventory.products.product_catalog p ON od.product_code = p.product_code
WHERE c.is_active = TRUE;
```

##### **Database Link View**
```sql
-- View incorporating remote data via database links
CREATE VIEW enterprise.consolidated.global_inventory AS
-- Local inventory
SELECT 
    'LOCAL' as location,
    product_code,
    quantity_available,
    warehouse_location,
    last_updated
FROM inventory.stock.current_inventory

UNION ALL

-- Remote warehouse data
SELECT 
    'EAST_COAST' as location,
    product_code,
    quantity_available,
    warehouse_location,
    last_updated
FROM inventory.stock.current_inventory@east_warehouse_link

UNION ALL

-- West coast warehouse
SELECT 
    'WEST_COAST' as location,
    product_code,
    quantity_available,
    warehouse_location,
    last_updated
FROM inventory.stock.current_inventory@west_warehouse_link;
```

##### **Updatable View with Check Option**
```sql
-- Updatable view with constraint enforcement
CREATE VIEW sales.active_orders AS
SELECT 
    order_id,
    customer_id,
    order_date,
    status,
    total_amount,
    notes
FROM sales.orders.order_headers
WHERE status IN ('PENDING', 'CONFIRMED', 'PROCESSING')
WITH CHECK OPTION;

-- This view ensures that updates maintain the WHERE condition
-- UPDATE sales.active_orders SET status = 'CANCELLED' WHERE order_id = 123;
-- This would fail because 'CANCELLED' doesn't meet the view's WHERE condition
```

##### **Conditional View Creation**
```sql
-- Safe view creation with IF NOT EXISTS
CREATE VIEW IF NOT EXISTS reporting.dashboard.key_metrics AS
SELECT 
    CURRENT_DATE as report_date,
    (SELECT COUNT(*) FROM customers WHERE is_active = TRUE) as active_customers,
    (SELECT COUNT(*) FROM sales.orders.order_headers WHERE order_date = CURRENT_DATE) as daily_orders,
    (SELECT SUM(total_amount) FROM sales.orders.order_headers WHERE order_date = CURRENT_DATE) as daily_revenue,
    (SELECT COUNT(*) FROM inventory.products.product_catalog WHERE is_active = TRUE) as active_products;
```

### ALTER VIEW

Modifies an existing view by replacing its definition with a new SELECT statement.

#### **Syntax**
```sql
ALTER VIEW qualified_view_name
[(column_list)]
AS select_statement
[WITH CHECK OPTION]
```

#### **Examples**
```sql
-- Modify existing view definition
ALTER VIEW active_customers AS
SELECT 
    customer_id, 
    name, 
    email, 
    phone,
    created_date,
    last_order_date
FROM customers c
LEFT JOIN (
    SELECT customer_id, MAX(order_date) as last_order_date
    FROM sales.orders.order_headers
    GROUP BY customer_id
) lo ON c.customer_id = lo.customer_id
WHERE c.is_active = TRUE;

-- Add check option to existing view
ALTER VIEW sales.active_orders AS
SELECT order_id, customer_id, order_date, status, total_amount
FROM sales.orders.order_headers
WHERE status IN ('PENDING', 'CONFIRMED', 'PROCESSING', 'SHIPPED')
WITH CHECK OPTION;
```

### CREATE OR ALTER VIEW

Creates a new view or modifies an existing one in a single statement.

#### **Examples**
```sql
-- Atomic create or alter operation
CREATE OR ALTER VIEW finance.reporting.profit_analysis AS
SELECT 
    DATE_TRUNC('month', transaction_date) as month_year,
    SUM(CASE WHEN transaction_type = 'REVENUE' THEN amount ELSE 0 END) as total_revenue,
    SUM(CASE WHEN transaction_type = 'EXPENSE' THEN amount ELSE 0 END) as total_expenses,
    SUM(CASE WHEN transaction_type = 'REVENUE' THEN amount ELSE -amount END) as net_profit,
    COUNT(*) as transaction_count
FROM finance.accounting.transaction_log
WHERE transaction_date >= CURRENT_DATE - INTERVAL '24 months'
GROUP BY DATE_TRUNC('month', transaction_date)
ORDER BY month_year DESC;
```

### RECREATE VIEW

Drops and recreates a view in a single atomic operation.

#### **Syntax**
```sql
RECREATE VIEW qualified_view_name
[(column_list)]
AS select_statement
[WITH CHECK OPTION]
```

#### **Examples**
```sql
-- Atomic recreate with structure changes
RECREATE VIEW customer_summary 
(customer_id, full_name, contact_info, order_summary, account_status)
AS
SELECT 
    c.customer_id,
    c.first_name || ' ' || c.last_name as full_name,
    c.email || COALESCE(' / ' || c.phone, '') as contact_info,
    COALESCE(os.order_count, 0) || ' orders, $' || COALESCE(os.total_spent, 0) as order_summary,
    CASE 
        WHEN c.is_active = TRUE THEN 'ACTIVE'
        WHEN c.is_active = FALSE THEN 'INACTIVE'
        ELSE 'UNKNOWN'
    END as account_status
FROM crm.customers.customer_profiles c
LEFT JOIN (
    SELECT 
        customer_id,
        COUNT(*) as order_count,
        SUM(total_amount) as total_spent
    FROM sales.orders.order_headers
    GROUP BY customer_id
) os ON c.customer_id = os.customer_id;
```

### DROP VIEW

Permanently removes a view from the database.

#### **Syntax**
```sql
DROP VIEW [IF EXISTS] qualified_view_name
```

#### **Examples**
```sql
-- Standard drop
DROP VIEW active_customers;

-- Safe drop with hierarchical schema
DROP VIEW IF EXISTS finance.reporting.monthly_sales_summary;

-- Drop multiple views
DROP VIEW old_customer_view;
DROP VIEW old_order_view;
DROP VIEW old_product_view;
```

---

## Usage Examples

### Business Intelligence Views

#### **Executive Dashboard View**
```sql
CREATE VIEW executive.dashboard.company_overview AS
SELECT 
    -- Key Performance Indicators
    'KPI_SUMMARY' as metric_type,
    CURRENT_DATE as report_date,
    
    -- Customer Metrics
    (SELECT COUNT(*) FROM crm.customers.customer_profiles WHERE is_active = TRUE) as active_customers,
    (SELECT COUNT(*) FROM crm.customers.customer_profiles WHERE created_date >= CURRENT_DATE - 30) as new_customers_30d,
    
    -- Sales Metrics  
    (SELECT COUNT(*) FROM sales.orders.order_headers WHERE order_date = CURRENT_DATE) as daily_orders,
    (SELECT COALESCE(SUM(total_amount), 0) FROM sales.orders.order_headers WHERE order_date = CURRENT_DATE) as daily_revenue,
    (SELECT COUNT(*) FROM sales.orders.order_headers WHERE order_date >= CURRENT_DATE - 30) as monthly_orders,
    (SELECT COALESCE(SUM(total_amount), 0) FROM sales.orders.order_headers WHERE order_date >= CURRENT_DATE - 30) as monthly_revenue,
    
    -- Inventory Metrics
    (SELECT COUNT(*) FROM inventory.products.product_catalog WHERE is_active = TRUE) as active_products,
    (SELECT COUNT(*) FROM inventory.stock.current_inventory WHERE quantity_available < reorder_level) as low_stock_items,
    
    -- Financial Metrics
    (SELECT COALESCE(SUM(amount), 0) FROM finance.accounting.transaction_log 
     WHERE transaction_date = CURRENT_DATE AND transaction_type = 'REVENUE') as daily_cash_flow
FROM RDB$DATABASE;
```

#### **Sales Performance Analysis**
```sql
CREATE VIEW sales.analytics.performance_analysis AS
SELECT 
    -- Time dimensions
    EXTRACT(YEAR FROM oh.order_date) as order_year,
    EXTRACT(MONTH FROM oh.order_date) as order_month,
    DATE_TRUNC('month', oh.order_date) as month_year,
    
    -- Geographic dimensions
    c.country_code,
    c.state_province,
    c.city,
    
    -- Product dimensions
    p.category as product_category,
    p.subcategory as product_subcategory,
    
    -- Metrics
    COUNT(DISTINCT oh.order_id) as order_count,
    COUNT(DISTINCT oh.customer_id) as unique_customers,
    SUM(od.quantity) as total_units_sold,
    SUM(od.line_total) as gross_revenue,
    AVG(oh.total_amount) as avg_order_value,
    
    -- Advanced calculations
    SUM(od.line_total) / NULLIF(SUM(od.quantity), 0) as avg_selling_price,
    COUNT(DISTINCT oh.order_id) / NULLIF(COUNT(DISTINCT oh.customer_id), 0) as orders_per_customer

FROM sales.orders.order_headers oh
JOIN sales.orders.order_details od ON oh.order_id = od.order_id
JOIN crm.customers.customer_profiles c ON oh.customer_id = c.customer_id
JOIN inventory.products.product_catalog p ON od.product_code = p.product_code
WHERE oh.order_date >= CURRENT_DATE - INTERVAL '24 months'
GROUP BY 
    EXTRACT(YEAR FROM oh.order_date),
    EXTRACT(MONTH FROM oh.order_date),
    DATE_TRUNC('month', oh.order_date),
    c.country_code,
    c.state_province, 
    c.city,
    p.category,
    p.subcategory;
```

### Security and Access Control Views

#### **Role-Based Data Access View**
```sql
-- View that filters data based on current user's role
CREATE VIEW hr.employees.employee_data_secure AS
SELECT 
    e.employee_id,
    e.first_name,
    e.last_name,
    e.email,
    e.department,
    e.job_title,
    e.hire_date,
    
    -- Conditional data based on user role
    CASE 
        WHEN CURRENT_ROLE IN ('HR_MANAGER', 'PAYROLL_ADMIN') THEN e.salary
        WHEN CURRENT_ROLE = 'DEPARTMENT_MANAGER' AND e.department = (
            SELECT department FROM hr.employees.employee_master 
            WHERE employee_id = (SELECT emp_id FROM session_context WHERE user_name = USER)
        ) THEN e.salary
        ELSE NULL 
    END as salary,
    
    CASE 
        WHEN CURRENT_ROLE IN ('HR_MANAGER', 'HR_ADMIN') THEN e.ssn
        ELSE 'XXX-XX-' || RIGHT(e.ssn, 4)
    END as ssn_masked,
    
    e.manager_id,
    e.is_active

FROM hr.employees.employee_master e
WHERE 
    -- Show all employees to HR roles
    CURRENT_ROLE IN ('HR_MANAGER', 'HR_ADMIN') OR
    -- Show department employees to department managers
    (CURRENT_ROLE = 'DEPARTMENT_MANAGER' AND e.department = (
        SELECT department FROM hr.employees.employee_master 
        WHERE employee_id = (SELECT emp_id FROM session_context WHERE user_name = USER)
    )) OR
    -- Show only self to regular employees
    (e.employee_id = (SELECT emp_id FROM session_context WHERE user_name = USER));
```

### Database Link Integration Views

#### **Multi-Location Consolidated View**
```sql
-- Consolidated view across multiple database links
CREATE VIEW logistics.consolidated.global_shipping_status AS
-- North America operations
SELECT 
    'NA' as region,
    'USA' as country,
    shipment_id,
    tracking_number,
    origin_location,
    destination_location,
    ship_date,
    estimated_delivery,
    current_status,
    carrier_name
FROM logistics.shipments.active_shipments@north_america_link
WHERE ship_date >= CURRENT_DATE - 30

UNION ALL

-- Europe operations  
SELECT 
    'EU' as region,
    'GER' as country,
    shipment_id,
    tracking_number,
    origin_location,
    destination_location,
    ship_date,
    estimated_delivery,
    current_status,
    carrier_name
FROM logistics.shipments.active_shipments@europe_link
WHERE ship_date >= CURRENT_DATE - 30

UNION ALL

-- Asia Pacific operations
SELECT 
    'APAC' as region,
    'JPN' as country,
    shipment_id,
    tracking_number,
    origin_location,
    destination_location,
    ship_date,
    estimated_delivery,
    current_status,
    carrier_name
FROM logistics.shipments.active_shipments@asia_pacific_link
WHERE ship_date >= CURRENT_DATE - 30;
```

### Advanced Query Views

#### **Window Function Analytics View**
```sql
CREATE VIEW sales.analytics.customer_lifecycle_analysis AS
SELECT 
    c.customer_id,
    c.name as customer_name,
    c.created_date as customer_since,
    
    -- Order sequence analysis
    oh.order_id,
    oh.order_date,
    oh.total_amount,
    
    -- Window function calculations
    ROW_NUMBER() OVER (PARTITION BY c.customer_id ORDER BY oh.order_date) as order_sequence,
    COUNT(*) OVER (PARTITION BY c.customer_id) as total_orders,
    SUM(oh.total_amount) OVER (PARTITION BY c.customer_id) as lifetime_value,
    AVG(oh.total_amount) OVER (PARTITION BY c.customer_id) as avg_order_value,
    
    -- Time-based analytics
    LAG(oh.order_date) OVER (PARTITION BY c.customer_id ORDER BY oh.order_date) as previous_order_date,
    LEAD(oh.order_date) OVER (PARTITION BY c.customer_id ORDER BY oh.order_date) as next_order_date,
    
    -- Calculate days between orders
    oh.order_date - LAG(oh.order_date) OVER (PARTITION BY c.customer_id ORDER BY oh.order_date) as days_since_last_order,
    
    -- Running totals
    SUM(oh.total_amount) OVER (PARTITION BY c.customer_id ORDER BY oh.order_date 
                               ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) as running_total,
    
    -- Customer segmentation
    CASE 
        WHEN COUNT(*) OVER (PARTITION BY c.customer_id) >= 10 THEN 'HIGH_FREQUENCY'
        WHEN COUNT(*) OVER (PARTITION BY c.customer_id) >= 5 THEN 'MEDIUM_FREQUENCY'
        WHEN COUNT(*) OVER (PARTITION BY c.customer_id) >= 2 THEN 'LOW_FREQUENCY'
        ELSE 'ONE_TIME'
    END as customer_segment

FROM crm.customers.customer_profiles c
JOIN sales.orders.order_headers oh ON c.customer_id = oh.customer_id
WHERE c.is_active = TRUE
  AND oh.order_date >= CURRENT_DATE - INTERVAL '24 months';
```

---

## Implementation Details

### Primary Implementation Files

| File | Purpose | Key Components |
|------|---------|----------------|
| `src/dsql/parse.y` | SQL grammar for view DDL | `create_view`, `alter_view`, `drop_view` rules |
| `src/dsql/DdlNodes.h` | DDL node definitions | `CreateAlterViewNode`, `DropRelationNode` |
| `src/dsql/DdlNodes.epp` | DDL node implementations | View creation/modification logic |
| `src/jrd/met.epp` | Metadata management | System catalog operations for views |
| `src/jrd/relations.h` | System table definitions | View-specific metadata structures |
| `src/dsql/pass1.cpp` | Query compilation | View query validation and optimization |

### Core Classes and Functions

#### **DDL Node Classes**
```cpp
// View creation and modification
class CreateAlterViewNode : public RelationNode {
    // View definition processing
    NestConst<SelectExprNode> viewSource;    // SELECT statement
    NestConst<ValueListNode> viewColumns;    // Optional column list
    bool checkOption;                        // WITH CHECK OPTION flag
    bool replace;                           // CREATE OR ALTER flag
    
    void execute(thread_db* tdbb) override;
    void dsqlPass(DsqlCompilerScratch* dsqlScratch) override;
    void storeViewDefinition(thread_db* tdbb);
    void validateViewQuery(DsqlCompilerScratch* dsqlScratch);
};

// View deletion
class DropRelationNode : public DdlNode {
    bool silent;        // IF EXISTS flag
    bool isView;        // Distinguish view from table drop
    
    void dropViewDefinition(thread_db* tdbb);
    void checkViewDependencies(thread_db* tdbb);
};
```

#### **Key Functions**
- `MET_store_view()` - Store view metadata in system catalog
- `MET_load_view()` - Load view definition for query expansion
- `dsql_expand_view()` - Expand view references in queries
- `validateViewUpdatability()` - Check if view supports DML operations
- `checkCircularViewReference()` - Prevent recursive view definitions

### System Catalog Entries

#### **RDB$RELATIONS** (View metadata)
```sql
-- Views are stored alongside tables with relation_type = 1
CREATE TABLE RDB$RELATIONS (
    RDB$RELATION_NAME VARCHAR(63) NOT NULL,
    RDB$SYSTEM_FLAG SMALLINT,
    RDB$RELATION_TYPE SMALLINT,  -- 1 for views, 0 for tables
    RDB$OWNER_NAME VARCHAR(63),
    RDB$DESCRIPTION BLOB SUB_TYPE TEXT,
    RDB$SQL_SECURITY BOOLEAN,
    RDB$SCHEMA_NAME VARCHAR(63),     -- ScratchBird enhancement
    RDB$PARENT_SCHEMA VARCHAR(63),   -- ScratchBird hierarchical support
    PRIMARY KEY (RDB$RELATION_NAME)
);
```

#### **RDB$VIEW_RELATIONS** (View dependencies)
```sql
CREATE TABLE RDB$VIEW_RELATIONS (
    RDB$VIEW_NAME VARCHAR(63) NOT NULL,
    RDB$RELATION_NAME VARCHAR(63) NOT NULL,
    RDB$VIEW_CONTEXT SMALLINT,
    RDB$CONTEXT_NAME VARCHAR(63),
    PRIMARY KEY (RDB$VIEW_NAME, RDB$RELATION_NAME, RDB$VIEW_CONTEXT)
);
```

#### **RDB$RELATION_FIELDS** (View column definitions)
```sql
-- View columns stored with computed field sources
CREATE TABLE RDB$RELATION_FIELDS (
    RDB$FIELD_NAME VARCHAR(63) NOT NULL,
    RDB$RELATION_NAME VARCHAR(63) NOT NULL,
    RDB$FIELD_SOURCE VARCHAR(63),    -- References computed field for views
    RDB$BASE_FIELD VARCHAR(63),      -- Source table column if applicable
    RDB$UPDATE_FLAG SMALLINT,        -- 0 = not updatable, 1 = updatable
    RDB$FIELD_POSITION SMALLINT,
    -- ... other standard fields
    PRIMARY KEY (RDB$FIELD_NAME, RDB$RELATION_NAME)
);
```

### View Processing Logic

#### **View Creation Process**
1. **Parse View Definition**: Extract SELECT statement and column list
2. **Validate Query**: Check syntax and resolve object references
3. **Schema Resolution**: Resolve hierarchical schema references
4. **Dependency Analysis**: Build dependency tree for referenced objects
5. **Store Metadata**: Save view definition in system catalog
6. **Generate BLR**: Convert SELECT to Binary Language Representation
7. **Update Statistics**: Initialize optimizer statistics for view

#### **View Query Expansion**
```cpp
// View expansion during query compilation
class ViewExpansion {
    // Replace view references with underlying query
    SelectExprNode* expandView(const QString& viewName, DsqlCompilerScratch* dsqlScratch) {
        // Load view definition from metadata
        ViewDefinition* viewDef = MET_load_view(dsqlScratch->getAttachment(), viewName);
        
        // Parse stored SELECT statement
        SelectExprNode* viewQuery = parseViewQuery(viewDef->getSelectText());
        
        // Apply context mapping for column aliases
        applyColumnMapping(viewQuery, viewDef->getColumnList());
        
        // Handle nested view expansion recursively
        expandNestedViews(viewQuery, dsqlScratch);
        
        return viewQuery;
    }
};
```

---

## Administrative Notes

### Backup/Restore Considerations

#### **View Backup**
- **Definition Storage**: View SELECT statements stored as text in system catalog
- **Dependency Tracking**: All referenced objects must be backed up first
- **Schema Preservation**: Hierarchical schema structure maintained
- **Security Context**: View permissions and ownership preserved

#### **Restore Process**
```sql
-- Views restored after all referenced tables and other views
-- 1. Base tables restored first
-- 2. Simple views (no view dependencies) restored
-- 3. Complex views (referencing other views) restored in dependency order
-- 4. View permissions and grants restored
```

### Security Implications

#### **View-Based Security**
- **Column-Level Security**: Hide sensitive columns through view definitions
- **Row-Level Security**: Filter data based on user context or roles
- **Schema Integration**: Leverage hierarchical schema security model
- **With Check Option**: Enforce security constraints on view updates

#### **Security Best Practices**
```sql
-- Create security views that filter based on user roles
CREATE VIEW hr.secure.employee_info AS
SELECT 
    employee_id,
    first_name,
    last_name,
    department,
    -- Conditionally expose sensitive data
    CASE WHEN CURRENT_ROLE IN ('HR_ADMIN', 'PAYROLL') 
         THEN salary 
         ELSE NULL 
    END as salary
FROM hr.employees.employee_master
WHERE 
    -- Show all to HR, only own record to others
    CURRENT_ROLE = 'HR_ADMIN' OR 
    employee_id = (SELECT emp_id FROM user_context WHERE user_name = USER);
```

### Performance Monitoring

#### **View Performance Metrics**
- **Query Execution Time**: Monitor view query performance
- **Dependency Impact**: Track performance of underlying objects
- **Materialization**: Consider materialized views for expensive queries
- **Index Usage**: Ensure underlying tables have appropriate indexes

#### **View Performance Analysis**
```sql
-- Analyze view usage and performance
SELECT 
    r.RDB$RELATION_NAME as view_name,
    r.RDB$SCHEMA_NAME,
    vr.relation_count,
    s.avg_execution_time,
    s.total_executions
FROM RDB$RELATIONS r
LEFT JOIN (
    SELECT 
        RDB$VIEW_NAME, 
        COUNT(*) as relation_count
    FROM RDB$VIEW_RELATIONS 
    GROUP BY RDB$VIEW_NAME
) vr ON r.RDB$RELATION_NAME = vr.RDB$VIEW_NAME
LEFT JOIN VIEW_STATISTICS s ON r.RDB$RELATION_NAME = s.view_name
WHERE r.RDB$RELATION_TYPE = 1  -- Views only
ORDER BY s.avg_execution_time DESC;
```

### Troubleshooting Tips

#### **Common Issues**

**1. Circular View Dependencies**
```sql
-- Problem: View A references View B which references View A
CREATE VIEW view_a AS SELECT * FROM view_b;
CREATE VIEW view_b AS SELECT * FROM view_a;
-- Error: Circular dependency detected

-- Solution: Redesign view hierarchy or use tables as intermediary
```

**2. View Column Ambiguity**
```sql
-- Problem: Column names conflict in view definition
CREATE VIEW order_summary AS
SELECT o.id, c.id, o.total  -- Both tables have 'id' column
FROM orders o
JOIN customers c ON o.customer_id = c.id;
-- Error: Ambiguous column reference

-- Solution: Use explicit column aliases
CREATE VIEW order_summary AS
SELECT o.id as order_id, c.id as customer_id, o.total
FROM orders o
JOIN customers c ON o.customer_id = c.id;
```

**3. With Check Option Violations**
```sql
-- Problem: Update violates view's WHERE condition
CREATE VIEW active_orders AS
SELECT * FROM orders WHERE status = 'ACTIVE'
WITH CHECK OPTION;

UPDATE active_orders SET status = 'CANCELLED' WHERE id = 123;
-- Error: WITH CHECK OPTION constraint violated

-- Solution: Ensure updates maintain view conditions
UPDATE active_orders SET status = 'PROCESSING' WHERE id = 123;
```

**4. Non-Updatable View Issues**
```sql
-- Problem: Attempting to update complex view
CREATE VIEW sales_summary AS
SELECT customer_id, COUNT(*) as order_count, SUM(total) as total_sales
FROM orders
GROUP BY customer_id;

INSERT INTO sales_summary VALUES (1, 5, 1000.00);
-- Error: Cannot update aggregate view

-- Solution: Use INSTEAD OF triggers or update base tables directly
```

**5. Database Link Connectivity in Views**
```sql
-- Problem: Database link unavailable when querying view
CREATE VIEW global_inventory AS
SELECT * FROM local_inventory
UNION ALL
SELECT * FROM remote_inventory@warehouse_link;

SELECT * FROM global_inventory;
-- Error: Database link 'warehouse_link' not accessible

-- Solution: Verify link status and connectivity
SELECT * FROM RDB$DATABASE_LINKS WHERE RDB$LINK_NAME = 'warehouse_link';
-- Test connectivity separately
SELECT COUNT(*) FROM remote_inventory@warehouse_link;
```

