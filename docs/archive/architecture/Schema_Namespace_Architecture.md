# ScratchBird Schema Namespace Architecture
## Hierarchical Database Organization

## Overview

ScratchBird implements a revolutionary hierarchical schema system inspired by filesystem structures, providing intuitive organization and powerful namespace management.

## Core Namespace Structure

```
[root]                                    # Database root (like / in Linux)
├── [sys]                                # System catalog and metadata
│   ├── tables                          # System table definitions
│   ├── procedures                      # System procedures
│   ├── views                           # System views
│   ├── triggers                        # System triggers
│   ├── domains                         # Type definitions
│   └── metadata                        # Database metadata
│
├── [sec]                                # Security namespace
│   ├── users                           # User accounts
│   ├── roles                           # Role definitions
│   ├── groups                          # Security groups (cumulative)
│   ├── permissions                     # Permission grants
│   ├── policies                        # Security policies
│   │   ├── ip_restrictions            # IP-based access control
│   │   ├── app_restrictions           # Application-based access
│   │   └── time_restrictions          # Time-based access
│   └── audit                           # Audit configuration
│
├── [agents]                             # Autonomous agents
│   ├── sweeper                         # Garbage collection agents
│   ├── scheduler                       # Cron-like job scheduler
│   ├── replication                     # Replication agents
│   ├── kafka                           # Kafka connectors
│   ├── monitoring                      # Health check agents
│   └── maintenance                     # Auto-maintenance tasks
│
├── [app]                                # Application namespaces
│   ├── [accounting]                    # Accounting applications
│   │   ├── [acct_foo]                 # Specific accounting app
│   │   │   ├── [division_1]           # Division-specific schema
│   │   │   │   ├── tables            # Division tables
│   │   │   │   └── procedures        # Division procedures
│   │   │   └── [division_2]           # Another division
│   │   └── [acct_bar]                 # Another accounting app
│   ├── [crm]                           # CRM applications
│   ├── [erp]                           # ERP applications
│   └── [analytics]                     # Analytics applications
│
├── [remote]                             # Remote database mappings
│   ├── [postgresql]                     # PostgreSQL servers
│   │   ├── [prod_pg]                  # Server alias
│   │   │   ├── [database1]            # Remote database
│   │   │   │   ├── [public]           # Remote schema
│   │   │   │   └── [app_schema]       # Another schema
│   │   │   └── [database2]
│   │   └── [dev_pg]                   # Another PG server
│   ├── [mysql]                          # MySQL servers
│   │   └── [mysql_prod]               # Server alias
│   │       └── [database]             # MySQL database
│   ├── [oracle]                         # Oracle servers
│   │   └── [ora_hr]                   # Server alias
│   │       └── [HR]                   # Oracle schema
│   └── [scratchbird]                    # Other ScratchBird nodes
│       └── [node2]                     # Remote ScratchBird
│
├── [users]                              # User home schemas
│   ├── [john]                          # John's private schema
│   │   ├── tables                     # Personal tables
│   │   ├── views                      # Personal views
│   │   └── procedures                 # Personal procedures
│   └── [jane]                          # Jane's private schema
│
├── [roles]                              # Role-specific schemas
│   ├── [analyst]                       # Analyst role schema
│   ├── [developer]                     # Developer role schema
│   └── [admin]                         # Admin role schema
│
└── [nosql]                              # Non-relational data (future)
    ├── [documents]                     # Document store
    ├── [graphs]                        # Graph data
    └── [timeseries]                    # Time-series data
```

## Implementation Details

### 1. Schema Navigation

