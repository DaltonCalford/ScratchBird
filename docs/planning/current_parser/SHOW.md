# SHOW Commands - Complete Reference

This document provides a comprehensive audit of all SHOW commands implemented in the ScratchBird parser.

**Source:** `/home/dcalford/CliWork/ScratchBird/src/parser/parser.cpp` (lines 4907-5466)

## Overview

The ScratchBird parser implements an extensive set of SHOW commands for database introspection and developer experience. These commands are divided into three categories:

1. **Basic SHOW Commands** (Alpha 1) - MySQL/PostgreSQL style commands
2. **Extended SHOW Commands** - Firebird ISQL compatibility commands
3. **Schema Navigation Commands** - Hierarchical schema navigation

## Contextual vs Reserved Keywords

The parser uses a sophisticated system to handle keywords that can be both SQL keywords and user-defined identifiers:

- **Reserved Keywords**: TABLES, DATABASES, COLUMNS, INDEXES, CREATE, TABLE, INDEX, TRIGGER, PROCEDURE, FUNCTION, VIEW, DOMAIN, GENERATOR, SEQUENCE, SCHEMA, ROLE, GRANTS, CHECKS, COLLATIONS, COMMENTS, DEPENDENCIES, PACKAGE, SYSTEM, SQL, DIALECT, VERSION, DATABASE, LOCATION, FROM, LIKE, IN, FOR

- **Contextual Keywords** (NOT reserved, lexed as IDENTIFIER): PATH, TREE, DEPTH, DETAIL, SEARCH, OF, RESOLVED, OBJECTS, ROOT

The parser uses helper lambdas `matchContextual()` and `checkContextual()` to handle contextual keywords without making them reserved words, avoiding conflicts with common column names.

---

## 1. Basic SHOW Commands (Alpha 1)

### 1.1 SHOW TABLES

**Syntax:**
```bnf
SHOW TABLES
    [ FROM schema_path ]
    [ IN schema_path | IN PATH ]
    [ LIKE 'pattern' ]
    [ IN DETAIL ]
```

**Description:** Lists all tables in the database or specified schema.

**Options:**
- `FROM schema_path` - Legacy syntax for specifying schema (treated as `IN schema_path`)
- `IN schema_path` - Show tables in a specific schema (dot-separated path)
- `IN PATH` - Show tables in all schemas in the current search path
- `LIKE 'pattern'` - Filter table names by SQL pattern (% and _ wildcards)
- `IN DETAIL` - Show extended table information

**Examples:**
```sql
SHOW TABLES;
SHOW TABLES LIKE 'user%';
SHOW TABLES IN public;
SHOW TABLES IN PATH;
SHOW TABLES IN company.sales;
SHOW TABLES FROM mydb LIKE '%_archive' IN DETAIL;
SHOW TABLES IN PATH LIKE 'tmp_%';
```

**Object Type:** `ShowObjectType::TABLES`

**Fields Used:**
- `object_type_` = TABLES
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `like_pattern_` = pattern (if LIKE specified)
- `in_detail_` = true/false

---

### 1.2 SHOW DATABASES

**Syntax:**
```bnf
SHOW DATABASES
    [ LIKE 'pattern' ]
```

**Description:** Lists all databases (schemas) available.

**Options:**
- `LIKE 'pattern'` - Filter database names by SQL pattern

**Examples:**
```sql
SHOW DATABASES;
SHOW DATABASES LIKE 'test%';
```

**Object Type:** `ShowObjectType::DATABASES`

**Fields Used:**
- `object_type_` = DATABASES
- `like_pattern_` = pattern (if LIKE specified)

**Note:** SHOW DATABASES and SHOW SCHEMAS are synonyms.

---

### 1.3 SHOW COLUMNS

**Syntax:**
```bnf
SHOW COLUMNS FROM table_name
    [ LIKE 'pattern' ]
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Lists all columns in a table.

**Options:**
- `FROM table_name` - **Required**. Can be qualified (schema.table)
- `LIKE 'pattern'` - Filter column names by SQL pattern
- `IN schema_path` - Specify schema for unqualified table name
- `IN PATH` - Search for table in search path
- `IN DETAIL` - Show extended column information

**Examples:**
```sql
SHOW COLUMNS FROM users;
SHOW COLUMNS FROM users LIKE 'email%';
SHOW COLUMNS FROM public.employees;
SHOW COLUMNS FROM orders IN DETAIL;
```

**Object Type:** `ShowObjectType::COLUMNS`

**Fields Used:**
- `object_type_` = COLUMNS
- `object_name_` = table name (possibly qualified)
- `like_pattern_` = pattern (if LIKE specified)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

**Note:** `DESCRIBE table_name` and `DESC table_name` are aliases for `SHOW COLUMNS FROM table_name`.

---

### 1.4 SHOW INDEXES

**Syntax:**
```bnf
SHOW INDEXES FROM table_name
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Lists all indexes on a table.

**Options:**
- `FROM table_name` - **Required**. Can be qualified (schema.table)
- `IN schema_path` - Specify schema for unqualified table name
- `IN PATH` - Search for table in search path
- `IN DETAIL` - Show extended index information

**Examples:**
```sql
SHOW INDEXES FROM users;
SHOW INDEXES FROM public.employees;
SHOW INDEXES FROM orders IN DETAIL;
```

**Object Type:** `ShowObjectType::INDEXES`

**Fields Used:**
- `object_type_` = INDEXES
- `object_name_` = table name (possibly qualified)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

---

### 1.5 SHOW CREATE TABLE

**Syntax:**
```bnf
SHOW CREATE TABLE table_name
```

**Description:** Shows the CREATE TABLE statement for a table.

**Options:**
- `table_name` - **Required**. Can be qualified (schema.table)

**Examples:**
```sql
SHOW CREATE TABLE users;
SHOW CREATE TABLE public.employees;
```

**Object Type:** `ShowObjectType::CREATE_TABLE`

**Fields Used:**
- `object_type_` = CREATE_TABLE
- `object_name_` = table name (possibly qualified)

---

## 2. Extended SHOW Commands (Firebird ISQL Compatibility)

These commands follow the pattern: `SHOW object_type [name] [modifiers]`

### 2.1 SHOW TABLE

