# Schema Navigation and Search Path Specification

**Created:** November 26, 2025
**Updated:** November 26, 2025
**Status:** DRAFT
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

1. **Current schema** (set by `SET CURRENT SCHEMA`)
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
SHOW SEARCH PATH;

-- Result:
-- users.alice, public, sys.catalog

-- Set search path for current session
SET SEARCH PATH TO 'users.alice.projects, users.alice, public';

-- Set persistent search path for user
ALTER USER alice SET SEARCH PATH TO 'users.alice, public, myapp';

-- Set search path for role
ALTER ROLE developer SET SEARCH PATH TO 'dev, staging, public';

-- Set client library default (stored in sys.config.client_settings)
-- (typically set via connection string or client config file)

-- Reset to default
SET SEARCH PATH TO DEFAULT;
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
-- Set current schema (absolute path)
SET SCHEMA users.alice.projects;          -- Absolute path from root
SET SCHEMA sys.catalog;                   -- Absolute path to system catalog

-- Set current schema (relative navigation)
SET SCHEMA UP;                            -- Go to parent schema (like cd ..)
SET SCHEMA UP.UP;                         -- Go up two levels
SET SCHEMA subschema;                     -- Go to child schema (relative)
SET SCHEMA UP.sibling;                    -- Go to sibling schema (up then down)
SET SCHEMA UP.UP.other.path;              -- Complex relative navigation

-- Explicit relative path (leading dot)
SET SCHEMA .subschema.deeper;             -- Explicit relative from current
SET SCHEMA .another_child;                -- Explicit relative to child

-- Special locations
SET SCHEMA ROOT;                          -- Go to root schema
SET SCHEMA HOME;                          -- Go to user's home (users.{username})

-- Show current schema
SHOW SCHEMA;                              -- Show current schema name
SHOW SCHEMA PATH;                         -- Show full path (e.g., 'users.alice.projects')
SELECT CURRENT_SCHEMA();                  -- Function form for schema name
SELECT CURRENT_SCHEMA_PATH();             -- Function form for full path
```

### Path Resolution Rules

```
Path Type        | Example                    | Resolution
-----------------|----------------------------|------------------------------------------
Absolute         | sys.catalog.tables         | Start from root, traverse down
Relative (UP)    | UP.sibling                 | Go to parent, then to 'sibling' child
Relative (dot)   | .child.grandchild          | From current schema, traverse down
Child only       | mysubschema                | If child exists, go there; else error
Special          | ROOT, HOME, UP             | Navigate to special location
```

**Examples:**
```sql
-- Current schema: users.alice.projects.webapp

SET SCHEMA UP;                    -- Result: users.alice.projects
SET SCHEMA UP.UP;                 -- Result: users.alice
SET SCHEMA UP.mobile;             -- Result: users.alice.projects.mobile
SET SCHEMA .api;                  -- Result: users.alice.projects.webapp.api
SET SCHEMA sys.catalog;           -- Result: sys.catalog (absolute)
SET SCHEMA HOME;                  -- Result: users.alice
SET SCHEMA ROOT;                  -- Result: / (root)
```

### Listing Objects

```sql
-- List objects in current schema
SHOW OBJECTS;                      -- All objects
SHOW TABLES;                       -- Tables only
SHOW VIEWS;                        -- Views only
SHOW INDEXES;                      -- Indexes only
SHOW SEQUENCES;                    -- Sequences only
SHOW FUNCTIONS;                    -- Functions only
SHOW PROCEDURES;                   -- Procedures only
SHOW TRIGGERS;                     -- Triggers only
SHOW SCHEMAS;                      -- Child schemas only

-- With filtering (LIKE pattern)
SHOW TABLES LIKE 'emp%';
SHOW TABLES LIKE '%_log';
SHOW FUNCTIONS LIKE 'calc_%';

-- With schema path
SHOW TABLES IN sys.catalog;
SHOW TABLES IN remote.emulated.firebird.localhost.employee;

-- Detailed information about specific object
SHOW TABLE employees;              -- Column info for specific table
SHOW TABLE employees IN DETAIL;    -- Extended info including constraints, indexes
SHOW FUNCTION calculate_total;     -- Parameter info for function
SHOW FUNCTION calculate_total IN DETAIL;  -- With source code, dependencies
SHOW INDEX idx_emp_name;           -- Index details
SHOW DOMAIN email_address;         -- Domain definition
SHOW DOMAIN email_address IN DETAIL;  -- With constraints, usage

-- Show schema tree
SHOW SCHEMA TREE;                  -- Show schema tree from current position
SHOW SCHEMA TREE FROM ROOT;        -- Show full tree from root
SHOW SCHEMA TREE DEPTH 2;          -- Limit depth
```

### Object Location Commands

```sql
-- Show where an object is located in search path
SHOW LOCATION OF employees;        -- Find 'employees' in search path
SHOW LOCATION OF TABLE employees;  -- Find table specifically
SHOW LOCATION OF FUNCTION calculate_total;  -- Find function specifically

-- Result:
-- Found 'employees' at:
--   1. users.alice.employees (TABLE)
--   2. public.employees (TABLE)
--   3. remote.emulated.firebird.localhost.hr.EMPLOYEE (SYNONYM)

-- Which object would be resolved by search path?
SHOW RESOLVED employees;           -- Shows which one search path would resolve to
-- Result: users.alice.employees (TABLE)

-- Show full path to object (function form)
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
-- Show object structure (using SHOW with IN DETAIL)
SHOW TABLE employees;              -- Basic table structure
SHOW TABLE employees IN DETAIL;    -- Full details with constraints, indexes, triggers