```sql
-- Current schema concept (like current directory)
SET SCHEMA = '[root].[app].[accounting].[acct_foo]';
SHOW CURRENT_SCHEMA;
-- Returns: [root].[app].[accounting].[acct_foo]

-- Show objects in current schema
SHOW TABLES;  -- Shows tables in current schema only
SHOW PROCEDURES;  -- Shows procedures in current schema only

-- Relative navigation (like ../.. in filesystem)
SELECT * FROM ..\..\common.shared_table;
-- Goes up 2 levels and looks in common schema

-- Absolute path
SELECT * FROM [root].[sys].tables;

-- Change schema (like cd command)
USE SCHEMA [root].[app].[crm];
```

### 2. Search Path Implementation

```sql
-- Set search path (like PATH in Linux)
SET SEARCH_PATH = '[current], [root].[app].[common], [root].[sys]';

-- When referencing an object without full path:
SELECT * FROM customers;
-- System searches:
-- 1. [current schema].customers
-- 2. [root].[app].[common].customers  
-- 3. [root].[sys].customers
-- Returns first match

-- User-specific default search path
ALTER USER john SET DEFAULT_SEARCH_PATH = 
    '[current], [root].[users].[john], [root].[app].[accounting], [root].[sys]';
```

### 3. Synonyms (Soft Links)

```sql
-- Create synonym (like symbolic link)
CREATE SYNONYM [root].[app].[common].all_customers
FOR [root].[remote].[oracle].[ora_prod].[CRM].[CUSTOMERS];

-- Now accessible locally
SELECT * FROM [root].[app].[common].all_customers;

-- Cross-server synonyms to avoid data duplication
CREATE SYNONYM [root].[app].[accounting].vendor_list
FOR [root].[remote].[postgresql].[prod_pg].[erp].[public].[vendors];

-- Synonym chains (synonym pointing to synonym)
CREATE SYNONYM quick_customers FOR all_customers;
```

### 4. Schema Permissions

```sql
-- Grant schema-level permissions
GRANT CREATE ON SCHEMA [root].[app].[accounting] TO accounting_team;
GRANT USAGE ON SCHEMA [root].[sys] TO PUBLIC;
GRANT ALL ON SCHEMA [root].[users].[john] TO john;

-- Recursive permissions
GRANT SELECT ON ALL TABLES IN SCHEMA [root].[app].[crm] TO analysts;
GRANT EXECUTE ON ALL PROCEDURES IN SCHEMA [root].[app].* TO developers;

-- Schema ownership
ALTER SCHEMA [root].[app].[accounting].[acct_foo] OWNER TO cfo;
```

### 5. Dynamic Schema Creation

```sql
-- Automatic user home schema
CREATE USER alice WITH HOME_SCHEMA = AUTO;
-- Creates [root].[users].[alice] automatically

-- Application registration creates schema tree
CALL sys.register_application(
    'TimeTracker',
    'productivity',
    'Multi-tenant time tracking application'
);
-- Creates [root].[app].[productivity].[timetracker]

-- Tenant provisioning
CALL app.provision_tenant('TimeTracker', 'AcmeCorp');
-- Creates [root].[app].[productivity].[timetracker].[acmecorp]
```

## System Tables in [root].[sys]

```sql
-- Schema hierarchy table
CREATE TABLE [root].[sys].schemas (
    schema_id UUID PRIMARY KEY,
    schema_path VARCHAR(1000) UNIQUE,  -- Full path like [root].[app].[crm]
    parent_id UUID REFERENCES schemas(schema_id),
    schema_name VARCHAR(255),          -- Just the name part
    schema_type ENUM('SYSTEM', 'APPLICATION', 'USER', 'ROLE', 'REMOTE'),
    owner_id UUID,
    created_at TIMESTAMP,
    is_virtual BOOLEAN,                -- For remote schemas
    metadata JSONB
);

-- Schema search paths
CREATE TABLE [root].[sys].search_paths (
    path_id UUID PRIMARY KEY,
    user_id UUID,
    role_id UUID,
    search_path TEXT[],  -- Array of schema paths
    is_default BOOLEAN,
    CHECK (user_id IS NOT NULL OR role_id IS NOT NULL)
);

-- Synonyms (soft links)
CREATE TABLE [root].[sys].synonyms (
    synonym_id UUID PRIMARY KEY,
    synonym_path VARCHAR(1000) UNIQUE,
    target_path VARCHAR(1000),
    target_type ENUM('LOCAL', 'REMOTE'),
    created_by UUID,
    created_at TIMESTAMP
);
```

