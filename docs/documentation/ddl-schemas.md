### DDL: Schemas

**What it is**

Schemas are logical namespaces that organize database objects like tables, views, functions, and indexes into groups. They provide a way to separate different applications, users, or functional areas within the same database while avoiding naming conflicts and enabling fine-grained access control.

**Why it matters**

- **Organization**: Group related objects together for better maintainability
- **Multi-tenancy**: Isolate different applications or tenants in the same database
- **Security**: Control access at the schema level with GRANT/REVOKE
- **Naming**: Avoid collisions by qualifying objects with schema names
- **Development**: Separate development, testing, and production objects

**How to use it**

Create schemas to organize your database objects logically. Use schema-qualified names (schema.object) to reference objects explicitly, or set the search_path to control implicit resolution. Grant appropriate permissions on schemas to control access.

## Schema Concepts

### Schema Namespace

Every database object exists within a schema:
- Default schema is typically `public`
- Objects can be referenced as `schema.object`
- Unqualified names resolved via search_path

### Schema Search Path

The search path determines how unqualified object names are resolved:

```sql
-- View current search path
SHOW search_path;

-- Set search path for session
SET search_path TO myapp, shared, public;

-- Set default search path for database
ALTER DATABASE mydb SET search_path TO app_v2, app_v1, public;

-- Set search path for role
ALTER ROLE app_user SET search_path TO application, public;
```

## CREATE SCHEMA

### Basic Syntax

```sql
CREATE SCHEMA [IF NOT EXISTS] schema_name
[AUTHORIZATION owner_name]
[schema_element [...]]
```

### Simple Schema Creation

```sql
-- Basic schema
CREATE SCHEMA analytics;

-- Schema with authorization
CREATE SCHEMA sales AUTHORIZATION sales_team;

-- Schema if not exists
CREATE SCHEMA IF NOT EXISTS reporting;

-- Schema with owner
CREATE SCHEMA marketing AUTHORIZATION marketing_user;
```

### Schema with Initial Objects

```sql
-- Create schema with tables
CREATE SCHEMA inventory
    CREATE TABLE products (
        id INTEGER PRIMARY KEY,
        name VARCHAR(100),
        quantity INTEGER
    )
    CREATE TABLE warehouses (
        id INTEGER PRIMARY KEY,
        location VARCHAR(200)
    )
    CREATE INDEX idx_products_name ON products(name);

-- Schema with multiple object types
CREATE SCHEMA app_v1
    CREATE TABLE users (
        id SERIAL PRIMARY KEY,
        username VARCHAR(50) UNIQUE
    )
    CREATE VIEW active_users AS 
        SELECT * FROM users WHERE is_active = TRUE
    CREATE SEQUENCE user_id_seq START WITH 1000;
```

## Schema Organization Patterns

### Application Separation

```sql
-- Separate schemas per application
CREATE SCHEMA crm;       -- Customer relationship management
CREATE SCHEMA inventory;  -- Inventory management  
CREATE SCHEMA accounting; -- Financial data
CREATE SCHEMA hr;        -- Human resources

-- Application-specific tables
CREATE TABLE crm.customers (
    id INTEGER PRIMARY KEY,
    name VARCHAR(200),
    email VARCHAR(100)
);

CREATE TABLE inventory.products (
    id INTEGER PRIMARY KEY,
    sku VARCHAR(50),
    quantity INTEGER
);

CREATE TABLE accounting.invoices (
    id INTEGER PRIMARY KEY,
    customer_id INTEGER,
    amount DECIMAL(10,2)
);
```

### Version Management

```sql
-- Versioned schemas for upgrades
CREATE SCHEMA app_v1;  -- Current production
CREATE SCHEMA app_v2;  -- Next version being developed
CREATE SCHEMA app_archive;  -- Archived old versions

-- Migrate by switching search path
-- Development uses v2
SET search_path TO app_v2, public;

-- Production still uses v1
SET search_path TO app_v1, public;

-- Atomic switch during deployment
ALTER DATABASE myapp SET search_path TO app_v2, public;
```

