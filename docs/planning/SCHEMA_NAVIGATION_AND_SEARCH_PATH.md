# Schema Navigation and Search Path Specification

**Created:** November 26, 2025
**Status:** DRAFT - NEEDS REVIEW
**Purpose:** Define schema navigation commands, search path system, and system table locations

---

## Table of Contents

1. [Complete Schema Tree](#complete-schema-tree)
2. [System Table Locations](#system-table-locations)
3. [Search Path System](#search-path-system)
4. [Navigation Commands](#navigation-commands)
5. [Configuration Settings](#configuration-settings)
6. [Implementation Requirements](#implementation-requirements)

---

## Complete Schema Tree

The full logical schema tree showing where all system objects reside:

```
/ (root)
│
├── sys/                                    # SYSTEM SCHEMA - Core system management
│   │
│   ├── catalog/                            # Core metadata tables
│   │   ├── schemas                         # All schema definitions
│   │   ├── tables                          # All table definitions
│   │   ├── columns                         # All column definitions
│   │   ├── indexes                         # All index definitions
│   │   ├── constraints                     # All constraint definitions
│   │   ├── sequences                       # All sequence definitions
│   │   ├── views                           # All view definitions
│   │   ├── triggers                        # All trigger definitions
│   │   ├── functions                       # All function definitions
│   │   ├── procedures                      # All procedure definitions
│   │   ├── domains                         # User-defined domains
│   │   ├── packages                        # Firebird-style packages
│   │   ├── synonyms                        # All synonym definitions
│   │   ├── dependencies                    # Object dependencies
│   │   ├── comments                        # Object comments/descriptions
│   │   └── statistics                      # Table/column statistics
│   │
│   ├── security/                           # Security and access control
│   │   ├── users                           # User accounts
│   │   ├── roles                           # Role definitions
│   │   ├── groups                          # Group definitions
│   │   ├── role_memberships                # User-to-role mappings
│   │   ├── group_memberships               # User-to-group mappings
│   │   ├── permissions                     # Object permissions
│   │   ├── column_permissions              # Column-level permissions
│   │   ├── policies                        # Row-level security policies
│   │   ├── sessions                        # Active sessions
│   │   ├── audit_log                       # Security audit trail
│   │   └── password_history                # Password change history
│   │
│   ├── storage/                            # Storage management
│   │   ├── tablespaces                     # Tablespace definitions
│   │   ├── tablespace_files                # Tablespace file locations
│   │   ├── page_allocation                 # Page allocation map
│   │   └── toast_tables                    # TOAST table registry
│   │
│   ├── transactions/                       # Transaction management
│   │   ├── active_transactions             # Currently active transactions
│   │   ├── locks                           # Current lock state
│   │   ├── tip_state                       # Transaction state pages
│   │   └── savepoints                      # Active savepoints
│   │
│   ├── config/                             # Configuration settings
│   │   ├── settings                        # Server configuration
│   │   ├── user_settings                   # Per-user settings
│   │   ├── role_settings                   # Per-role settings
│   │   ├── client_settings                 # Client library defaults
│   │   └── search_paths                    # Search path configurations
│   │
│   ├── udr/                                # User-Defined Routines (external)
│   │   ├── engines                         # UDR engine plugins
│   │   ├── modules                         # Loaded UDR modules
│   │   └── registry                        # UDR function registry
│   │
│   ├── replication/                        # Distributed/Replication (Phase 2)
│   │   ├── servers                         # Server registry
│   │   ├── clusters                        # Cluster definitions
│   │   └── replication_slots               # Replication slot state
│   │
│   ├── i18n/                               # Internationalization
│   │   ├── charsets                        # Character set definitions
│   │   ├── collations                      # Collation definitions
│   │   └── timezones                       # Timezone definitions
│   │
│   └── monitoring/                         # Runtime monitoring
│       ├── connections                     # Active connections
│       ├── queries                         # Running queries
│       ├── cache_stats                     # Buffer pool statistics
│       └── io_stats                        # I/O statistics
│
├── information_schema/                     # SQL Standard system views
│   ├── schemata                            # → VIEW to sys.catalog.schemas
│   ├── tables                              # → VIEW to sys.catalog.tables
│   ├── columns                             # → VIEW to sys.catalog.columns
│   ├── table_constraints                   # → VIEW to sys.catalog.constraints
│   ├── key_column_usage                    # → VIEW
│   ├── referential_constraints             # → VIEW
│   ├── views                               # → VIEW to sys.catalog.views
│   ├── routines                            # → VIEW to sys.catalog.functions/procedures
│   ├── parameters                          # → VIEW
│   ├── triggers                            # → VIEW to sys.catalog.triggers
│   ├── sequences                           # → VIEW to sys.catalog.sequences
│   ├── domains                             # → VIEW to sys.catalog.domains
│   └── user_defined_types                  # → VIEW
│
├── users/                                  # User home schemas
│   ├── {username}/                         # Each user's home directory
│   │   ├── [user-created schemas]
│   │   └── temp/                           # User's temp schema (auto-created)
│   └── ...
│
├── remote/                                 # Remote connections
│   │
│   ├── scratchbird/                        # Remote ScratchBird servers
│   │   └── {server_name}/                  # Mounted remote server
│   │       └── [remote schemas visible]
│   │
│   └── emulated/                           # Emulated foreign databases
│       │
│       ├── firebird/                       # Firebird emulation
│       │   └── {server}/
│       │       └── {database}/
│       │           ├── RDB$RELATIONS       # → VIEW to sys.catalog.tables
│       │           ├── RDB$FIELDS          # → VIEW to sys.catalog.columns
│       │           ├── RDB$INDICES         # → VIEW to sys.catalog.indexes
│       │           ├── RDB$TRIGGERS        # → VIEW to sys.catalog.triggers
│       │           ├── RDB$PROCEDURES      # → VIEW to sys.catalog.procedures
│       │           ├── RDB$FUNCTIONS       # → VIEW to sys.catalog.functions
│       │           ├── RDB$GENERATORS      # → VIEW to sys.catalog.sequences
│       │           ├── RDB$EXCEPTIONS      # → VIEW
│       │           ├── RDB$DEPENDENCIES    # → VIEW to sys.catalog.dependencies
│       │           └── [user tables]
│       │
│       ├── postgresql/                     # PostgreSQL emulation
│       │   └── {server}/
│       │       └── {database}/
│       │           ├── pg_catalog/
│       │           │   ├── pg_namespace    # → VIEW to sys.catalog.schemas
│       │           │   ├── pg_class        # → VIEW to sys.catalog.tables
│       │           │   ├── pg_attribute    # → VIEW to sys.catalog.columns
│       │           │   ├── pg_type         # → VIEW
│       │           │   ├── pg_index        # → VIEW to sys.catalog.indexes
│       │           │   ├── pg_constraint   # → VIEW to sys.catalog.constraints
│       │           │   ├── pg_proc         # → VIEW to sys.catalog.functions
│       │           │   ├── pg_trigger      # → VIEW to sys.catalog.triggers
│       │           │   ├── pg_roles        # → VIEW to sys.security.roles
│       │           │   └── pg_user         # → VIEW to sys.security.users
│       │           └── [user schemas]
│       │
│       ├── mysql/                          # MySQL emulation
│       │   └── {server}/
│       │       └── {database}/
│       │           ├── mysql/
│       │           │   ├── user            # → VIEW to sys.security.users
│       │           │   ├── db              # → VIEW
│       │           │   ├── tables_priv     # → VIEW to sys.security.permissions
│       │           │   ├── columns_priv    # → VIEW
│       │           │   ├── proc            # → VIEW to sys.catalog.procedures
│       │           │   └── event           # → VIEW
│       │           └── [user schemas]
│       │
│       └── mssql/                          # SQL Server emulation
│           └── {server}/
│               └── {database}/
│                   ├── sys/
│                   │   ├── schemas         # → VIEW to sys.catalog.schemas
│                   │   ├── tables          # → VIEW to sys.catalog.tables
│                   │   ├── columns         # → VIEW to sys.catalog.columns
│                   │   ├── indexes         # → VIEW to sys.catalog.indexes
│                   │   ├── types           # → VIEW
│                   │   ├── objects         # → VIEW (all objects)
│                   │   ├── procedures      # → VIEW to sys.catalog.procedures
│                   │   ├── sql_modules     # → VIEW
│                   │   └── database_principals  # → VIEW to sys.security.users
│                   └── [user schemas]
│
├── public/                                 # Default public schema
│   └── [shared objects]
│
├── temp/                                   # Global temporary objects
│   └── [temporary tables, etc.]
│
└── [application schemas]/                  # User-created application schemas
    └── ...
```

---

## System Table Locations

### Quick Reference: Where to Find System Tables

| Category | Schema Path | Key Tables |
|----------|-------------|------------|
| **Schemas** | sys.catalog | schemas |
| **Tables** | sys.catalog | tables, columns, constraints |
| **Indexes** | sys.catalog | indexes |
| **Views** | sys.catalog | views |
| **Code** | sys.catalog | functions, procedures, triggers, packages |
| **Sequences** | sys.catalog | sequences |
| **Users** | sys.security | users, roles, groups |
| **Permissions** | sys.security | permissions, column_permissions, policies |
| **Sessions** | sys.security | sessions, audit_log |
| **Storage** | sys.storage | tablespaces, tablespace_files |
| **Transactions** | sys.transactions | active_transactions, locks |
| **Config** | sys.config | settings, search_paths |
| **I18N** | sys.i18n | charsets, collations, timezones |
| **Monitoring** | sys.monitoring | connections, queries, cache_stats |
| **SQL Standard** | information_schema | schemata, tables, columns, etc. |

---

## Search Path System

### Overview

The search path determines the order in which schemas are searched when resolving unqualified object names.

### Search Path Resolution Order

When resolving an unqualified name (e.g., `SELECT * FROM employees`):

1. **Current schema** (set by USE or CD command)
2. **User's personal search path** (user_settings.search_path)
3. **Role's search path** (role_settings.search_path for active roles)
4. **Client library's default path** (client_settings.search_path)
5. **System default path** (settings.default_search_path)
6. **Implicit schemas:**
   - `sys.catalog` (for system table access)
   - `information_schema` (for SQL standard access)
   - `public` (shared objects)

### Search Path Configuration

```sql
-- View current search path
SHOW SEARCH_PATH;

-- Result:
-- users.alice, public, sys.catalog

-- Set search path for current session
SET SEARCH_PATH = 'users.alice.projects, users.alice, public';

-- Set persistent search path for user
ALTER USER alice SET SEARCH_PATH = 'users.alice, public, myapp';

-- Set search path for role
ALTER ROLE developer SET SEARCH_PATH = 'dev, staging, public';

-- Set client library default (stored in sys.config.client_settings)
-- (typically set via connection string or client config file)

-- Reset to default
SET SEARCH_PATH = DEFAULT;
```

### Search Path Storage

```sql
-- sys.config.search_paths table structure
CREATE TABLE sys.config.search_paths (
    path_id         UUID PRIMARY KEY,
    path_type       VARCHAR(20),      -- 'USER', 'ROLE', 'CLIENT', 'SYSTEM'
    entity_id       UUID,             -- user_id, role_id, client_id, or NULL for system
    entity_name     VARCHAR(255),     -- For display
    search_path     TEXT[],           -- Array of schema paths in order
    created_time    TIMESTAMP,
    modified_time   TIMESTAMP
);

-- Examples:
-- ('uuid1', 'USER', 'alice-uuid', 'alice', ['users.alice', 'public'], ...)
-- ('uuid2', 'ROLE', 'dev-uuid', 'developer', ['dev', 'staging', 'public'], ...)
-- ('uuid3', 'SYSTEM', NULL, 'default', ['public', 'sys.catalog'], ...)
```

### Current Schema Tracking

```sql
-- sys.security.sessions includes:
ALTER TABLE sys.security.sessions ADD COLUMN current_schema VARCHAR(1000);
ALTER TABLE sys.security.sessions ADD COLUMN effective_search_path TEXT[];
```

---

## Navigation Commands

### Schema Navigation

```sql
-- Set current schema (like 'cd' in filesystem)
USE schema_name;
USE users.alice.projects;
CD schema_name;                    -- Alias for USE
CD ..;                             -- Go to parent schema
CD /;                              -- Go to root
CD ~;                              -- Go to user's home (users.{username})

-- Show current schema (like 'pwd')
SELECT CURRENT_SCHEMA();
SHOW CURRENT_SCHEMA;
PWD;                               -- Alias

-- Show current path
SELECT CURRENT_PATH();             -- Returns full path like 'users.alice.projects'
```

### Listing Objects

```sql
-- List objects in current schema
SHOW;                              -- All objects
SHOW TABLES;                       -- Tables only
SHOW VIEWS;                        -- Views only
SHOW INDEXES;                      -- Indexes only
SHOW SEQUENCES;                    -- Sequences only
SHOW FUNCTIONS;                    -- Functions only
SHOW PROCEDURES;                   -- Procedures only
SHOW TRIGGERS;                     -- Triggers only
SHOW SCHEMAS;                      -- Child schemas only
SHOW ALL;                          -- Everything including child schemas

-- With filtering (LIKE pattern)
SHOW TABLES LIKE 'emp%';
SHOW TABLES LIKE '%_log';
SHOW FUNCTIONS LIKE 'calc_%';

-- With schema path
SHOW TABLES IN sys.catalog;
SHOW TABLES IN remote.emulated.firebird.localhost.employee;

-- Detailed information
SHOW TABLE employees;              -- Column info for specific table
SHOW FUNCTION calculate_total;     -- Parameter info for function
SHOW INDEX idx_emp_name;           -- Index details

-- Show schema tree
SHOW TREE;                         -- Show schema tree from current position
SHOW TREE FROM /;                  -- Show full tree from root
SHOW TREE DEPTH 2;                 -- Limit depth
```

### Path and Location Commands

```sql
-- Show where an object is located
LOCATE employees;                  -- Find 'employees' in search path
LOCATE TABLE employees;            -- Find table specifically
LOCATE FUNCTION calculate_total;   -- Find function specifically

-- Result:
-- Found 'employees' at:
--   1. users.alice.employees (TABLE)
--   2. public.employees (TABLE)
--   3. remote.emulated.firebird.localhost.hr.EMPLOYEE (SYNONYM)

-- Which object would be used?
WHICH employees;                   -- Shows which one search path would resolve to
-- Result: users.alice.employees (TABLE)

-- Show full path to object
SELECT OBJECT_PATH('employees');   -- Returns 'users.alice.employees'
```

### Information Commands

```sql
-- Database/server information
SHOW DATABASES;                    -- List available databases
SHOW SERVERS;                      -- List configured emulation servers
SHOW CONNECTIONS;                  -- Active connections
SHOW SESSIONS;                     -- Active sessions

-- Configuration
SHOW SETTINGS;                     -- All settings
SHOW SETTING search_path;          -- Specific setting
SHOW USER;                         -- Current user info
SHOW ROLES;                        -- Current user's roles
SHOW PRIVILEGES;                   -- Current effective privileges

-- Statistics
SHOW STATISTICS FOR employees;     -- Table statistics
SHOW LOCKS;                        -- Current locks
SHOW TRANSACTIONS;                 -- Active transactions
```

### Object Description

```sql
-- Describe object structure
DESCRIBE employees;                -- Same as \d in psql
DESC employees;                    -- Alias
\d employees;                      -- PostgreSQL-style

-- Extended description
DESCRIBE FULL employees;           -- Include storage, statistics
DESCRIBE EXTENDED employees;       -- Include constraints, indexes, triggers

-- Describe schema
DESCRIBE SCHEMA users.alice;       -- Schema details including object counts
```

---

## Configuration Settings

### Search Path Settings

```sql
-- System default search path
-- sys.config.settings
INSERT INTO sys.config.settings (name, value, description) VALUES
('default_search_path', 'public, sys.catalog', 'Default search path for new sessions');

-- Per-user settings
-- sys.config.user_settings
CREATE TABLE sys.config.user_settings (
    user_id         UUID REFERENCES sys.security.users(user_id),
    setting_name    VARCHAR(255),
    setting_value   TEXT,
    PRIMARY KEY (user_id, setting_name)
);

-- Per-role settings
-- sys.config.role_settings
CREATE TABLE sys.config.role_settings (
    role_id         UUID REFERENCES sys.security.roles(role_id),
    setting_name    VARCHAR(255),
    setting_value   TEXT,
    PRIMARY KEY (role_id, setting_name)
);

-- Client library settings
-- sys.config.client_settings
CREATE TABLE sys.config.client_settings (
    client_id       UUID PRIMARY KEY,
    client_name     VARCHAR(255),        -- 'libpq', 'jdbc', 'odbc', etc.
    client_version  VARCHAR(50),
    default_search_path TEXT[],
    default_schema  VARCHAR(1000),
    other_settings  JSONB
);
```

### User Home Directory Settings

```sql
-- sys.security.users extended:
ALTER TABLE sys.security.users ADD COLUMN home_schema VARCHAR(1000);
-- Default: 'users.' || username

-- Auto-create home schema on user creation
-- Trigger: after INSERT on sys.security.users
--   CREATE SCHEMA users.{username};
--   CREATE SCHEMA users.{username}.temp;
--   UPDATE sys.security.users SET home_schema = 'users.' || username;
```

### Session Settings

```sql
-- sys.security.sessions extended:
ALTER TABLE sys.security.sessions ADD COLUMN (
    current_schema      VARCHAR(1000),       -- Current working schema
    effective_search_path TEXT[],            -- Computed search path
    session_settings    JSONB                -- Session-specific settings
);
```

---

## Implementation Requirements

### New Structures Needed

```cpp
// In catalog_manager.h

// Search path entry
struct SearchPathEntry {
    ID path_id;
    std::string path_type;      // "USER", "ROLE", "CLIENT", "SYSTEM"
    ID entity_id;               // user_id, role_id, client_id, or zero
    std::string entity_name;
    std::vector<std::string> search_path;
    uint64_t created_time;
    uint64_t modified_time;
};

// User settings entry
struct UserSettingEntry {
    ID user_id;
    std::string setting_name;
    std::string setting_value;
};

// Session with navigation state
struct SessionInfo {
    // ... existing fields ...
    std::string current_schema;           // Current working schema path
    std::vector<std::string> search_path; // Effective search path
    std::unordered_map<std::string, std::string> session_settings;
};
```

### New CRUD Methods Needed

```cpp
// Search path management
auto setSearchPath(const ID& entity_id, const std::string& path_type,
                   const std::vector<std::string>& paths,
                   ErrorContext* ctx = nullptr) -> Status;
auto getSearchPath(const ID& entity_id, const std::string& path_type,
                   std::vector<std::string>& paths_out,
                   ErrorContext* ctx = nullptr) -> Status;
auto computeEffectiveSearchPath(const ID& session_id,
                                std::vector<std::string>& paths_out,
                                ErrorContext* ctx = nullptr) -> Status;

// Object resolution
auto resolveObjectName(const std::string& name, ObjectType type_hint,
                       const std::vector<std::string>& search_path,
                       ID& object_id_out, std::string& full_path_out,
                       ErrorContext* ctx = nullptr) -> Status;
auto locateObject(const std::string& name, ObjectType type_hint,
                  const std::vector<std::string>& search_path,
                  std::vector<std::pair<std::string, ObjectType>>& locations_out,
                  ErrorContext* ctx = nullptr) -> Status;

// Navigation
auto setCurrentSchema(const ID& session_id, const std::string& schema_path,
                      ErrorContext* ctx = nullptr) -> Status;
auto getCurrentSchema(const ID& session_id, std::string& schema_path_out,
                      ErrorContext* ctx = nullptr) -> Status;
auto navigateToParent(const ID& session_id, ErrorContext* ctx = nullptr) -> Status;
auto navigateToHome(const ID& session_id, ErrorContext* ctx = nullptr) -> Status;
auto navigateToRoot(const ID& session_id, ErrorContext* ctx = nullptr) -> Status;

// Listing
auto listObjectsInSchema(const ID& schema_id, ObjectType type_filter,
                         const std::string& name_pattern,
                         std::vector<ObjectInfo>& objects_out,
                         ErrorContext* ctx = nullptr) -> Status;
auto getSchemaTree(const ID& start_schema_id, int max_depth,
                   SchemaTreeNode& tree_out,
                   ErrorContext* ctx = nullptr) -> Status;
```

### Parser Changes Needed

New SQL statements to parse:
- `USE schema_path`
- `CD schema_path`
- `SHOW [object_type] [LIKE pattern] [IN schema]`
- `SET SEARCH_PATH = path_list`
- `LOCATE [object_type] name`
- `WHICH name`
- `DESCRIBE [FULL|EXTENDED] object`
- `PWD`

### Executor Changes Needed

New built-in functions:
- `CURRENT_SCHEMA()` - Returns current schema path
- `CURRENT_PATH()` - Returns full path
- `OBJECT_PATH(name)` - Returns full path to named object
- `SCHEMA_PATH(schema_id)` - Returns path for schema ID

---

## Command Summary

| Command | Description | Example |
|---------|-------------|---------|
| `USE` / `CD` | Change current schema | `USE users.alice` |
| `CD ..` | Go to parent schema | `CD ..` |
| `CD /` | Go to root | `CD /` |
| `CD ~` | Go to home | `CD ~` |
| `PWD` | Show current schema | `PWD` |
| `SHOW` | List objects | `SHOW TABLES LIKE 'emp%'` |
| `SHOW TREE` | Show schema tree | `SHOW TREE DEPTH 3` |
| `LOCATE` | Find object locations | `LOCATE TABLE employees` |
| `WHICH` | Show resolved object | `WHICH employees` |
| `DESCRIBE` | Object details | `DESCRIBE FULL employees` |
| `SET SEARCH_PATH` | Set search path | `SET SEARCH_PATH = 'a, b, c'` |
| `SHOW SEARCH_PATH` | Show search path | `SHOW SEARCH_PATH` |

---

## Open Questions

1. **Relative paths** - Should we support `./subschema` and `../sibling`?
2. **Wildcards in paths** - Should `users.*/projects` match all users' projects?
3. **Case sensitivity** - Are schema names case-sensitive? (Recommend: case-preserving, case-insensitive comparison)
4. **Path separators** - Use `.` consistently? What about `::` for namespaces?
5. **Auto-completion** - How should tab-completion work in CLI tools?

---

## Related Documents

- `SCHEMA_ARCHITECTURE.md` - Hierarchical schema design
- `CATALOG_CLEANUP_PHASE_A_CRUD.md` - CRUD operations
- `CATALOG_CLEANUP_PHASE_B_STRUCTURES.md` - Structure definitions

---

**Document Version:** 1.0
**Last Updated:** November 26, 2025