## Security Tables in [root].[sec]

```sql
-- Connection restrictions
CREATE TABLE [root].[sec].connection_policies (
    policy_id UUID PRIMARY KEY,
    user_id UUID,
    role_id UUID,
    allowed_ips INET[],
    blocked_ips INET[],
    allowed_apps VARCHAR[],
    time_restrictions JSONB,  -- {"monday": {"start": "09:00", "end": "17:00"}}
    max_connections INTEGER,
    require_ssl BOOLEAN
);

-- Security groups (cumulative roles)
CREATE TABLE [root].[sec].security_groups (
    group_id UUID PRIMARY KEY,
    group_name VARCHAR(255) UNIQUE,
    is_cumulative BOOLEAN DEFAULT TRUE,  -- TRUE = additive, FALSE = exclusive
    parent_group_id UUID REFERENCES security_groups(group_id)
);

-- Audit policies
CREATE TABLE [root].[sec].audit_policies (
    policy_id UUID PRIMARY KEY,
    schema_pattern VARCHAR(1000),  -- Can use wildcards
    operation_types TEXT[],        -- ['SELECT', 'INSERT', 'UPDATE', 'DELETE']
    audit_level ENUM('NONE', 'FAILED', 'SUCCESSFUL', 'ALL'),
    retention_days INTEGER
);
```

## Agent Configuration in [root].[agents]

```sql
-- Sweeper (garbage collection) configuration
CREATE TABLE [root].[agents].sweeper_config (
    config_id UUID PRIMARY KEY,
    schema_pattern VARCHAR(1000),
    sweep_interval INTERVAL,
    threshold_percent DECIMAL(5,2),
    priority INTEGER,
    is_active BOOLEAN
);

-- Scheduled jobs
CREATE TABLE [root].[agents].scheduled_jobs (
    job_id UUID PRIMARY KEY,
    job_name VARCHAR(255),
    job_type ENUM('SQL', 'PROCEDURE', 'EXTERNAL'),
    job_definition TEXT,  -- SQL or procedure call
    cron_expression VARCHAR(100),
    target_schema VARCHAR(1000),
    is_active BOOLEAN,
    last_run TIMESTAMP,
    next_run TIMESTAMP
);

-- Kafka connectors
CREATE TABLE [root].[agents].kafka_connectors (
    connector_id UUID PRIMARY KEY,
    connector_name VARCHAR(255),
    kafka_cluster VARCHAR(500),
    topic_pattern VARCHAR(255),
    target_schema VARCHAR(1000),
    target_table VARCHAR(255),
    transform_procedure VARCHAR(1000),
    is_active BOOLEAN
);
```

## Remote Database Mapping

```sql
-- Remote server definitions
CREATE TABLE [root].[sys].remote_servers (
    server_id UUID PRIMARY KEY,
    server_alias VARCHAR(255) UNIQUE,
    server_type ENUM('POSTGRESQL', 'MYSQL', 'ORACLE', 'MSSQL', 'SCRATCHBIRD'),
    connection_string TEXT ENCRYPTED,
    schema_path VARCHAR(1000),  -- Where it appears in tree
    options JSONB
);

-- Remote schema mapping
CREATE TABLE [root].[sys].remote_schemas (
    mapping_id UUID PRIMARY KEY,
    server_id UUID REFERENCES remote_servers(server_id),
    remote_schema VARCHAR(255),
    local_path VARCHAR(1000),  -- Where it appears locally
    refresh_interval INTERVAL,
    last_refresh TIMESTAMP
);
```

## Advanced Features

### 1. Schema Templates