### Multi-Tenant Architecture

```sql
-- Schema per tenant
CREATE SCHEMA tenant_001;
CREATE SCHEMA tenant_002;
CREATE SCHEMA tenant_003;

-- Shared schema for common data
CREATE SCHEMA shared;

-- Template schema for new tenants
CREATE SCHEMA template;

-- Create tenant tables
CREATE TABLE tenant_001.users AS TABLE template.users;
CREATE TABLE tenant_001.orders AS TABLE template.orders;

-- Dynamic schema selection
SET search_path TO tenant_001, shared, public;
```

### Staging and ETL

```sql
-- ETL pipeline schemas
CREATE SCHEMA staging;     -- Raw imported data
CREATE SCHEMA transformed; -- Cleaned data
CREATE SCHEMA warehouse;   -- Final analytical tables

-- Staging tables (temporary)
CREATE TABLE staging.raw_sales (
    data JSONB,
    imported_at TIMESTAMP DEFAULT NOW()
);

-- Transformation tables
CREATE TABLE transformed.sales (
    id INTEGER,
    product_id INTEGER,
    amount DECIMAL(10,2),
    sale_date DATE
);

-- Final warehouse
CREATE TABLE warehouse.sales_facts (
    id BIGINT PRIMARY KEY,
    product_key INTEGER,
    date_key INTEGER,
    amount DECIMAL(10,2),
    quantity INTEGER
);
```

## ALTER SCHEMA

Modify existing schemas:

### Rename Schema

```sql
-- Rename schema
ALTER SCHEMA old_name RENAME TO new_name;

-- Example: Version upgrade
ALTER SCHEMA app_current RENAME TO app_old;
ALTER SCHEMA app_new RENAME TO app_current;
```

### Change Owner

```sql
-- Change schema owner
ALTER SCHEMA analytics OWNER TO analytics_team;

-- Transfer all objects in schema
ALTER SCHEMA legacy_app OWNER TO new_team;
```

### Schema Attributes

```sql
-- Set schema comment
COMMENT ON SCHEMA analytics IS 'Data analytics and reporting tables';

-- Schema-level settings (captured raw in parser)
ALTER SCHEMA myapp SET configuration_parameter = value;
```

## DROP SCHEMA

Remove schemas:

```sql
-- Drop empty schema
DROP SCHEMA old_schema;

-- Drop if exists
DROP SCHEMA IF EXISTS temp_schema;

-- Drop schema and all objects (dangerous!)
DROP SCHEMA legacy CASCADE;

-- Drop only if empty (default)
DROP SCHEMA test_schema RESTRICT;

-- Drop multiple schemas
DROP SCHEMA temp1, temp2, temp3;
```

## Schema Permissions

### GRANT Permissions

```sql
-- Grant usage (ability to access objects)
GRANT USAGE ON SCHEMA analytics TO analyst_role;

-- Grant create (ability to create objects)
GRANT CREATE ON SCHEMA sandbox TO developer_role;

-- Grant all privileges
GRANT ALL ON SCHEMA app_schema TO app_owner;

-- Grant to multiple roles
GRANT USAGE ON SCHEMA reporting 
TO analyst_role, manager_role, executive_role;

-- Grant with grant option
GRANT USAGE ON SCHEMA shared 
TO team_lead WITH GRANT OPTION;
```

### REVOKE Permissions

```sql
-- Revoke usage
REVOKE USAGE ON SCHEMA sensitive FROM public;

-- Revoke create
REVOKE CREATE ON SCHEMA production FROM developer_role;

-- Revoke all
REVOKE ALL ON SCHEMA temp FROM guest_user;

-- Cascade revoke
REVOKE ALL ON SCHEMA old_app FROM ALL CASCADE;
```

### Default Privileges