-- Show schema details
SHOW SCHEMA users.alice;           -- Schema info including object counts
SHOW SCHEMA users.alice IN DETAIL; -- Extended info with permissions, settings

-- PostgreSQL compatibility (optional, in emulation mode)
-- \d employees                    -- Maps to SHOW TABLE employees
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

**Note:** ScratchBird uses a context-aware parser. Only statement-starting keywords need to be reserved (CREATE, ALTER, DROP, SET, SHOW, SELECT, INSERT, UPDATE, DELETE, etc.). Words like SCHEMA, PATH, TABLE, DOMAIN, UP, HOME, ROOT are NOT reserved - the parser knows from context what to expect after each token.

New SQL statements to parse:
- `SET SCHEMA schema_path` - supports absolute, relative (UP.), dot-relative (.path), and special (ROOT, HOME, UP)
- `SHOW SCHEMA` - current schema name
- `SHOW SCHEMA PATH` - full path to current schema
- `SHOW [object_type] [LIKE pattern] [IN schema] [IN DETAIL]`
- `SHOW SCHEMA TREE [FROM ROOT] [DEPTH n]`
- `SET SEARCH PATH TO path_list`
- `SHOW SEARCH PATH`
- `SHOW LOCATION OF [object_type] name`
- `SHOW RESOLVED name`

**Parser Context Examples:**
```
SET SCHEMA ...      -- After SET, parser expects: SCHEMA, SEARCH, or other SET targets
                    -- After SET SCHEMA, parser expects: path | UP | ROOT | HOME
SHOW SCHEMA ...     -- After SHOW, parser expects: SCHEMA, TABLE, TABLES, etc.
                    -- After SHOW SCHEMA, parser expects: PATH | TREE | <nothing> | schema_name
```

Path parsing must handle:
- Absolute paths: `sys.catalog.tables`
- UP-relative paths: `UP`, `UP.UP`, `UP.sibling.child`
- Dot-relative paths: `.child`, `.child.grandchild`
- Special navigation: `ROOT`, `HOME`, `UP` (recognized by position, not reservation)
- Single identifier: checked as child schema first, then as absolute root-level schema

### Executor Changes Needed

New built-in functions:
- `CURRENT_SCHEMA()` - Returns current schema path
- `CURRENT_PATH()` - Returns full path
- `OBJECT_PATH(name)` - Returns full path to named object
- `SCHEMA_PATH(schema_id)` - Returns path for schema ID

---

## Command Summary

### SET Commands (Change State)

| Command | Description | Example |
|---------|-------------|---------|
| `SET SCHEMA path` | Change to schema (absolute) | `SET SCHEMA users.alice` |
| `SET SCHEMA UP` | Go to parent schema | `SET SCHEMA UP` |
| `SET SCHEMA UP.path` | Relative from parent | `SET SCHEMA UP.sibling` |
| `SET SCHEMA .path` | Relative from current | `SET SCHEMA .child.grandchild` |
| `SET SCHEMA child` | Go to child schema | `SET SCHEMA subschema` |
| `SET SCHEMA ROOT` | Go to root | `SET SCHEMA ROOT` |
| `SET SCHEMA HOME` | Go to home | `SET SCHEMA HOME` |
| `SET SEARCH PATH TO` | Set search path | `SET SEARCH PATH TO 'a, b, c'` |

### SHOW Commands (Display Information)

| Command | Description | Example |
|---------|-------------|---------|
| `SHOW SCHEMA` | Show current schema name | `SHOW SCHEMA` |
| `SHOW SCHEMA PATH` | Show full path | `SHOW SCHEMA PATH` |
| `SHOW SEARCH PATH` | Show search path | `SHOW SEARCH PATH` |
| `SHOW TABLES` | List tables | `SHOW TABLES LIKE 'emp%'` |
| `SHOW VIEWS` | List views | `SHOW VIEWS` |
| `SHOW SCHEMAS` | List child schemas | `SHOW SCHEMAS` |
| `SHOW SCHEMA TREE` | Show schema tree | `SHOW SCHEMA TREE DEPTH 3` |
| `SHOW TABLE name` | Table details | `SHOW TABLE employees IN DETAIL` |
| `SHOW DOMAIN name` | Domain details | `SHOW DOMAIN email IN DETAIL` |
| `SHOW LOCATION OF` | Find object locations | `SHOW LOCATION OF TABLE employees` |
| `SHOW RESOLVED` | Show resolved object | `SHOW RESOLVED employees` |

---

## Open Questions

1. **Wildcards in paths** - Should `users.*/projects` match all users' projects for SHOW commands?
2. **Case sensitivity** - Are schema names case-sensitive? (Recommend: case-preserving, case-insensitive comparison)
3. **Path separators** - Use `.` consistently? What about `::` for namespaces?
4. **Auto-completion** - How should tab-completion work in CLI tools?
5. **IN DETAIL output** - What format should detailed output use? (tabular, JSON, both?)

---

## Related Documents

- `SCHEMA_ARCHITECTURE.md` - Hierarchical schema design
- `CATALOG_CLEANUP_PHASE_A_CRUD.md` - CRUD operations
- `CATALOG_CLEANUP_PHASE_B_STRUCTURES.md` - Structure definitions

---

**Document Version:** 1.2
**Last Updated:** November 26, 2025
**Change Log:**
- v1.2: Added relative path support (UP, UP.path, .path), simplified to SET SCHEMA / SHOW SCHEMA
- v1.1: Converted navigation commands from directory-style (USE, CD, PWD) to SQL-style (SET/SHOW)