```sql
-- Create reusable schema templates
CREATE SCHEMA TEMPLATE saas_tenant AS (
    CREATE TABLE settings (
        key VARCHAR(255) PRIMARY KEY,
        value JSONB
    );
    
    CREATE TABLE users (
        user_id UUID PRIMARY KEY,
        email VARCHAR(255) UNIQUE
    );
    
    CREATE TABLE audit_log (
        event_id UUID PRIMARY KEY,
        event_time TIMESTAMP,
        event_data JSONB
    );
);

-- Instantiate template for new tenant
CREATE SCHEMA [root].[app].[saas].[customer1] 
FROM TEMPLATE saas_tenant;
```

### 2. Schema Inheritance

```sql
-- Child schemas inherit permissions and settings
CREATE SCHEMA [root].[app].[erp].[modules]
INHERITS FROM [root].[app].[erp];

-- Override specific settings
ALTER SCHEMA [root].[app].[erp].[modules]
SET search_path = '[current], [parent], [root].[sys]';
```

### 3. Virtual Schemas

```sql
-- Create virtual schema that aggregates multiple schemas
CREATE VIRTUAL SCHEMA [root].[app].[unified_view] AS
SELECT FROM [root].[app].[crm],
           [root].[app].[erp],
           [root].[remote].[oracle].[prod];

-- Appears as single schema to users
USE SCHEMA [root].[app].[unified_view];
SHOW TABLES;  -- Shows all tables from all constituent schemas
```

### 4. Schema Discovery

```sql
-- Browse schema tree
SELECT * FROM [root].[sys].show_schema_tree('[root].[app]');

-- Returns hierarchical result:
-- [root].[app]
--   ├── [accounting]
--   │   ├── [acct_foo]
--   │   │   ├── [division_1]
--   │   │   └── [division_2]
--   │   └── [acct_bar]
--   ├── [crm]
--   └── [erp]

-- Find schemas by pattern
SELECT * FROM [root].[sys].find_schemas('%accounting%');

-- Schema statistics
SELECT * FROM [root].[sys].schema_stats('[root].[app].[crm]');
-- Returns: table_count, procedure_count, size_bytes, last_accessed
```

## Benefits of This Architecture

### 1. **Intuitive Organization**
- Filesystem-like structure familiar to all users
- Clear separation of concerns
- Logical grouping of related objects

### 2. **Multi-Tenancy Support**
- Natural tenant isolation
- Per-tenant schemas with templates
- Shared common schemas

### 3. **Security Boundaries**
- Schema-level permissions
- Clear security zones ([sec] for security)
- Audit boundaries

### 4. **Federation Transparency**
- Remote databases appear local
- Synonyms avoid data duplication
- Unified namespace across systems

### 5. **User Experience**
- Personal workspaces ([users])
- Role-based schemas ([roles])
- Relative navigation

### 6. **Application Isolation**
- Each app has its own subtree
- No namespace collisions
- Clear ownership

## Comparison with Traditional Databases

| Feature | Traditional | ScratchBird |
|---------|------------|-------------|
| Schema Structure | Flat | Hierarchical |
| Navigation | Absolute only | Relative + Absolute |
| Search Path | Limited | Full PATH-like |
| Remote Objects | Complex FDW | Natural tree integration |
| User Workspace | None | Personal schemas |
| Multi-tenancy | Complex | Natural with tree |
| Synonyms | Limited | Full soft-link support |

## Implementation Considerations

### 1. **Performance**
- Cache frequently accessed paths
- Optimize path resolution
- Index schema hierarchy

### 2. **Security**
- Validate all path traversals
- Prevent directory traversal attacks
- Audit schema access

### 3. **Compatibility**
- Map traditional flat schemas to tree
- Support both syntaxes
- Provide migration tools

This hierarchical schema architecture makes ScratchBird incredibly intuitive while providing powerful organization capabilities that traditional flat-schema databases lack!