**Syntax:**
```bnf
SHOW TABLE [ name ]
    [ IN schema_path | IN PATH ]
    [ LIKE 'pattern' ]
    [ IN DETAIL ]
```

**Description:** Shows detailed table structure. If name is omitted, lists all tables.

**Options:**
- `name` - Optional table name (if omitted, lists all tables)
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `LIKE 'pattern'` - Filter by pattern (when name omitted)
- `IN DETAIL` - Show extended information

**Examples:**
```sql
SHOW TABLE;                    -- List all tables
SHOW TABLE users;              -- Show structure of users table
SHOW TABLE IN public;          -- List tables in public schema
SHOW TABLE users IN DETAIL;    -- Show detailed structure
SHOW TABLE LIKE 'tmp%';        -- List tables matching pattern
```

**Object Type:** `ShowObjectType::TABLE`

**Fields Used:**
- `object_type_` = TABLE
- `object_name_` = table name (optional)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `like_pattern_` = pattern (if LIKE specified)
- `in_detail_` = true/false

**Note:** When `name` is provided, shows structure similar to Firebird ISQL. When omitted, lists tables.

---

### 2.2 SHOW INDEX

**Syntax:**
```bnf
SHOW INDEX [ name ]
    [ IN schema_path | IN PATH ]
    [ LIKE 'pattern' ]
    [ IN DETAIL ]
```

**Description:** Shows detailed index information. If name is omitted, lists all indexes.

**Options:**
- `name` - Optional index name
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `LIKE 'pattern'` - Filter by pattern (when name omitted)
- `IN DETAIL` - Show extended information

**Examples:**
```sql
SHOW INDEX;                    -- List all indexes
SHOW INDEX idx_users_email;    -- Show index details
SHOW INDEX IN public;          -- List indexes in public schema
SHOW INDEX LIKE 'idx_user%';   -- List indexes matching pattern
```

**Object Type:** `ShowObjectType::INDEX`

**Fields Used:**
- `object_type_` = INDEX
- `object_name_` = index name (optional)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `like_pattern_` = pattern (if LIKE specified)
- `in_detail_` = true/false

---

### 2.3 SHOW TRIGGER

**Syntax:**
```bnf
SHOW TRIGGER [ name ]
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Shows trigger definitions. If name is omitted, lists all triggers.

**Options:**
- `name` - Optional trigger name
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `IN DETAIL` - Show extended information including trigger body

**Examples:**
```sql
SHOW TRIGGER;                       -- List all triggers
SHOW TRIGGER trg_audit_insert;      -- Show trigger definition
SHOW TRIGGER IN public;             -- List triggers in public schema
SHOW TRIGGER trg_audit_insert IN DETAIL;  -- Show full definition
```

**Object Type:** `ShowObjectType::TRIGGER`

**Fields Used:**
- `object_type_` = TRIGGER
- `object_name_` = trigger name (optional)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

---

### 2.4 SHOW PROCEDURE

**Syntax:**
```bnf
SHOW PROCEDURE [ name ]
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Shows stored procedure definitions. If name is omitted, lists all procedures.

**Options:**
- `name` - Optional procedure name
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `IN DETAIL` - Show extended information including procedure body

**Examples:**
```sql
SHOW PROCEDURE;                     -- List all procedures
SHOW PROCEDURE calculate_discount;  -- Show procedure definition
SHOW PROCEDURE IN public;           -- List procedures in public schema
SHOW PROCEDURE calculate_discount IN DETAIL;  -- Show full definition
```

**Object Type:** `ShowObjectType::PROCEDURE`

**Fields Used:**
- `object_type_` = PROCEDURE
- `object_name_` = procedure name (optional)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

---

### 2.5 SHOW FUNCTION

**Syntax:**
```bnf
SHOW FUNCTION [ name ]
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Shows user-defined function definitions. If name is omitted, lists all functions.

**Options:**
- `name` - Optional function name
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `IN DETAIL` - Show extended information including function body

**Examples:**
```sql
SHOW FUNCTION;                      -- List all functions
SHOW FUNCTION validate_email;       -- Show function definition
SHOW FUNCTION IN public;            -- List functions in public schema
SHOW FUNCTION validate_email IN DETAIL;  -- Show full definition
```

**Object Type:** `ShowObjectType::FUNCTION`

**Fields Used:**
- `object_type_` = FUNCTION
- `object_name_` = function name (optional)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

---

### 2.6 SHOW VIEW

**Syntax:**
```bnf
SHOW VIEW [ name ]
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Shows view definitions. If name is omitted, lists all views.

**Options:**
- `name` - Optional view name
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `IN DETAIL` - Show extended information including view definition

**Examples:**
```sql
SHOW VIEW;                          -- List all views
SHOW VIEW active_users;             -- Show view definition
SHOW VIEW IN public;                -- List views in public schema
SHOW VIEW active_users IN DETAIL;   -- Show full definition
```

**Object Type:** `ShowObjectType::VIEW`

**Fields Used:**
- `object_type_` = VIEW
- `object_name_` = view name (optional)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

---

### 2.7 SHOW DOMAIN

**Syntax:**
```bnf
SHOW DOMAIN [ name ]
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Shows domain (user-defined type) definitions. If name is omitted, lists all domains.

**Options:**
- `name` - Optional domain name
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `IN DETAIL` - Show extended information including constraints

**Examples:**
```sql
SHOW DOMAIN;                        -- List all domains
SHOW DOMAIN email_address;          -- Show domain definition
SHOW DOMAIN IN public;              -- List domains in public schema
SHOW DOMAIN email_address IN DETAIL;  -- Show full definition
```

**Object Type:** `ShowObjectType::DOMAIN`

**Fields Used:**
- `object_type_` = DOMAIN
- `object_name_` = domain name (optional)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

---

### 2.8 SHOW GENERATOR / SHOW SEQUENCE

**Syntax:**
```bnf
SHOW GENERATOR [ name ]
    [ IN schema_path | IN PATH | IN DETAIL ]

SHOW SEQUENCE [ name ]
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Shows sequence/generator definitions. GENERATOR (Firebird) and SEQUENCE (PostgreSQL/SQL Standard) are synonyms.

**Options:**
- `name` - Optional sequence name
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `IN DETAIL` - Show extended information