```sql
-- Set default privileges for objects created in schema
ALTER DEFAULT PRIVILEGES IN SCHEMA analytics
GRANT SELECT ON TABLES TO analyst_role;

ALTER DEFAULT PRIVILEGES IN SCHEMA app
GRANT ALL ON TABLES TO app_role;

ALTER DEFAULT PRIVILEGES IN SCHEMA reporting
GRANT SELECT ON TABLES TO PUBLIC;

-- For specific user's objects
ALTER DEFAULT PRIVILEGES FOR ROLE data_engineer
IN SCHEMA warehouse
GRANT SELECT ON TABLES TO analyst_role;
```

## Schema Usage Examples

### Cross-Schema Queries

```sql
-- Explicit schema references
SELECT c.name, o.total
FROM crm.customers c
JOIN accounting.orders o ON c.id = o.customer_id;

-- Using search path
SET search_path TO crm, accounting;
SELECT c.name, o.total
FROM customers c  -- Found in crm schema
JOIN orders o ON c.id = o.customer_id;  -- Found in accounting schema
```

### Schema-Qualified DDL

```sql
-- Create objects in specific schema
CREATE TABLE analytics.daily_summary (
    date DATE PRIMARY KEY,
    revenue DECIMAL(12,2),
    orders INTEGER
);

CREATE VIEW reporting.customer_summary AS
SELECT * FROM crm.customers c
JOIN analytics.customer_metrics m ON c.id = m.customer_id;

CREATE INDEX analytics.idx_daily_date 
ON analytics.daily_summary(date);
```

### Information Schema Queries

```sql
-- List all schemas
SELECT schema_name 
FROM information_schema.schemata
WHERE schema_name NOT IN ('pg_catalog', 'information_schema')
ORDER BY schema_name;

-- Count objects per schema
SELECT 
    schemaname,
    COUNT(*) as table_count
FROM pg_tables
WHERE schemaname NOT IN ('pg_catalog', 'information_schema')
GROUP BY schemaname;

-- Find all tables in a schema
SELECT table_name
FROM information_schema.tables
WHERE table_schema = 'analytics'
  AND table_type = 'BASE TABLE'
ORDER BY table_name;

-- Schema sizes
SELECT 
    schema_name,
    pg_size_pretty(SUM(pg_total_relation_size(quote_ident(schema_name)||'.'||quote_ident(table_name)))::BIGINT) as size
FROM information_schema.tables
WHERE table_schema NOT IN ('pg_catalog', 'information_schema')
GROUP BY schema_name
ORDER BY SUM(pg_total_relation_size(quote_ident(schema_name)||'.'||quote_ident(table_name))) DESC;
```

## Schema Design Best Practices

### Naming Conventions

```sql
-- Use descriptive, lowercase names
CREATE SCHEMA user_management;  -- Good
-- CREATE SCHEMA UsrMgmt;        -- Avoid

-- Use underscores for multi-word names
CREATE SCHEMA data_warehouse;   -- Good
-- CREATE SCHEMA data-warehouse; -- Avoid (requires quoting)

-- Version suffixes for upgrades
CREATE SCHEMA app_v2;
CREATE SCHEMA app_2024_01;
```

### Security Patterns

```sql
-- Least privilege principle
-- 1. Revoke default public access
REVOKE ALL ON SCHEMA sensitive FROM PUBLIC;

-- 2. Grant specific permissions
GRANT USAGE ON SCHEMA sensitive TO authorized_role;

-- 3. Use separate schemas for sensitive data
CREATE SCHEMA pii;  -- Personal information
CREATE SCHEMA financial;  -- Financial data
CREATE SCHEMA public_data;  -- Non-sensitive

-- 4. Audit schema access
CREATE TABLE audit.schema_access (
    username VARCHAR(50),
    schema_name VARCHAR(50),
    access_time TIMESTAMP,
    action VARCHAR(20)
);
```

