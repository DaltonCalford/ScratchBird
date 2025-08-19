# ScratchBird SCHEMA - Complete DDL Documentation

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

SCHEMA objects in ScratchBird provide hierarchical namespace organization for database objects. Unlike traditional SQL databases with flat schema structures, ScratchBird implements true hierarchical schemas supporting up to 11 levels of nesting, enabling sophisticated organizational patterns like `company.division.department.team.schema`. This advanced schema system provides enterprise-level namespace management and object organization.

### Key Features and Capabilities

- **Hierarchical Organization**: Multi-level schema nesting (up to 11 levels deep)
- **3-Level Qualified Names**: Full support for `schema.subschema.object` naming
- **Character Set Control**: Per-schema default character sets and collations
- **Security Integration**: Schema-level SQL security modes (INVOKER/DEFINER)
- **Ownership Management**: Individual schema ownership and access control
- **Path Resolution**: Intelligent schema path resolution and navigation
- **Current Schema Context**: SET SCHEMA and HOME SCHEMA support
- **System Integration**: Full integration with all database objects
- **Performance Optimization**: Efficient schema resolution caching

### ScratchBird-Specific Enhancements

1. **Hierarchical Architecture**: PostgreSQL-style nested schemas with deeper nesting (11 levels vs PostgreSQL's flat schemas)
2. **Advanced Path Management**: Full path tracking with `RDB$SCHEMA_PATH` field
3. **Level-Aware Operations**: Schema depth tracking and level-based operations
4. **Parent-Child Relationships**: Explicit parent schema references in `RDB$PARENT_SCHEMA_NAME`
5. **Schema Navigation**: SET SCHEMA UP for hierarchical navigation
6. **Home Schema Support**: User-specific default schema assignment
7. **Character Set Inheritance**: Schema-level character set defaults
8. **Security Hierarchy**: Hierarchical security model with inheritance

---

## DDL Syntax Reference

### CREATE SCHEMA

Creates a new schema with optional configuration and parent schema relationships.

#### **Basic Syntax**
```sql
CREATE SCHEMA [IF NOT EXISTS] schema_name
    [DEFAULT CHARACTER SET charset_name]
    [DEFAULT {INVOKER | DEFINER}];
```

#### **Hierarchical Schema Syntax**
```sql
CREATE SCHEMA [IF NOT EXISTS] [parent_schema.]schema_name
    [DEFAULT CHARACTER SET charset_name [COLLATION collation_name]]
    [DEFAULT {INVOKER | DEFINER}];
```

#### **Complete Syntax with All Options**
```sql
CREATE SCHEMA [IF NOT EXISTS] [parent_schema.[subparent_schema.]]schema_name
    [DEFAULT CHARACTER SET charset_name [COLLATION collation_name]]
    [DEFAULT {INVOKER | DEFINER}];
```

#### **Parameters**

- **IF NOT EXISTS**: Skip creation if schema already exists (no error)
- **parent_schema**: Parent schema for hierarchical organization
- **schema_name**: Schema identifier (63 characters max per level)
- **DEFAULT CHARACTER SET**: Default character encoding for schema objects
- **COLLATION**: Default text collation for character data
- **DEFAULT INVOKER/DEFINER**: Default SQL security mode for schema objects

---

## CREATE SCHEMA Examples

### **Basic Schema Creation**

#### **Simple Flat Schemas**
```sql
-- Create basic schema
CREATE SCHEMA sales;

-- Create schema with character set
CREATE SCHEMA international 
    DEFAULT CHARACTER SET UTF8;

-- Create schema with full configuration
CREATE SCHEMA finance 
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI
    DEFAULT DEFINER;
```

#### **Conditional Schema Creation**
```sql
-- Safe schema creation (no error if exists)
CREATE SCHEMA IF NOT EXISTS development;

-- Create with UTF8 and error handling
CREATE SCHEMA IF NOT EXISTS production
    DEFAULT CHARACTER SET UTF8
    DEFAULT DEFINER;
```

### **Hierarchical Schema Creation**

#### **2-Level Hierarchical Schemas**
```sql
-- Create parent and child schemas
CREATE SCHEMA company;
CREATE SCHEMA company.finance;
CREATE SCHEMA company.hr;
CREATE SCHEMA company.it;

-- Create with configuration
CREATE SCHEMA company.sales
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI
    DEFAULT DEFINER;
```

#### **3-Level Hierarchical Schemas**
```sql
-- Create deep hierarchy
CREATE SCHEMA enterprise;
CREATE SCHEMA enterprise.americas;
CREATE SCHEMA enterprise.americas.usa;
CREATE SCHEMA enterprise.americas.canada;
CREATE SCHEMA enterprise.americas.mexico;

-- European division
CREATE SCHEMA enterprise.europe;
CREATE SCHEMA enterprise.europe.uk;
CREATE SCHEMA enterprise.europe.germany;
CREATE SCHEMA enterprise.europe.france;
```

#### **Complex Organizational Hierarchies**
```sql
-- Corporate structure hierarchy
CREATE SCHEMA corporation;
CREATE SCHEMA corporation.divisions;
CREATE SCHEMA corporation.divisions.technology;
CREATE SCHEMA corporation.divisions.technology.development;
CREATE SCHEMA corporation.divisions.technology.development.backend;
CREATE SCHEMA corporation.divisions.technology.development.frontend;
CREATE SCHEMA corporation.divisions.technology.development.mobile;

-- Financial structure hierarchy
CREATE SCHEMA corporation.divisions.finance;
CREATE SCHEMA corporation.divisions.finance.accounting;
CREATE SCHEMA corporation.divisions.finance.accounting.receivables;
CREATE SCHEMA corporation.divisions.finance.accounting.payables;
CREATE SCHEMA corporation.divisions.finance.treasury;
CREATE SCHEMA corporation.divisions.finance.treasury.investments;
```

### **Enterprise Schema Patterns**

#### **Department-Based Hierarchy**
```sql
-- Create departmental schema structure
CREATE SCHEMA company;
CREATE SCHEMA company.departments;

-- Individual departments
CREATE SCHEMA company.departments.sales
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI
    DEFAULT DEFINER;

CREATE SCHEMA company.departments.marketing
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI
    DEFAULT DEFINER;

CREATE SCHEMA company.departments.engineering
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI
    DEFAULT DEFINER;

-- Sub-departments
CREATE SCHEMA company.departments.engineering.backend;
CREATE SCHEMA company.departments.engineering.frontend;
CREATE SCHEMA company.departments.engineering.qa;
CREATE SCHEMA company.departments.engineering.devops;
```

#### **Geographic Schema Organization**
```sql
-- Global organization by geography
CREATE SCHEMA global;
CREATE SCHEMA global.regions;

-- Regional schemas
CREATE SCHEMA global.regions.northamerica
    DEFAULT CHARACTER SET UTF8 COLLATION EN_US;

CREATE SCHEMA global.regions.europe
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI;

CREATE SCHEMA global.regions.asia
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI;

-- Country-specific schemas
CREATE SCHEMA global.regions.europe.uk
    DEFAULT CHARACTER SET UTF8 COLLATION EN_GB;

CREATE SCHEMA global.regions.europe.germany
    DEFAULT CHARACTER SET UTF8 COLLATION DE_DE;

CREATE SCHEMA global.regions.asia.japan
    DEFAULT CHARACTER SET UTF8 COLLATION JA_JP;
```

#### **Application Module Hierarchy**
```sql
-- Application module organization
CREATE SCHEMA application;
CREATE SCHEMA application.modules;

-- Core modules
CREATE SCHEMA application.modules.user_management
    DEFAULT CHARACTER SET UTF8
    DEFAULT INVOKER;

CREATE SCHEMA application.modules.authentication
    DEFAULT CHARACTER SET UTF8
    DEFAULT DEFINER;

CREATE SCHEMA application.modules.reporting
    DEFAULT CHARACTER SET UTF8
    DEFAULT DEFINER;

-- Sub-modules
CREATE SCHEMA application.modules.user_management.profiles;
CREATE SCHEMA application.modules.user_management.permissions;
CREATE SCHEMA application.modules.reporting.financial;
CREATE SCHEMA application.modules.reporting.operational;
```

### **Development Environment Schemas**

#### **Environment-Based Organization**
```sql
-- Development environments
CREATE SCHEMA environments;

CREATE SCHEMA environments.development
    DEFAULT CHARACTER SET UTF8
    DEFAULT INVOKER;  -- More flexible for development

CREATE SCHEMA environments.testing
    DEFAULT CHARACTER SET UTF8
    DEFAULT DEFINER;

CREATE SCHEMA environments.staging
    DEFAULT CHARACTER SET UTF8
    DEFAULT DEFINER;

CREATE SCHEMA environments.production
    DEFAULT CHARACTER SET UTF8
    DEFAULT DEFINER;

-- Feature-specific development schemas
CREATE SCHEMA environments.development.features;
CREATE SCHEMA environments.development.features.user_auth;
CREATE SCHEMA environments.development.features.payment_system;
CREATE SCHEMA environments.development.features.reporting_v2;
```

---

## ALTER SCHEMA

Modifies existing schema properties including character sets and security modes.

### **ALTER SCHEMA Syntax**
```sql
ALTER SCHEMA schema_name
    { SET DEFAULT CHARACTER SET charset_name |
      DROP DEFAULT CHARACTER SET |
      SET DEFAULT {INVOKER | DEFINER} |
      DROP DEFAULT SQL SECURITY };
```

### **ALTER SCHEMA Examples**

#### **Character Set Management**
```sql
-- Change default character set
ALTER SCHEMA sales SET DEFAULT CHARACTER SET UTF8;

-- Set character set with collation
ALTER SCHEMA international SET DEFAULT CHARACTER SET UTF8;

-- Remove default character set (inherit from database)
ALTER SCHEMA legacy DROP DEFAULT CHARACTER SET;
```

#### **Security Mode Configuration**
```sql
-- Set default SQL security to DEFINER
ALTER SCHEMA finance SET DEFAULT DEFINER;

-- Set default SQL security to INVOKER
ALTER SCHEMA development SET DEFAULT INVOKER;

-- Remove default SQL security (inherit from database)
ALTER SCHEMA temp DROP DEFAULT SQL SECURITY;
```

#### **Hierarchical Schema Alterations**
```sql
-- Modify nested schema properties
ALTER SCHEMA company.finance.accounting SET DEFAULT CHARACTER SET UTF8;
ALTER SCHEMA company.hr.payroll SET DEFAULT DEFINER;

-- Modify deep hierarchy schemas
ALTER SCHEMA enterprise.americas.usa.operations SET DEFAULT CHARACTER SET UTF8;
ALTER SCHEMA corporation.divisions.technology.development SET DEFAULT INVOKER;
```

---

## RECREATE SCHEMA

Drops and recreates schema with new definition, preserving dependencies where possible.

### **RECREATE SCHEMA Syntax**
```sql
RECREATE SCHEMA schema_name
    [DEFAULT CHARACTER SET charset_name]
    [DEFAULT {INVOKER | DEFINER}];
```

### **RECREATE SCHEMA Examples**

#### **Complete Schema Redefinition**
```sql
-- Recreate with new character set
RECREATE SCHEMA sales
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI
    DEFAULT DEFINER;

-- Recreate hierarchical schema
RECREATE SCHEMA company.finance
    DEFAULT CHARACTER SET UTF8
    DEFAULT INVOKER;
```

---

## DROP SCHEMA

Removes schema definitions from the database. Schemas can only be dropped if no objects depend on them.

### **DROP SCHEMA Syntax**
```sql
DROP SCHEMA [IF EXISTS] schema_name [CASCADE | RESTRICT];
```

### **DROP SCHEMA Examples**

#### **Basic Schema Removal**
```sql
-- Drop schema if it exists
DROP SCHEMA IF EXISTS temp_schema;

-- Drop schema (will fail if contains objects)
DROP SCHEMA old_development;

-- Drop schema and all contained objects
DROP SCHEMA old_legacy CASCADE;

-- Drop hierarchical schema
DROP SCHEMA company.obsolete_division CASCADE;
```

#### **Hierarchical Schema Cleanup**
```sql
-- Drop deep hierarchy (must drop children first)
DROP SCHEMA IF EXISTS corp.div.tech.dev.feature1;
DROP SCHEMA IF EXISTS corp.div.tech.dev.feature2;
DROP SCHEMA IF EXISTS corp.div.tech.dev;
DROP SCHEMA IF EXISTS corp.div.tech;
DROP SCHEMA IF EXISTS corp.div;
DROP SCHEMA IF EXISTS corp;
```

---

## Schema Navigation and Context

### **SET SCHEMA Operations**

#### **Current Schema Management**
```sql
-- Set current schema
SET SCHEMA sales;

-- Set hierarchical schema
SET SCHEMA company.finance.accounting;

-- Navigate up one level in hierarchy
SET SCHEMA UP;

-- Use objects without schema qualification
CREATE TABLE customers (id INTEGER, name VARCHAR(100));
-- Creates sales.customers or company.finance.accounting.customers
```

#### **Schema Context Examples**
```sql
-- Navigate through schema hierarchy
SET SCHEMA enterprise.americas.usa;

-- Create objects in current schema
CREATE TABLE regional_customers (
    customer_id INTEGER PRIMARY KEY,
    company_name VARCHAR(200),
    state_code CHAR(2)
);

-- Navigate up to parent schema
SET SCHEMA UP;  -- Now in enterprise.americas

-- Create objects in parent schema  
CREATE TABLE americas_summary (
    country VARCHAR(50),
    total_customers INTEGER,
    total_revenue DECIMAL(15,2)
);
```

### **Home Schema Management**

#### **SET HOME SCHEMA**
```sql
-- Set home schema for current user
SET HOME SCHEMA company.finance;

-- Set home schema for specific user
SET HOME SCHEMA FOR USER 'john_doe' TO company.hr;

-- Set hierarchical home schema
SET HOME SCHEMA enterprise.americas.usa.operations;
```

#### **Home Schema Usage**
```sql
-- Set user's default schema
SET HOME SCHEMA company.departments.engineering;

-- User connects and automatically uses home schema
-- All unqualified object references resolve to home schema

-- Create table in home schema (no qualification needed)
CREATE TABLE project_tasks (
    task_id INTEGER PRIMARY KEY,
    project_name VARCHAR(100),
    assigned_developer VARCHAR(50),
    due_date DATE
);
```

---

## Using Schemas with Database Objects

### **Schema-Qualified Object Creation**

#### **Tables in Hierarchical Schemas**
```sql
-- Create tables in nested schemas
CREATE TABLE company.sales.customers (
    customer_id INTEGER PRIMARY KEY,
    company_name VARCHAR(200),
    contact_email VARCHAR(255),
    created_date DATE DEFAULT CURRENT_DATE
);

CREATE TABLE company.sales.orders (
    order_id INTEGER PRIMARY KEY,
    customer_id INTEGER REFERENCES company.sales.customers(customer_id),
    order_date DATE DEFAULT CURRENT_DATE,
    total_amount DECIMAL(10,2)
);

-- Deep hierarchy tables
CREATE TABLE enterprise.americas.usa.operations.daily_metrics (
    metric_date DATE PRIMARY KEY,
    total_orders INTEGER,
    total_revenue DECIMAL(15,2),
    active_customers INTEGER
);
```

#### **Views Across Schema Hierarchies**
```sql
-- Create view combining data from multiple schema levels
CREATE VIEW company.reporting.sales_summary AS
SELECT 
    c.company_name,
    COUNT(o.order_id) as total_orders,
    SUM(o.total_amount) as total_revenue
FROM company.sales.customers c
LEFT JOIN company.sales.orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.company_name;

-- Cross-hierarchy view
CREATE VIEW global.executive.worldwide_summary AS
SELECT 
    'USA' as region,
    SUM(total_revenue) as revenue
FROM enterprise.americas.usa.operations.daily_metrics
WHERE metric_date >= CURRENT_DATE - 30

UNION ALL

SELECT 
    'Europe' as region,
    SUM(total_revenue) as revenue  
FROM enterprise.europe.operations.daily_metrics
WHERE metric_date >= CURRENT_DATE - 30;
```

#### **Indexes and Sequences in Schemas**
```sql
-- Create indexes in specific schemas
CREATE INDEX company.sales.idx_customers_email 
    ON company.sales.customers (contact_email);

-- Create sequences in hierarchical schemas
CREATE SEQUENCE company.finance.accounting.invoice_seq 
    START WITH 100000 INCREMENT BY 1;

-- Use schema-qualified sequence
INSERT INTO company.finance.accounting.invoices (
    invoice_id, 
    customer_id, 
    amount
) VALUES (
    NEXT VALUE FOR company.finance.accounting.invoice_seq,
    12345,
    1500.00
);
```

### **Cross-Schema References**

#### **Foreign Keys Across Schemas**
```sql
-- Reference across schema hierarchy
CREATE TABLE company.inventory.products (
    product_id INTEGER PRIMARY KEY,
    product_name VARCHAR(200),
    category_id INTEGER
);

CREATE TABLE company.sales.order_items (
    item_id INTEGER PRIMARY KEY,
    order_id INTEGER REFERENCES company.sales.orders(order_id),
    product_id INTEGER REFERENCES company.inventory.products(product_id),
    quantity INTEGER,
    unit_price DECIMAL(10,2)
);
```

#### **Procedures and Functions in Schemas**
```sql
-- Create procedure in specific schema
CREATE PROCEDURE company.sales.calculate_customer_discount(
    IN customer_id INTEGER,
    OUT discount_percentage DECIMAL(5,2)
)
AS
BEGIN
    -- Calculate discount based on purchase history
    SELECT 
        CASE 
            WHEN SUM(total_amount) > 100000 THEN 15.00
            WHEN SUM(total_amount) > 50000 THEN 10.00
            WHEN SUM(total_amount) > 10000 THEN 5.00
            ELSE 0.00
        END
    FROM company.sales.orders 
    WHERE orders.customer_id = calculate_customer_discount.customer_id
    INTO discount_percentage;
END;

-- Call procedure with schema qualification
EXECUTE PROCEDURE company.sales.calculate_customer_discount(12345, :discount);
```

---

## System Catalog Integration

ScratchBird stores schema definitions in the RDB$SCHEMAS system table with hierarchical support.

### **Querying Schema Information**

#### **List All Schemas**
```sql
-- Show all schemas with hierarchy information
SELECT 
    RDB$SCHEMA_NAME as SCHEMA_NAME,
    RDB$PARENT_SCHEMA_NAME as PARENT_SCHEMA,
    RDB$SCHEMA_PATH as FULL_PATH,
    RDB$SCHEMA_LEVEL as NESTING_LEVEL,
    RDB$OWNER_NAME as OWNER,
    RDB$CHARACTER_SET_NAME as DEFAULT_CHARSET,
    CASE RDB$SQL_SECURITY
        WHEN 0 THEN 'INVOKER'
        WHEN 1 THEN 'DEFINER'
        ELSE 'INHERITED'
    END as DEFAULT_SECURITY,
    RDB$DESCRIPTION as DESCRIPTION
FROM RDB$SCHEMAS
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
ORDER BY RDB$SCHEMA_LEVEL, RDB$SCHEMA_PATH;
```

#### **Schema Hierarchy Tree View**
```sql
-- Display schema hierarchy as tree structure
SELECT 
    LPAD('', (RDB$SCHEMA_LEVEL - 1) * 2, ' ') || RDB$SCHEMA_NAME as SCHEMA_TREE,
    RDB$SCHEMA_LEVEL as LEVEL,
    RDB$PARENT_SCHEMA_NAME as PARENT,
    RDB$OWNER_NAME as OWNER
FROM RDB$SCHEMAS
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
ORDER BY RDB$SCHEMA_PATH, RDB$SCHEMA_LEVEL;
```

#### **Schema by Level Analysis**
```sql
-- Analyze schemas by nesting level
SELECT 
    RDB$SCHEMA_LEVEL as NESTING_LEVEL,
    COUNT(*) as SCHEMA_COUNT,
    AVG(CHAR_LENGTH(RDB$SCHEMA_PATH)) as AVG_PATH_LENGTH
FROM RDB$SCHEMAS
WHERE RDB$SYSTEM_FLAG = 0 OR RDB$SYSTEM_FLAG IS NULL
GROUP BY RDB$SCHEMA_LEVEL
ORDER BY RDB$SCHEMA_LEVEL;
```

### **Schema Usage Analysis**

#### **Objects by Schema**
```sql
-- Count objects in each schema
SELECT 
    s.RDB$SCHEMA_NAME,
    s.RDB$SCHEMA_LEVEL,
    COUNT(DISTINCT r.RDB$RELATION_NAME) as TABLES_COUNT,
    COUNT(DISTINCT i.RDB$INDEX_NAME) as INDEXES_COUNT,
    COUNT(DISTINCT g.RDB$GENERATOR_NAME) as SEQUENCES_COUNT
FROM RDB$SCHEMAS s
LEFT JOIN RDB$RELATIONS r ON s.RDB$SCHEMA_NAME = r.RDB$SCHEMA_NAME
LEFT JOIN RDB$INDICES i ON s.RDB$SCHEMA_NAME = i.RDB$SCHEMA_NAME  
LEFT JOIN RDB$GENERATORS g ON s.RDB$SCHEMA_NAME = g.RDB$SCHEMA_NAME
WHERE s.RDB$SYSTEM_FLAG = 0 OR s.RDB$SYSTEM_FLAG IS NULL
GROUP BY s.RDB$SCHEMA_NAME, s.RDB$SCHEMA_LEVEL
ORDER BY s.RDB$SCHEMA_PATH;
```

#### **Schema Dependency Analysis**
```sql
-- Find cross-schema dependencies
SELECT DISTINCT
    rf.RDB$RELATION_NAME as TABLE_NAME,
    r.RDB$SCHEMA_NAME as TABLE_SCHEMA,
    dep_r.RDB$RELATION_NAME as REFERENCED_TABLE,
    dep_r.RDB$SCHEMA_NAME as REFERENCED_SCHEMA,
    'FOREIGN_KEY' as DEPENDENCY_TYPE
FROM RDB$RELATION_CONSTRAINTS rc
JOIN RDB$RELATIONS r ON rc.RDB$RELATION_NAME = r.RDB$RELATION_NAME
JOIN RDB$REF_CONSTRAINTS ref ON rc.RDB$CONSTRAINT_NAME = ref.RDB$CONSTRAINT_NAME
JOIN RDB$RELATION_CONSTRAINTS dep_rc ON ref.RDB$CONST_NAME_UQ = dep_rc.RDB$CONSTRAINT_NAME
JOIN RDB$RELATIONS dep_r ON dep_rc.RDB$RELATION_NAME = dep_r.RDB$RELATION_NAME
JOIN RDB$RELATION_FIELDS rf ON rc.RDB$RELATION_NAME = rf.RDB$RELATION_NAME
WHERE r.RDB$SCHEMA_NAME <> dep_r.RDB$SCHEMA_NAME
ORDER BY r.RDB$SCHEMA_NAME, rf.RDB$RELATION_NAME;
```

### **Current Schema Context Queries**

#### **Current User Schema Context**
```sql
-- Show current schema context
SELECT 
    CURRENT_SCHEMA as CURRENT_SCHEMA,
    HOME_SCHEMA as HOME_SCHEMA,
    USER as CURRENT_USER,
    CURRENT_ROLE as CURRENT_ROLE;

-- Show schema resolution path
SELECT 
    s.RDB$SCHEMA_NAME,
    s.RDB$SCHEMA_PATH,
    s.RDB$SCHEMA_LEVEL,
    CASE 
        WHEN s.RDB$SCHEMA_NAME = CURRENT_SCHEMA THEN 'CURRENT'
        WHEN s.RDB$SCHEMA_NAME = HOME_SCHEMA THEN 'HOME'  
        ELSE 'OTHER'
    END as SCHEMA_CONTEXT
FROM RDB$SCHEMAS s
WHERE s.RDB$SCHEMA_NAME IN (CURRENT_SCHEMA, HOME_SCHEMA)
   OR s.RDB$SCHEMA_PATH LIKE CURRENT_SCHEMA || '%'
   OR s.RDB$SCHEMA_PATH LIKE HOME_SCHEMA || '%';
```

---

## Advanced Schema Features

### **Schema Path Resolution**

#### **Intelligent Path Lookup**
```sql
-- Schema path resolution examples
SET SCHEMA company.finance;

-- These are equivalent when current schema is company.finance:
SELECT * FROM accounts;
SELECT * FROM company.finance.accounts;

-- Relative schema navigation
SET SCHEMA company.finance.accounting;
-- Reference parent schema objects
SELECT * FROM company.finance.master_accounts;

-- Navigate to sibling schemas
SELECT * FROM company.hr.employees;
```

#### **Schema Search Path**
```sql
-- Current schema resolution order:
-- 1. Explicit schema qualification
-- 2. Current schema
-- 3. Home schema  
-- 4. Database default schema

-- Example with search path
SET SCHEMA company.sales;
SET HOME SCHEMA company.shared;

-- This searches:
-- 1. company.sales.customers
-- 2. company.shared.customers  
-- 3. Default schema customers
SELECT * FROM customers;
```

### **Schema-Level Security**

#### **Schema Access Control**
```sql
-- Grant schema usage permissions
GRANT USAGE ON SCHEMA company.finance TO ROLE financial_users;
GRANT USAGE ON SCHEMA company.hr TO ROLE hr_staff;

-- Grant permissions on schema hierarchy
GRANT USAGE ON SCHEMA enterprise.americas TO ROLE americas_managers;
GRANT USAGE ON SCHEMA enterprise.americas.usa TO ROLE usa_operations;

-- Schema-level object creation rights
GRANT CREATE ON SCHEMA company.development TO ROLE developers;
```

#### **Hierarchical Security Inheritance**
```sql
-- Security flows down the hierarchy
-- Grant on parent schema
GRANT USAGE ON SCHEMA company TO ROLE company_users;

-- Automatically grants access to:
-- - company.finance (if user has object-level permissions)
-- - company.hr (if user has object-level permissions)  
-- - company.sales (if user has object-level permissions)

-- Explicit child schema permissions override parent
GRANT USAGE ON SCHEMA company.finance TO ROLE finance_team;
REVOKE USAGE ON SCHEMA company.finance FROM ROLE general_users;
```

### **Schema Monitoring and Administration**

#### **Schema Usage Monitoring**
```sql
-- Monitor schema usage patterns
SELECT 
    s.RDB$SCHEMA_NAME,
    COUNT(DISTINCT a.MON$ATTACHMENT_ID) as ACTIVE_CONNECTIONS,
    COUNT(DISTINCT st.MON$STATEMENT_ID) as ACTIVE_STATEMENTS
FROM RDB$SCHEMAS s
LEFT JOIN MON$ATTACHMENTS a ON a.MON$SCHEMA_NAME = s.RDB$SCHEMA_NAME
LEFT JOIN MON$STATEMENTS st ON st.MON$SCHEMA_NAME = s.RDB$SCHEMA_NAME
WHERE s.RDB$SYSTEM_FLAG = 0 OR s.RDB$SYSTEM_FLAG IS NULL
GROUP BY s.RDB$SCHEMA_NAME
ORDER BY ACTIVE_CONNECTIONS DESC;
```

#### **Schema Maintenance Operations**
```sql
-- Schema cleanup - find empty schemas
SELECT 
    s.RDB$SCHEMA_NAME,
    s.RDB$SCHEMA_PATH,
    'EMPTY_SCHEMA' as STATUS
FROM RDB$SCHEMAS s
LEFT JOIN RDB$RELATIONS r ON s.RDB$SCHEMA_NAME = r.RDB$SCHEMA_NAME
WHERE s.RDB$SYSTEM_FLAG = 0 OR s.RDB$SYSTEM_FLAG IS NULL
  AND r.RDB$RELATION_NAME IS NULL
ORDER BY s.RDB$SCHEMA_PATH;

-- Schema statistics update
UPDATE RDB$STATISTICS_SCHEMAS;  -- If implemented
```

---

## Error Handling and Troubleshooting

### **Common Schema Errors**

#### **Schema Creation Errors**
```sql
-- Error: Schema already exists
CREATE SCHEMA finance;
-- Solution: Use IF NOT EXISTS or choose different name

-- Error: Parent schema doesn't exist
CREATE SCHEMA nonexistent.child;
-- Solution: Create parent schema first

-- Error: Schema name too long
CREATE SCHEMA very_long_schema_name_that_exceeds_maximum_length_limit;
-- Solution: Use shorter name (63 characters max per level)
```

#### **Schema Navigation Errors**
```sql
-- Error: Schema not found
SET SCHEMA nonexistent_schema;
-- Solution: Verify schema exists and user has access

-- Error: Cannot navigate up from root
SET SCHEMA root;
SET SCHEMA UP;  -- Error: no parent schema
-- Solution: Check current schema has parent
```

### **Schema Resolution Debugging**

#### **Path Resolution Analysis**
```sql
-- Debug schema resolution
SELECT 
    'CURRENT_SCHEMA' as CONTEXT_TYPE,
    CURRENT_SCHEMA as SCHEMA_NAME,
    s.RDB$SCHEMA_PATH as FULL_PATH,
    s.RDB$SCHEMA_LEVEL as LEVEL
FROM RDB$SCHEMAS s 
WHERE s.RDB$SCHEMA_NAME = CURRENT_SCHEMA

UNION ALL

SELECT 
    'HOME_SCHEMA' as CONTEXT_TYPE,
    HOME_SCHEMA as SCHEMA_NAME,
    s.RDB$SCHEMA_PATH as FULL_PATH,
    s.RDB$SCHEMA_LEVEL as LEVEL
FROM RDB$SCHEMAS s 
WHERE s.RDB$SCHEMA_NAME = HOME_SCHEMA;
```

#### **Object Resolution Testing**
```sql
-- Test object resolution in current context
SELECT 
    'TABLE' as OBJECT_TYPE,
    r.RDB$RELATION_NAME as OBJECT_NAME,
    r.RDB$SCHEMA_NAME as RESOLVED_SCHEMA,
    CASE 
        WHEN r.RDB$SCHEMA_NAME = CURRENT_SCHEMA THEN 'CURRENT_SCHEMA'
        WHEN r.RDB$SCHEMA_NAME = HOME_SCHEMA THEN 'HOME_SCHEMA'
        ELSE 'OTHER_SCHEMA'
    END as RESOLUTION_SOURCE
FROM RDB$RELATIONS r
WHERE r.RDB$RELATION_NAME = 'target_table_name'
ORDER BY 
    CASE 
        WHEN r.RDB$SCHEMA_NAME = CURRENT_SCHEMA THEN 1
        WHEN r.RDB$SCHEMA_NAME = HOME_SCHEMA THEN 2
        ELSE 3
    END;
```

---

## Best Practices

### **Schema Design Guidelines**

1. **Logical Organization**: Use schemas to reflect business or application structure
2. **Hierarchical Planning**: Design schema hierarchy to match organizational needs
3. **Naming Conventions**: Use consistent, descriptive schema names
4. **Depth Management**: Avoid excessive nesting (recommend 3-5 levels max for readability)
5. **Security Planning**: Design schema hierarchy to support security requirements

### **Recommended Schema Patterns**

#### **Enterprise Application Pattern**
```sql
-- Top-level enterprise organization
CREATE SCHEMA enterprise;
CREATE SCHEMA enterprise.applications;

-- Application-specific schemas
CREATE SCHEMA enterprise.applications.erp
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI
    DEFAULT DEFINER;

CREATE SCHEMA enterprise.applications.crm
    DEFAULT CHARACTER SET UTF8 COLLATION UTF8_UNICODE_CI
    DEFAULT DEFINER;

-- Functional sub-schemas
CREATE SCHEMA enterprise.applications.erp.finance;
CREATE SCHEMA enterprise.applications.erp.inventory;
CREATE SCHEMA enterprise.applications.erp.hr;

CREATE SCHEMA enterprise.applications.crm.leads;
CREATE SCHEMA enterprise.applications.crm.customers;
CREATE SCHEMA enterprise.applications.crm.campaigns;
```

#### **Development Lifecycle Pattern**
```sql
-- Environment-based schema organization
CREATE SCHEMA app;
CREATE SCHEMA app.environments;

CREATE SCHEMA app.environments.dev
    DEFAULT CHARACTER SET UTF8
    DEFAULT INVOKER;  -- Flexible for development

CREATE SCHEMA app.environments.test
    DEFAULT CHARACTER SET UTF8
    DEFAULT DEFINER;

CREATE SCHEMA app.environments.prod
    DEFAULT CHARACTER SET UTF8
    DEFAULT DEFINER;

-- Feature branches in development
CREATE SCHEMA app.environments.dev.features;
CREATE SCHEMA app.environments.dev.features.user_auth;
CREATE SCHEMA app.environments.dev.features.payment_integration;
```

#### **Multi-Tenant Application Pattern**
```sql
-- Tenant-based schema organization
CREATE SCHEMA saas;
CREATE SCHEMA saas.tenants;

-- Individual tenant schemas
CREATE SCHEMA saas.tenants.company_a
    DEFAULT CHARACTER SET UTF8
    DEFAULT DEFINER;

CREATE SCHEMA saas.tenants.company_b
    DEFAULT CHARACTER SET UTF8
    DEFAULT DEFINER;

-- Shared services schema
CREATE SCHEMA saas.shared
    DEFAULT CHARACTER SET UTF8
    DEFAULT DEFINER;

CREATE SCHEMA saas.shared.authentication;
CREATE SCHEMA saas.shared.billing;
CREATE SCHEMA saas.shared.monitoring;
```

---

## Migration and Integration

### **Schema Migration Strategies**

#### **From Flat Schema Systems**
```sql
-- Migrate from traditional flat schemas
-- Old: sales, finance, hr (flat)
-- New: company.sales, company.finance, company.hr (hierarchical)

-- Create hierarchical structure
CREATE SCHEMA company;

-- Migrate existing schemas to hierarchy
-- (Requires data migration tools)
CREATE SCHEMA company.sales
    DEFAULT CHARACTER SET UTF8;
-- Copy objects from old 'sales' schema

CREATE SCHEMA company.finance
    DEFAULT CHARACTER SET UTF8;
-- Copy objects from old 'finance' schema
```

#### **Cross-Database Schema Integration**
```sql
-- Integrate schemas across databases using database links
CREATE DATABASE LINK remote_erp 
    TO 'erp_server:erp_database'
    SCHEMA_MODE HIERARCHICAL
    LOCAL_SCHEMA 'enterprise.integration'
    REMOTE_SCHEMA 'erp.production';

-- Access remote hierarchical schemas
SELECT * FROM remote_table@remote_erp;
-- Resolves to erp.production.remote_table on remote server
```

### **Schema Deployment Automation**

#### **Automated Schema Creation Scripts**
```sql
-- deployment_schemas.sql
CREATE SCHEMA IF NOT EXISTS $(COMPANY_NAME);
CREATE SCHEMA IF NOT EXISTS $(COMPANY_NAME).$(ENVIRONMENT);
CREATE SCHEMA IF NOT EXISTS $(COMPANY_NAME).$(ENVIRONMENT).$(APPLICATION);

-- Set default properties
ALTER SCHEMA $(COMPANY_NAME).$(ENVIRONMENT).$(APPLICATION)
    SET DEFAULT CHARACTER SET UTF8;

-- Set user context
SET HOME SCHEMA $(COMPANY_NAME).$(ENVIRONMENT).$(APPLICATION);
```

---

## Implementation Details

### **Primary Implementation Files**

#### **Parser and Grammar**
- **File**: `src/dsql/parse.y:3529-3586`
- **Classes**: `schema_clause`, `alter_schema_clause`, `schema_clause_options`
- **Functionality**: Parsing CREATE/ALTER/DROP SCHEMA syntax with hierarchical support

#### **DDL Node Classes**
- **File**: `src/dsql/DdlNodes.h:2817-2878`
- **Classes**:
  - `CreateAlterSchemaNode` (lines 2817-2853): Schema creation and modification
  - `DropSchemaNode` (lines 2856-2878): Schema removal with dependency checking

#### **System Catalog Integration**
- **File**: `src/jrd/relations.h:827-840`
- **Table**: `RDB$SCHEMAS` - Stores hierarchical schema definitions
- **Fields**:
  - `f_sch_schema`: Schema name
  - `f_sch_parent`: Parent schema name (hierarchical reference)
  - `f_sch_path`: Full hierarchical path
  - `f_sch_level`: Nesting level (1-11)
  - `f_sch_owner`: Schema owner
  - `f_sch_charset`: Default character set
  - `f_sch_sql_security`: Default SQL security mode

#### **Schema Navigation Support**
- **File**: `src/dsql/parse.y:6452-6472`
- **Functions**: 
  - `SET SCHEMA` - Change current schema context
  - `SET SCHEMA UP` - Navigate to parent schema
  - `SET HOME SCHEMA` - Set user default schema

### **Core Schema Operations**

#### **CreateAlterSchemaNode Methods**
- Handles both CREATE and ALTER operations
- Hierarchical path validation and parent checking
- Character set and security inheritance
- Schema dependency management

#### **DropSchemaNode Methods**
- Dependency checking across schema hierarchy
- Cascade deletion of child schemas and objects
- Orphan object handling

#### **Schema Resolution System**
- **Path Parsing**: Efficient parsing of hierarchical schema paths
- **Context Management**: Current schema and home schema tracking
- **Object Resolution**: Multi-level name resolution with inheritance
- **Security Integration**: Hierarchical permission checking

### **Storage Structures**

Hierarchical schemas are stored in RDB$SCHEMAS with:
- **Identity**: Schema name and full hierarchical path
- **Hierarchy**: Parent schema reference and nesting level
- **Configuration**: Character set, collation, SQL security defaults
- **Ownership**: Schema owner and system flags
- **Metadata**: Description and creation timestamps

---

## Administrative Operations

### **Schema Maintenance**

#### **Hierarchy Validation**
```sql
-- Validate schema hierarchy integrity
SELECT 
    s.RDB$SCHEMA_NAME,
    s.RDB$PARENT_SCHEMA_NAME,
    CASE 
        WHEN p.RDB$SCHEMA_NAME IS NULL AND s.RDB$PARENT_SCHEMA_NAME IS NOT NULL
        THEN 'ORPHANED'
        WHEN s.RDB$SCHEMA_LEVEL <> (COALESCE(p.RDB$SCHEMA_LEVEL, 0) + 1)
        THEN 'LEVEL_MISMATCH'
        ELSE 'OK'
    END as STATUS
FROM RDB$SCHEMAS s
LEFT JOIN RDB$SCHEMAS p ON s.RDB$PARENT_SCHEMA_NAME = p.RDB$SCHEMA_NAME
WHERE s.RDB$SYSTEM_FLAG = 0 OR s.RDB$SYSTEM_FLAG IS NULL;
```

#### **Schema Cleanup Operations**
```sql
-- Find and clean up empty schemas
CREATE PROCEDURE cleanup_empty_schemas
AS
DECLARE VARIABLE schema_name VARCHAR(63);
BEGIN
    FOR SELECT s.RDB$SCHEMA_NAME
        FROM RDB$SCHEMAS s
        LEFT JOIN RDB$RELATIONS r ON s.RDB$SCHEMA_NAME = r.RDB$SCHEMA_NAME
        WHERE (s.RDB$SYSTEM_FLAG = 0 OR s.RDB$SYSTEM_FLAG IS NULL)
          AND r.RDB$RELATION_NAME IS NULL
        INTO schema_name
    DO BEGIN
        EXECUTE STATEMENT 'DROP SCHEMA IF EXISTS ' || schema_name;
    END
END;
```

---