**Examples:**
```sql
SHOW GENERATOR;                     -- List all sequences
SHOW SEQUENCE user_id_seq;          -- Show sequence definition
SHOW GENERATOR order_id_gen;        -- Firebird style
SHOW SEQUENCE IN public;            -- List sequences in public schema
SHOW SEQUENCE user_id_seq IN DETAIL;  -- Show full definition
```

**Object Type:** `ShowObjectType::GENERATOR`

**Fields Used:**
- `object_type_` = GENERATOR
- `object_name_` = sequence name (optional)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

---

### 2.9 SHOW SCHEMA

**Syntax:**
```bnf
SHOW SCHEMA [ name ]
    [ IN DETAIL ]
```

**Description:** Shows schema definition. If name is omitted, shows current schema name.

**Options:**
- `name` - Optional schema name
- `IN DETAIL` - Show extended schema information

**Examples:**
```sql
SHOW SCHEMA;                        -- Show current schema name
SHOW SCHEMA public;                 -- Show public schema details
SHOW SCHEMA company.sales IN DETAIL;  -- Show detailed schema info
```

**Object Type:** `ShowObjectType::SCHEMA`

**Fields Used:**
- `object_type_` = SCHEMA
- `object_name_` = schema name (optional)
- `in_detail_` = true/false

**Note:** See also SHOW SCHEMA PATH and SHOW SCHEMA TREE for navigation commands.

---

### 2.10 SHOW ROLE

**Syntax:**
```bnf
SHOW ROLE [ name ]
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Shows role definitions. If name is omitted, lists all roles or shows current role.

**Options:**
- `name` - Optional role name
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `IN DETAIL` - Show extended role information including members

**Examples:**
```sql
SHOW ROLE;                          -- List all roles or show current
SHOW ROLE admin;                    -- Show admin role details
SHOW ROLE IN DETAIL;                -- List all roles with details
```

**Object Type:** `ShowObjectType::ROLE`

**Fields Used:**
- `object_type_` = ROLE
- `object_name_` = role name (optional)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

---

### 2.11 SHOW GRANTS

**Syntax:**
```bnf
SHOW GRANTS [ FOR object_name ]
    [ IN schema_path | IN PATH | IN DETAIL ]

SHOW GRANTS [ object_name ]
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Shows privilege grants. Can show grants for a specific object or all grants.

**Options:**
- `FOR object_name` - Show grants for specific object
- `object_name` - Alternative syntax without FOR
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `IN DETAIL` - Show extended grant information

**Examples:**
```sql
SHOW GRANTS;                        -- Show all grants
SHOW GRANTS FOR users;              -- Show grants on users table
SHOW GRANTS users;                  -- Alternative syntax
SHOW GRANTS IN public;              -- Show grants in public schema
```

**Object Type:** `ShowObjectType::GRANTS`

**Fields Used:**
- `object_type_` = GRANTS
- `object_name_` = object name (optional)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

---

### 2.12 SHOW CHECKS

**Syntax:**
```bnf
SHOW CHECKS [ table_name ]
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Shows check constraints. If table_name is omitted, shows all check constraints.

**Options:**
- `table_name` - Optional table name
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `IN DETAIL` - Show extended constraint information

**Examples:**
```sql
SHOW CHECKS;                        -- Show all check constraints
SHOW CHECKS users;                  -- Show checks on users table
SHOW CHECKS IN public;              -- Show checks in public schema
SHOW CHECKS users IN DETAIL;        -- Show detailed constraint info
```

**Object Type:** `ShowObjectType::CHECKS`

**Fields Used:**
- `object_type_` = CHECKS
- `object_name_` = table name (optional)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

---

### 2.13 SHOW COLLATIONS

**Syntax:**
```bnf
SHOW COLLATIONS
    [ LIKE 'pattern' ]
```

**Description:** Shows available collation sequences.

**Options:**
- `LIKE 'pattern'` - Filter collation names by pattern

**Examples:**
```sql
SHOW COLLATIONS;                    -- Show all collations
SHOW COLLATIONS LIKE 'utf8%';       -- Show UTF-8 collations
```

**Object Type:** `ShowObjectType::COLLATIONS`

**Fields Used:**
- `object_type_` = COLLATIONS
- `like_pattern_` = pattern (if LIKE specified)

---

### 2.14 SHOW COMMENTS

**Syntax:**
```bnf
SHOW COMMENTS [ object_name ]
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Shows object comments/descriptions. If object_name is omitted, shows all comments.

**Options:**
- `object_name` - Optional object name
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `IN DETAIL` - Show extended information

**Examples:**
```sql
SHOW COMMENTS;                      -- Show all comments
SHOW COMMENTS users;                -- Show comments for users table
SHOW COMMENTS IN public;            -- Show comments in public schema
```

**Object Type:** `ShowObjectType::COMMENTS`

**Fields Used:**
- `object_type_` = COMMENTS
- `object_name_` = object name (optional)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

---

### 2.15 SHOW DEPENDENCIES

**Syntax:**
```bnf
SHOW DEPENDENCIES [ object_name ]
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Shows object dependency graph. If object_name is omitted, shows all dependencies.

**Options:**
- `object_name` - Optional object name
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `IN DETAIL` - Show extended dependency information

**Examples:**
```sql
SHOW DEPENDENCIES;                  -- Show all dependencies
SHOW DEPENDENCIES active_users;     -- Show dependencies for view
SHOW DEPENDENCIES IN public;        -- Show dependencies in public schema
```

**Object Type:** `ShowObjectType::DEPENDENCIES`

**Fields Used:**
- `object_type_` = DEPENDENCIES
- `object_name_` = object name (optional)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

---

### 2.16 SHOW PACKAGE

**Syntax:**
```bnf
SHOW PACKAGE [ name ]
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Shows package definitions (Firebird/Oracle style packages).

**Options:**
- `name` - Optional package name
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `IN DETAIL` - Show extended package information including body

**Examples:**
```sql
SHOW PACKAGE;                       -- List all packages
SHOW PACKAGE utils;                 -- Show package definition
SHOW PACKAGE IN public;             -- List packages in public schema
SHOW PACKAGE utils IN DETAIL;       -- Show full package definition
```

**Object Type:** `ShowObjectType::PACKAGE`

**Fields Used:**
- `object_type_` = PACKAGE
- `object_name_` = package name (optional)
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

---

### 2.17 SHOW SYSTEM