### Development Workflow

```sql
-- Development schemas
CREATE SCHEMA dev_john;    -- Developer sandbox
CREATE SCHEMA test;        -- Testing environment
CREATE SCHEMA staging;     -- Pre-production
CREATE SCHEMA production;  -- Production

-- Promotion workflow
-- 1. Develop in personal schema
CREATE TABLE dev_john.new_feature (...);

-- 2. Test in test schema
CREATE TABLE test.new_feature AS TABLE dev_john.new_feature;

-- 3. Stage for production
CREATE TABLE staging.new_feature AS TABLE test.new_feature;

-- 4. Deploy to production
CREATE TABLE production.new_feature AS TABLE staging.new_feature;
```

## Complex Schema Examples

### Microservices Architecture

```sql
-- Service-specific schemas
CREATE SCHEMA auth_service;
CREATE SCHEMA user_service;
CREATE SCHEMA order_service;
CREATE SCHEMA payment_service;
CREATE SCHEMA notification_service;

-- Shared schemas
CREATE SCHEMA common;  -- Shared types and functions
CREATE SCHEMA audit;   -- Audit logs

-- Service tables
CREATE TABLE auth_service.sessions (
    id UUID PRIMARY KEY,
    user_id INTEGER,
    token VARCHAR(255),
    expires_at TIMESTAMP
);

CREATE TABLE order_service.orders (
    id BIGINT PRIMARY KEY,
    user_id INTEGER,
    status VARCHAR(20),
    total DECIMAL(10,2)
);

-- Cross-service view
CREATE VIEW common.user_orders AS
SELECT 
    u.id,
    u.username,
    COUNT(o.id) as order_count
FROM user_service.users u
LEFT JOIN order_service.orders o ON u.id = o.user_id
GROUP BY u.id, u.username;
```

### Data Lake Organization

```sql
-- Raw data ingestion
CREATE SCHEMA bronze;  -- Raw data as ingested

-- Cleaned and validated
CREATE SCHEMA silver;  -- Cleaned, deduplicated

-- Business-ready
CREATE SCHEMA gold;    -- Aggregated, business rules applied

-- Bronze: Raw data
CREATE TABLE bronze.raw_events (
    data JSONB,
    source VARCHAR(50),
    ingested_at TIMESTAMP DEFAULT NOW()
);

-- Silver: Structured
CREATE TABLE silver.events (
    event_id UUID,
    event_type VARCHAR(50),
    user_id INTEGER,
    timestamp TIMESTAMP,
    properties JSONB
);

-- Gold: Analytics-ready
CREATE TABLE gold.user_metrics (
    user_id INTEGER PRIMARY KEY,
    first_event TIMESTAMP,
    last_event TIMESTAMP,
    event_count INTEGER,
    active_days INTEGER
);
```

## Implementation Details

**Parser Implementation** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_schema`: Handles CREATE/ALTER/DROP SCHEMA
- Captures schema name and raw attributes
- Supports AUTHORIZATION clause
- Handles IF NOT EXISTS condition

**AST Structure** (`include/scratchbird/engine/ast.h`):
```cpp
struct DdlSchemaAst {
    std::string name;
    std::string action;  // CREATE|ALTER|DROP
    std::string owner;   // AUTHORIZATION
    std::string attrs;   // Raw attributes
};
```

**Code Anchors**:
- Schema parser: `src/engine/parser_ddl.cpp` (parse_ddl_schema)
- AST definition: `include/scratchbird/engine/ast.h` (DdlSchemaAst)

## See also

- [Tables](./ddl-tables.md) - Creating tables within schemas
- [Roles, Users, Grants](./ddl-roles-users-grants.md) - Schema permissions
- [Views](./ddl-views.md) - Schema-qualified views
- [Functions](./psql-routines-and-triggers.md) - Schema-qualified routines
- [Session & Transaction](./session-and-transaction.md) - Setting search_path