**Syntax:**
```bnf
SHOW SYSTEM
    [ IN schema_path | IN PATH | IN DETAIL ]
```

**Description:** Shows system tables and views.

**Options:**
- `IN schema_path` - Specify schema to search
- `IN PATH` - Search in all schemas in search path
- `IN DETAIL` - Show extended system information

**Examples:**
```sql
SHOW SYSTEM;                        -- Show system tables/views
SHOW SYSTEM IN DETAIL;              -- Show detailed system info
```

**Object Type:** `ShowObjectType::SYSTEM`

**Fields Used:**
- `object_type_` = SYSTEM
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `in_detail_` = true/false

---

### 2.18 SHOW SQL DIALECT

**Syntax:**
```bnf
SHOW SQL DIALECT
```

**Description:** Shows the current SQL dialect setting (Firebird ISQL compatibility).

**Examples:**
```sql
SHOW SQL DIALECT;
```

**Object Type:** `ShowObjectType::SQL_DIALECT`

**Fields Used:**
- `object_type_` = SQL_DIALECT

**Note:** Firebird supports SQL dialects 1, 2, and 3. This is for ISQL compatibility.

---

### 2.19 SHOW VERSION

**Syntax:**
```bnf
SHOW VERSION
```

**Description:** Shows server version information.

**Examples:**
```sql
SHOW VERSION;
```

**Object Type:** `ShowObjectType::VERSION`

**Fields Used:**
- `object_type_` = VERSION

---

### 2.20 SHOW DATABASE

**Syntax:**
```bnf
SHOW DATABASE
```

**Description:** Shows current database metadata and connection information.

**Examples:**
```sql
SHOW DATABASE;
```

**Object Type:** `ShowObjectType::DATABASE`

**Fields Used:**
- `object_type_` = DATABASE

---

## 3. Schema Navigation Commands

These commands support hierarchical schema navigation and search path resolution.

### 3.1 SHOW SCHEMA PATH

**Syntax:**
```bnf
SHOW SCHEMA PATH
```

**Description:** Shows the full path to the current schema in the schema hierarchy.

**Examples:**
```sql
SHOW SCHEMA PATH;
-- Output: /company/sales/orders
```

**Object Type:** `ShowObjectType::SCHEMA_PATH`

**Fields Used:**
- `object_type_` = SCHEMA_PATH

**Contextual Keywords:** PATH

---

### 3.2 SHOW SCHEMA TREE

**Syntax:**
```bnf
SHOW SCHEMA TREE
    [ DEPTH n ]
    [ FROM ROOT ]
```

**Description:** Shows the schema hierarchy as a tree structure.

**Options:**
- `DEPTH n` - Limit tree depth to n levels (0 or omitted = unlimited)
- `FROM ROOT` - Start from root schema (default: current schema)

**Examples:**
```sql
SHOW SCHEMA TREE;                   -- Show tree from current schema
SHOW SCHEMA TREE DEPTH 2;           -- Show 2 levels deep
SHOW SCHEMA TREE FROM ROOT;         -- Show entire schema hierarchy
SHOW SCHEMA TREE DEPTH 3 FROM ROOT; -- Show 3 levels from root
```

**Object Type:** `ShowObjectType::SCHEMA_TREE`

**Fields Used:**
- `object_type_` = SCHEMA_TREE
- `tree_depth_` = depth limit (0 = unlimited)
- `schema_path_` = "/" if FROM ROOT specified

**Contextual Keywords:** TREE, DEPTH, ROOT

---

### 3.3 SHOW SEARCH PATH

**Syntax:**
```bnf
SHOW SEARCH PATH
```

**Description:** Shows the current schema search path (list of schemas searched for unqualified names).

**Examples:**
```sql
SHOW SEARCH PATH;
-- Output: public, company.sales, company.hr
```

**Object Type:** `ShowObjectType::SEARCH_PATH`

**Fields Used:**
- `object_type_` = SEARCH_PATH

**Contextual Keywords:** SEARCH, PATH

**Note:** Related to `SET SEARCH PATH` command.

---

### 3.4 SHOW LOCATION OF

**Syntax:**
```bnf
SHOW LOCATION OF [ object_type ] object_name
```

**Description:** Finds where an object exists in the schema search path.

**Options:**
- `object_type` - Optional object type qualifier (TABLE, VIEW, FUNCTION, PROCEDURE, SEQUENCE, INDEX, TRIGGER, DOMAIN)
- `object_name` - **Required**. Name of object to locate

**Examples:**
```sql
SHOW LOCATION OF users;             -- Find users table/view/etc
SHOW LOCATION OF TABLE users;       -- Find specifically a table named users
SHOW LOCATION OF FUNCTION validate; -- Find function named validate
SHOW LOCATION OF INDEX idx_email;   -- Find index named idx_email
```

**Object Type:** `ShowObjectType::LOCATION`

**Fields Used:**
- `object_type_` = LOCATION
- `object_name_` = object name to locate
- `database_name_` = object type qualifier (repurposed field)

**Contextual Keywords:** LOCATION (keyword), OF (contextual)

**Note:** Searches through all schemas in the search path and reports where the object is found.

---

### 3.5 SHOW RESOLVED

**Syntax:**
```bnf
SHOW RESOLVED object_name
```

**Description:** Shows which object the search path resolves to for a given name (i.e., which schema's version would be used).

**Options:**
- `object_name` - **Required**. Name to resolve

**Examples:**
```sql
SHOW RESOLVED users;                -- Which users table would be used?
SHOW RESOLVED calculate_tax;        -- Which function would be called?
```

**Object Type:** `ShowObjectType::RESOLVED`

**Fields Used:**
- `object_type_` = RESOLVED
- `object_name_` = name to resolve

**Contextual Keywords:** RESOLVED

**Note:** Unlike SHOW LOCATION OF, this shows only the first match in search path order (the one that would actually be used).

---

### 3.6 SHOW OBJECTS

**Syntax:**
```bnf
SHOW OBJECTS
    [ IN schema_path | IN PATH ]
    [ LIKE 'pattern' ]
    [ IN DETAIL ]
```

**Description:** Shows all objects in the current schema or specified scope.

**Options:**
- `IN schema_path` - Show objects in specific schema
- `IN PATH` - Show objects in all schemas in search path
- `LIKE 'pattern'` - Filter object names by pattern
- `IN DETAIL` - Show extended object information

**Examples:**
```sql
SHOW OBJECTS;                       -- Show all objects in current schema
SHOW OBJECTS IN public;             -- Show objects in public schema
SHOW OBJECTS IN PATH;               -- Show objects in search path
SHOW OBJECTS LIKE 'user%';          -- Filter by pattern
SHOW OBJECTS IN PATH IN DETAIL;     -- Detailed listing
```

**Object Type:** `ShowObjectType::OBJECTS`

**Fields Used:**
- `object_type_` = OBJECTS
- `schema_scope_` = CURRENT | IN_PATH | IN_SCHEMA
- `schema_path_` = schema path (if IN_SCHEMA)
- `like_pattern_` = pattern (if LIKE specified)
- `in_detail_` = true/false

**Contextual Keywords:** OBJECTS

---

## 4. Common Clauses and Modifiers

### 4.1 Schema Scope Clauses

These clauses control where the SHOW command searches for objects:

**IN schema_path**
- Searches in a specific schema (can be dot-separated hierarchical path)
- Sets `schema_scope_` = `ShowSchemaScope::IN_SCHEMA`
- Sets `schema_path_` to the specified path
- Example: `IN public`, `IN company.sales.orders`

**IN PATH**
- Searches in all schemas in the current search path
- Sets `schema_scope_` = `ShowSchemaScope::IN_PATH`
- Contextual keyword PATH (not reserved)
- Example: `SHOW TABLES IN PATH`

**Default (no IN clause)**
- Searches in current schema only
- Sets `schema_scope_` = `ShowSchemaScope::CURRENT`

**Multiple IN clauses:**
- Can appear before and after other clauses (LIKE, etc.)
- Last one wins
- Example: `SHOW TABLES IN public LIKE 'user%' IN DETAIL`

---

### 4.2 IN DETAIL Clause

**Syntax:** `IN DETAIL`

**Description:** Shows extended information about objects
- Contextual keyword DETAIL (not reserved)
- Sets `in_detail_` = true
- Can be combined with other IN clauses

**Examples:**
```sql
SHOW TABLE users IN DETAIL;
SHOW TABLES IN public IN DETAIL;
SHOW FUNCTION validate_email IN DETAIL;
```

---

### 4.3 LIKE Clause

**Syntax:** `LIKE 'pattern'`

**Description:** Filters results by SQL pattern matching
- Uses SQL wildcards: `%` (any characters), `_` (single character)
- Pattern must be a string literal
- Sets `like_pattern_` field
- Reserved keyword LIKE

**Examples:**
```sql
SHOW TABLES LIKE 'user%';           -- Tables starting with 'user'
SHOW TABLES LIKE '%_archive';       -- Tables ending with '_archive'
SHOW DATABASES LIKE 'test_';        -- 5-char databases starting with 'test_'
```

---

### 4.4 FROM Clause

**Syntax:** `FROM table_name`

**Description:** Used in specific commands to specify target
- `SHOW COLUMNS FROM table` - **Required**
- `SHOW INDEXES FROM table` - **Required**
- `SHOW TABLES FROM schema` - Optional, treated as `IN schema` (legacy syntax)
- Reserved keyword FROM

---

### 4.5 FOR Clause

**Syntax:** `FOR object_name`

**Description:** Used in SHOW GRANTS to specify object
- `SHOW GRANTS FOR object_name` - Shows grants on specific object
- Reserved keyword FOR
- Alternative: `SHOW GRANTS object_name` (without FOR)

---

## 5. Object Name Parsing

### 5.1 Optional Names

Many commands accept optional object names using `parseOptionalName()`:

**Behavior:**
- If identifier or keyword present, uses it as name
- Skips contextual keywords (PATH, DETAIL, TREE, DEPTH, SEARCH, OF, RESOLVED, OBJECTS)
- Allows most keywords as object names (except IN, LIKE, FROM)
- Returns 0 (no name) if contextual keyword or end of statement

**Example:**
```sql
SHOW TABLE;             -- No name, lists all tables
SHOW TABLE users;       -- Name = users
SHOW TABLE public;      -- Name = public (keyword used as name)
SHOW TABLE PATH;        -- No name (PATH is contextual keyword)
```

---

### 5.2 Schema Paths

Schema paths can be dot-separated hierarchical identifiers:

**Syntax:** `schema1.schema2.schema3` or `schema.object`

**Parsing:** `parseSchemaPath()`
- Accepts identifiers and keywords
- Stops at contextual keywords (PATH, DETAIL, TREE, DEPTH)
- Constructs dot-separated path
- Used for: table names, schema names, object names

**Examples:**
```sql
public.users
company.sales.customers
myschema.mytable
```

---

## 6. Implementation Details

### 6.1 ShowObjectType Enum

**Location:** `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast.h` (line 3087)

**Values:**
```cpp
enum class ShowObjectType : uint8_t
{
    // Basic commands
    TABLES,         // SHOW TABLES
    DATABASES,      // SHOW DATABASES / SHOW SCHEMAS
    COLUMNS,        // SHOW COLUMNS FROM table
    INDEXES,        // SHOW INDEXES FROM table
    CREATE_TABLE,   // SHOW CREATE TABLE table

    // Extended commands (Firebird ISQL)
    TABLE,          // SHOW TABLE [name]
    INDEX,          // SHOW INDEX [name]
    TRIGGER,        // SHOW TRIGGER [name]
    PROCEDURE,      // SHOW PROCEDURE [name]
    FUNCTION,       // SHOW FUNCTION [name]
    VIEW,           // SHOW VIEW [name]
    DOMAIN,         // SHOW DOMAIN [name]
    GENERATOR,      // SHOW GENERATOR/SEQUENCE [name]
    SCHEMA,         // SHOW SCHEMA [name]
    ROLE,           // SHOW ROLE [name]
    GRANTS,         // SHOW GRANTS [object]
    CHECKS,         // SHOW CHECKS [table]
    COLLATIONS,     // SHOW COLLATIONS
    COMMENTS,       // SHOW COMMENTS [object]
    DEPENDENCIES,   // SHOW DEPENDENCIES [object]
    PACKAGE,        // SHOW PACKAGE [name]
    SYSTEM,         // SHOW SYSTEM
    SQL_DIALECT,    // SHOW SQL DIALECT
    VERSION,        // SHOW VERSION
    DATABASE,       // SHOW DATABASE

    // Schema navigation
    SCHEMA_PATH,    // SHOW SCHEMA PATH
    SCHEMA_TREE,    // SHOW SCHEMA TREE [DEPTH n]
    SEARCH_PATH,    // SHOW SEARCH PATH
    LOCATION,       // SHOW LOCATION OF [type] name
    RESOLVED,       // SHOW RESOLVED name
    OBJECTS         // SHOW OBJECTS
};
```

---

### 6.2 ShowSchemaScope Enum

**Location:** `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast.h` (line 3128)

**Values:**
```cpp
enum class ShowSchemaScope : uint8_t
{
    CURRENT,    // Show in current schema only (default)
    IN_PATH,    // Show in all schemas in search path
    IN_SCHEMA   // Show in specific schema (schema_path_ specifies which)
};
```

---

### 6.3 ShowStmt Class

**Location:** `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast.h` (line 3135)

**Fields:**
```cpp
private:
    ShowObjectType object_type_;        // Type of object to show
    StringPool::StringId object_name_;  // Object name (optional)
    StringPool::StringId database_name_; // Database name or type qualifier
    StringPool::StringId like_pattern_;  // LIKE pattern (optional)
    ShowSchemaScope schema_scope_;       // CURRENT, IN_PATH, or IN_SCHEMA
    StringPool::StringId schema_path_;   // Schema path for IN_SCHEMA
    bool in_detail_;                     // Show extended details
    uint32_t tree_depth_;                // Depth for SCHEMA_TREE (0=unlimited)
```

**Accessors:**
- `ShowObjectType objectType()`
- `StringPool::StringId objectName()` / `tableName()`
- `StringPool::StringId databaseName()`
- `StringPool::StringId likePattern()`
- `ShowSchemaScope schemaScope()`
- `StringPool::StringId schemaPath()`
- `bool inDetail()`
- `uint32_t treeDepth()`

---

### 6.4 Parser Helper Functions

**parseOptionalName()** - Parses optional object name
- Accepts identifiers and keywords
- Skips contextual keywords
- Returns 0 if no name present

**parseSchemaPath()** - Parses dot-separated schema path
- Accepts identifiers and keywords
- Constructs path string
- Stops at contextual keywords

**parseOptionalLike()** - Parses optional LIKE clause
- Expects string literal after LIKE
- Returns pattern string ID or 0

**parseInClauses()** - Parses IN modifiers (loop)
- Handles `IN PATH`, `IN DETAIL`, `IN schema_path`
- Can be called multiple times (before/after LIKE)
- Updates `schema_scope_`, `schema_path_`, `in_detail_`

**matchContextual()** - Matches and consumes contextual keyword
- Case-insensitive comparison
- Only matches IDENTIFIER tokens
- Advances if match found

**checkContextual()** - Checks for contextual keyword without consuming
- Case-insensitive comparison
- Returns true/false, doesn't advance

---

## 7. Complete BNF Grammar

```bnf
<show_statement> ::=
    SHOW <show_command>

<show_command> ::=
    <show_tables>
  | <show_databases>
  | <show_columns>
  | <show_indexes>
  | <show_create_table>
  | <show_table>
  | <show_index>
  | <show_trigger>
  | <show_procedure>
  | <show_function>
  | <show_view>
  | <show_domain>
  | <show_generator>
  | <show_schema>
  | <show_schema_path>
  | <show_schema_tree>
  | <show_role>
  | <show_grants>
  | <show_checks>
  | <show_collations>
  | <show_comments>
  | <show_dependencies>
  | <show_package>
  | <show_system>
  | <show_sql_dialect>
  | <show_version>
  | <show_database>
  | <show_search_path>
  | <show_location_of>
  | <show_resolved>
  | <show_objects>

<show_tables> ::=
    TABLES [ FROM <schema_path> ] [ <in_clauses> ] [ <like_clause> ] [ <in_clauses> ]

<show_databases> ::=
    DATABASES [ <like_clause> ]

<show_columns> ::=
    COLUMNS FROM <schema_path> [ <like_clause> ] [ <in_clauses> ]

<show_indexes> ::=
    INDEXES FROM <schema_path> [ <in_clauses> ]

<show_create_table> ::=
    CREATE TABLE <schema_path>

<show_table> ::=
    TABLE [ <identifier> ] [ <in_clauses> ] [ <like_clause> ] [ <in_clauses> ]

<show_index> ::=
    INDEX [ <identifier> ] [ <in_clauses> ] [ <like_clause> ] [ <in_clauses> ]

<show_trigger> ::=
    TRIGGER [ <identifier> ] [ <in_clauses> ]

<show_procedure> ::=
    PROCEDURE [ <identifier> ] [ <in_clauses> ]

<show_function> ::=
    FUNCTION [ <identifier> ] [ <in_clauses> ]

<show_view> ::=
    VIEW [ <identifier> ] [ <in_clauses> ]

<show_domain> ::=
    DOMAIN [ <identifier> ] [ <in_clauses> ]

<show_generator> ::=
    ( GENERATOR | SEQUENCE ) [ <identifier> ] [ <in_clauses> ]

<show_schema> ::=
    SCHEMA [ <identifier> ] [ <in_clauses> ]

<show_schema_path> ::=
    SCHEMA PATH

<show_schema_tree> ::=
    SCHEMA TREE [ DEPTH <integer> ] [ FROM ROOT ]

<show_role> ::=
    ROLE [ <identifier> ] [ <in_clauses> ]

<show_grants> ::=
    GRANTS [ FOR <identifier> | <identifier> ] [ <in_clauses> ]

<show_checks> ::=
    CHECKS [ <identifier> ] [ <in_clauses> ]

<show_collations> ::=
    COLLATIONS [ <like_clause> ]

<show_comments> ::=
    COMMENTS [ <identifier> ] [ <in_clauses> ]

<show_dependencies> ::=
    DEPENDENCIES [ <identifier> ] [ <in_clauses> ]

<show_package> ::=
    PACKAGE [ <identifier> ] [ <in_clauses> ]

<show_system> ::=
    SYSTEM [ <in_clauses> ]

<show_sql_dialect> ::=
    SQL DIALECT

<show_version> ::=
    VERSION

<show_database> ::=
    DATABASE

<show_search_path> ::=
    SEARCH PATH

<show_location_of> ::=
    ( LOCATION | contextual:LOCATION ) OF [ <object_type_keyword> ] <identifier>

<show_resolved> ::=
    contextual:RESOLVED <identifier>

<show_objects> ::=
    contextual:OBJECTS [ <in_clauses> ] [ <like_clause> ] [ <in_clauses> ]

<in_clauses> ::=
    { <in_clause> }

<in_clause> ::=
    IN ( contextual:PATH | contextual:DETAIL | <schema_path> )

<like_clause> ::=
    LIKE <string_literal>

<schema_path> ::=
    <identifier> { DOT <identifier> }

<object_type_keyword> ::=
    TABLE | VIEW | FUNCTION | PROCEDURE | SEQUENCE | INDEX | TRIGGER | DOMAIN

<identifier> ::=
    IDENTIFIER | <keyword_as_identifier>
```

**Notes:**
- `contextual:KEYWORD` indicates a contextual keyword (lexed as IDENTIFIER, matched case-insensitively)
- `<keyword_as_identifier>` allows most keywords except IN, LIKE, FROM
- `{ }` indicates zero or more repetitions
- `[ ]` indicates optional elements
- `|` indicates alternatives

---

## 8. Keyword Classification

### 8.1 Reserved Keywords (from TokenType enum)

These are recognized by the lexer as keyword tokens:

- TABLES, DATABASES, COLUMNS, INDEXES
- CREATE, TABLE, INDEX, VIEW, DOMAIN, SEQUENCE, GENERATOR
- TRIGGER, PROCEDURE, FUNCTION, SCHEMA, ROLE
- GRANTS, CHECKS, COLLATIONS, COMMENTS, DEPENDENCIES
- PACKAGE, SYSTEM, SQL, DIALECT, VERSION, DATABASE
- LOCATION (dual-purpose: TABLESPACE and SHOW)
- FROM, LIKE, IN, FOR
- All other SQL keywords

---

### 8.2 Contextual Keywords (handled by parser)

These are lexed as IDENTIFIER but have special meaning in SHOW context:

- **PATH** - Used in `SHOW SCHEMA PATH`, `IN PATH`
- **TREE** - Used in `SHOW SCHEMA TREE`
- **DEPTH** - Used in `SHOW SCHEMA TREE DEPTH n`
- **DETAIL** - Used in `IN DETAIL`
- **SEARCH** - Used in `SHOW SEARCH PATH`
- **OF** - Used in `SHOW LOCATION OF`
- **RESOLVED** - Used in `SHOW RESOLVED`
- **OBJECTS** - Used in `SHOW OBJECTS`
- **ROOT** - Used in `SHOW SCHEMA TREE FROM ROOT`

**Rationale:** These words are commonly used as column names, table names, etc. Making them reserved would break existing schemas. The parser handles them contextually only where they have special meaning.

---

## 9. Error Handling

The parser provides specific error messages for each SHOW command variant:

**Missing required elements:**
```
"Expected FROM after SHOW COLUMNS"
"Expected TABLE after SHOW CREATE"
"Expected table name after FROM"
"Expected PATH after SHOW SEARCH"
"Expected OF after SHOW LOCATION"
"Expected object name after SHOW LOCATION OF"
"Expected object name after SHOW RESOLVED"
"Expected DIALECT after SHOW SQL"
```

**Invalid syntax:**
```
"Expected string pattern after LIKE"
"Expected schema path, PATH, or DETAIL after IN"
"Expected integer after DEPTH"
"Expected ROOT after FROM in SHOW SCHEMA TREE"
```

**Unknown command:**
```
"Expected TABLES, DATABASES, COLUMNS, INDEXES, CREATE, TABLE, INDEX,
 TRIGGER, PROCEDURE, FUNCTION, VIEW, DOMAIN, GENERATOR, SEQUENCE,
 SCHEMA, ROLE, GRANTS, CHECKS, COLLATIONS, COMMENTS, DEPENDENCIES,
 PACKAGE, SYSTEM, SQL DIALECT, VERSION, DATABASE, SEARCH PATH,
 LOCATION OF, RESOLVED, or OBJECTS after SHOW"
```

---

## 10. Usage Examples by Category

### 10.1 Basic Introspection

```sql
-- List all tables
SHOW TABLES;

-- List tables in specific schema
SHOW TABLES IN public;

-- Find tables matching pattern
SHOW TABLES LIKE 'user%';

-- List all databases
SHOW DATABASES;

-- Show table structure
SHOW COLUMNS FROM users;
DESCRIBE users;  -- Equivalent
DESC users;      -- Equivalent

-- Show table indexes
SHOW INDEXES FROM orders;

-- Get CREATE TABLE statement
SHOW CREATE TABLE employees;
```

### 10.2 Firebird ISQL Style

```sql
-- Show detailed table structure
SHOW TABLE users;

-- Show specific index details
SHOW INDEX idx_users_email;

-- Show trigger definition
SHOW TRIGGER trg_audit_insert;

-- Show procedure with body
SHOW PROCEDURE calculate_discount IN DETAIL;

-- Show function definition
SHOW FUNCTION validate_email;

-- Show view definition
SHOW VIEW active_users;

-- Show domain constraints
SHOW DOMAIN email_address IN DETAIL;

-- Show sequence info
SHOW SEQUENCE user_id_seq;
SHOW GENERATOR order_id_gen;  -- Firebird alias
```

### 10.3 Security and Privileges

```sql
-- Show current role
SHOW ROLE;

-- Show specific role members
SHOW ROLE admin IN DETAIL;

-- Show all grants
SHOW GRANTS;

-- Show grants on specific object
SHOW GRANTS FOR users;

-- Show check constraints
SHOW CHECKS users;
```

### 10.4 Schema Navigation

```sql
-- Show current schema
SHOW SCHEMA;

-- Show schema path
SHOW SCHEMA PATH;

-- Show schema hierarchy
SHOW SCHEMA TREE;
SHOW SCHEMA TREE DEPTH 3;
SHOW SCHEMA TREE FROM ROOT;

-- Show search path
SHOW SEARCH PATH;

-- Find where object exists
SHOW LOCATION OF users;
SHOW LOCATION OF TABLE users;
SHOW LOCATION OF FUNCTION validate;

-- Show which object would be resolved
SHOW RESOLVED users;

-- List all objects in current schema
SHOW OBJECTS;
SHOW OBJECTS IN PATH;
SHOW OBJECTS LIKE 'tmp%';
```

### 10.5 System Information

```sql
-- Show server version
SHOW VERSION;

-- Show database info
SHOW DATABASE;

-- Show SQL dialect
SHOW SQL DIALECT;

-- Show system tables
SHOW SYSTEM;

-- Show available collations
SHOW COLLATIONS;
SHOW COLLATIONS LIKE 'utf8%';

-- Show comments
SHOW COMMENTS users;

-- Show dependencies
SHOW DEPENDENCIES active_users;

-- Show packages
SHOW PACKAGE utils;
```

### 10.6 Advanced Filtering

```sql
-- Combine IN PATH with LIKE
SHOW TABLES IN PATH LIKE 'temp%';

-- Show detailed info across search path
SHOW TABLE IN PATH IN DETAIL;

-- Search in specific schema hierarchy
SHOW OBJECTS IN company.sales LIKE 'order%';

-- Multiple filters
SHOW TABLES IN public LIKE 'user%' IN DETAIL;
```

---

## 11. Comparison with Other Databases

### 11.1 MySQL Compatibility

ScratchBird supports MySQL-style SHOW commands:
- `SHOW TABLES [FROM schema] [LIKE 'pattern']`
- `SHOW DATABASES [LIKE 'pattern']`
- `SHOW COLUMNS FROM table [LIKE 'pattern']`
- `SHOW INDEXES FROM table`
- `SHOW CREATE TABLE table`

**Extensions beyond MySQL:**
- `IN PATH` - Schema search path support
- `IN DETAIL` - Extended information
- Hierarchical schema paths (schema.sub.table)

---

### 11.2 PostgreSQL Compatibility

ScratchBird supports PostgreSQL concepts:
- `SHOW DATABASES` equivalent to `\dn` (list schemas)
- `DESCRIBE table` equivalent to `\d table`
- Schema search path concept (similar to PostgreSQL's search_path)

**Extensions beyond PostgreSQL:**
- More granular SHOW commands (SHOW INDEX, SHOW TRIGGER, etc.)
- Hierarchical schemas (PostgreSQL has flat namespace)
- `IN PATH` clause for multi-schema searches

---

### 11.3 Firebird ISQL Compatibility

ScratchBird implements Firebird ISQL commands:
- `SHOW TABLE [name]` - Firebird ISQL format
- `SHOW INDEX [name]`
- `SHOW TRIGGER [name]`
- `SHOW PROCEDURE [name]`
- `SHOW FUNCTION [name]` (Firebird 3.0+)
- `SHOW VIEW [name]`
- `SHOW DOMAIN [name]`
- `SHOW GENERATOR [name]` / `SHOW SEQUENCE [name]`
- `SHOW GRANTS [object]`
- `SHOW SQL DIALECT`
- `SHOW VERSION`

**Extensions beyond Firebird:**
- Hierarchical schemas (Firebird has single namespace)
- `IN PATH` / `IN DETAIL` modifiers
- Schema navigation commands (PATH, TREE, LOCATION, RESOLVED)
- `SHOW OBJECTS` - comprehensive listing

---

## 12. Future Considerations

### Potential Enhancements:

1. **Output Formatting Options**
   - `SHOW ... FORMAT JSON`
   - `SHOW ... FORMAT XML`
   - `SHOW ... FORMAT TABLE` (default)

2. **Additional Filters**
   - `WHERE` clause for programmatic filtering
   - Regular expressions in LIKE clause

3. **Performance Metadata**
   - `SHOW STATISTICS FOR table`
   - `SHOW QUERY CACHE`
   - `SHOW CONNECTIONS`

4. **Extended Object Types**
   - `SHOW CONSTRAINT name`
   - `SHOW FOREIGN KEY name`
   - `SHOW PARTITION table`
   - `SHOW CHARSET`

5. **Compatibility Modes**
   - `SET SHOW_MODE = 'MYSQL' | 'POSTGRESQL' | 'FIREBIRD'`
   - Adjust output format to match target database

---

## 13. Testing Recommendations

To ensure comprehensive test coverage of SHOW commands:

### 13.1 Basic Functionality Tests
- Each SHOW command with no options
- Each SHOW command with all optional clauses
- LIKE pattern matching (%, _, escaping)
- Schema path parsing (dot-separated)

### 13.2 Schema Scope Tests
- IN PATH across multiple schemas
- IN schema_path with qualified paths
- Current schema (default behavior)
- Empty results vs. schema not found

### 13.3 Contextual Keyword Tests
- Using PATH, TREE, etc. as table/column names
- Contextual keywords in different positions
- Case-insensitive matching

### 13.4 Error Cases
- Missing required clauses (FROM, etc.)
- Invalid patterns
- Non-existent schemas/objects
- Syntax errors in each variant

### 13.5 Integration Tests
- SHOW after CREATE/ALTER/DROP
- Schema search path resolution
- Dependencies and object graphs
- Privilege display after GRANT/REVOKE

---

## 14. Summary

The ScratchBird parser implements **32 distinct SHOW command variants** organized into:

- **5** basic commands (MySQL/PostgreSQL style)
- **21** extended commands (Firebird ISQL compatibility)
- **6** schema navigation commands (hierarchical schema support)

**Key Features:**
- Flexible schema scoping (current, search path, specific schema)
- Pattern matching with LIKE
- Detailed vs. summary output modes
- Contextual keywords to avoid reservation conflicts
- Dot-separated hierarchical schema paths
- Compatible with MySQL, PostgreSQL, and Firebird ISQL conventions

**Implementation:**
- Source: `/home/dcalford/CliWork/ScratchBird/src/parser/parser.cpp:4907-5466`
- AST: `ShowStmt` class with `ShowObjectType` enum
- Schema scope: `ShowSchemaScope` enum (CURRENT, IN_PATH, IN_SCHEMA)
- Parser helpers for contextual keywords and path parsing

This comprehensive SHOW command system provides developers with powerful introspection capabilities while maintaining compatibility with multiple SQL dialects.
