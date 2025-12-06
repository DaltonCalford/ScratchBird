# sb_isql Command Classification
## Server-Side (Parser/SQL) vs Client-Only Commands

**Date:** 2025-12-06

This document categorizes SHOW and SET commands by where they should be implemented:
- **Server-Side**: Implemented in the parser/executor, available in SQL/PSQL/DML contexts
- **Client-Only**: Implemented only in sb_isql, controls client display/behavior

---

## SHOW Commands

### Server-Side (Catalog Queries via Parser)

These commands query the ScratchBird system catalog and should be implemented as SQL statements usable in PSQL, triggers, or any SQL context.

| Command | ScratchBird Implementation | System Tables Used |
|---------|---------------------------|-------------------|
| `SHOW CHECKS [table]` | `SELECT` check constraints | `sb_constraints` (constraint_type=3) |
| `SHOW COLLATIONS` | `SELECT` collation sequences | `sb_collation`, `sb_charset` |
| `SHOW COMMENTS` | `SELECT` object comments | `sb_comments` |
| `SHOW DATABASE` | `SELECT` database metadata | Database header info, `sb_schema`, `sb_tables` |
| `SHOW DEPENDENCIES obj` | `SELECT` dependency graph | `sb_dependencies` |
| `SHOW DOMAIN [name]` | `SELECT` domain definitions | `sb_domains` |
| `SHOW EXCEPTION [name]` | `SELECT` user exceptions | (Future: sb_exceptions) |
| `SHOW FILTER [name]` | `SELECT` BLOB filters | (Future: sb_filters) |
| `SHOW FUNCTION [name]` | `SELECT` user-defined functions | `sb_procedures` (procedure_type=1) |
| `SHOW GENERATOR [name]` | `SELECT` sequences | `sb_sequences` |
| `SHOW GRANTS obj` | `SELECT` object privileges | `sb_permissions`, `sb_column_permissions` |
| `SHOW INDEX [name]` | `SELECT` index details | `sb_indexes`, `sb_columns` |
| `SHOW MAPPING [name]` | `SELECT` security mappings | `sb_group_mappings`, `sb_user_mappings` |
| `SHOW PACKAGE [name]` | `SELECT` package definitions | `sb_packages` |
| `SHOW PROCEDURE [name]` | `SELECT` stored procedures | `sb_procedures` (procedure_type=0), `sb_procedure_params` |
| `SHOW PUBLICATION [name]` | `SELECT` replication pubs | (Future: sb_publications) |
| `SHOW ROLE [name]` | `SELECT` role definitions | `sb_roles`, `sb_role_members` |
| `SHOW SCHEMA [name]` | `SELECT` schema definitions | `sb_schema` |
| `SHOW SECCLASS [name]` | `SELECT` security classes | (Future: sb_security_classes) |
| `SHOW SQL DIALECT` | Session variable query | Session state |
| `SHOW SYSTEM` | `SELECT` system objects | `sb_tables`, `sb_views` (system flag) |
| `SHOW TABLE [name]` | `SELECT` table structure | `sb_tables`, `sb_columns`, `sb_constraints` |
| `SHOW TRIGGER [name]` | `SELECT` trigger definitions | `sb_triggers` |
| `SHOW VERSION` | Server version info | Database header / server info |
| `SHOW VIEW [name]` | `SELECT` view definitions | `sb_views`, `sb_columns` |

**Total Server-Side SHOW: 25 commands**

### Client-Only SHOW Commands

| Command | Reason | Notes |
|---------|--------|-------|
| (None identified) | All SHOW commands query server metadata | - |

**Note:** All SHOW commands should be server-side as they query catalog data.

---

## SET Commands

### Server-Side (Session Variables via Parser)

These affect the database session state and should be available in SQL/PSQL contexts.

| Command | Implementation | Notes |
|---------|---------------|-------|
| `SET SQL DIALECT N` | Session variable, affects parser behavior | Dialect 1/2/3 |
| `SET NAMES charset` | Session character set | Uses `sb_charset` for validation |
| `SET TRANSACTION ...` | Full SQL statement | Transaction parameters |
| `SET LOCAL_TIMEOUT N` | Session variable | Statement timeout |
| `SET ROLE name` | SQL statement | Active role from `sb_roles` |
| `SET SESSION AUTHORIZATION` | SQL statement | Session user context |

**Total Server-Side SET: 6 commands**

### Hybrid Commands (Server + Client Coordination)

These affect both server behavior and client display.

| Command | Server Component | Client Component |
|---------|-----------------|------------------|
| `SET AUTODDL [ON\|OFF]` | Transaction commit behavior | Client tracks mode |
| `SET PLAN [ON\|OFF]` | Server generates plan | Client displays it |
| `SET PLANONLY [ON\|OFF]` | Server prepares but doesn't execute | Client displays plan |
| `SET EXPLAIN [ON\|OFF]` | Server generates detailed plan | Client displays it |
| `SET STATS [ON\|OFF]` | Server tracks statistics | Client displays them |
| `SET PER_TABLE_STATS [ON\|OFF]` | Server tracks per-table stats | Client displays them |
| `SET KEEP_TRAN_PARAMS [ON\|OFF]` | Affects transaction handling | Client remembers params |
| `SET WARNINGS [ON\|OFF]` | Server returns warnings | Client displays them |

**Total Hybrid SET: 8 commands**

### Client-Only SET Commands

These control only sb_isql display behavior and have no server-side component.

| Command | Purpose | Notes |
|---------|---------|-------|
| `SET BAIL [ON\|OFF]` | Stop script on first error | Client error handling |
| `SET BLOB [ALL\|N]` | BLOB display mode | Display formatting |
| `SET BLOBDISPLAY [N]` | BLOB subtype display | Display formatting |
| `SET COUNT [ON\|OFF]` | Display row counts | Client output |
| `SET ECHO [ON\|OFF]` | Echo input commands | Client input echo |
| `SET HEADING [ON\|OFF]` | Show column headings | Display formatting |
| `SET LIST [ON\|OFF]` | Vertical display mode | Display formatting |
| `SET MAXROWS N` | Limit rows displayed | Client-side limit |
| `SET SQLDA_DISPLAY [ON\|OFF]` | Show SQLDA structure | Debug display |
| `SET TIME [ON\|OFF]` | Show time in prompt | Prompt formatting |
| `SET TERM string` | Statement terminator | Client parsing |
| `SET WIDTH col N` | Column display width | Display formatting |
| `SET WNG [ON\|OFF]` | Alias for WARNINGS | (see WARNINGS) |

**Total Client-Only SET: 13 commands**

---

## Summary

| Category | Count | Implementation Location |
|----------|-------|------------------------|
| **Server-Side SHOW** | 25 | Parser → Catalog SELECT |
| **Client-Only SHOW** | 0 | N/A |
| **Server-Side SET** | 6 | Parser → Session State |
| **Hybrid SET** | 8 | Parser + Client |
| **Client-Only SET** | 13 | sb_isql only |
| **TOTAL** | 52 | - |

---

## Implementation Recommendations

### Server-Side Parser Implementation

1. **SHOW Commands as Parser Feature**
   - Parse `SHOW <object> [name]` as a statement type
   - Generate appropriate `SELECT` from ScratchBird system tables
   - Return result set like any query
   - Usable in PSQL: `FOR SELECT * FROM SHOW TABLE 'CUSTOMERS' INTO ...`

2. **Session SET Commands**
   - `SET SQL DIALECT` - Modifies parser/executor behavior
   - `SET NAMES` - Character set for the connection (validated against `sb_charset`)
   - `SET TRANSACTION` - Standard SQL transaction control
   - `SET LOCAL_TIMEOUT` - Statement timeout
   - `SET ROLE` - Security context (validated against `sb_roles`)

3. **Hybrid Commands**
   - Server provides data (plans, stats, warnings)
   - Client requests and displays it
   - Could use `EXPLAIN` statement for plans
   - Stats available via `sb_statistics`

### Client-Side Only Implementation

1. **Display Formatting** (sb_isql only)
   - `SET HEADING`, `SET LIST`, `SET WIDTH`, `SET BLOB`, `SET BLOBDISPLAY`
   - `SET COUNT`, `SET MAXROWS`
   - These never go to the server

2. **Script Control** (sb_isql only)
   - `SET TERM` - Client-side statement splitting
   - `SET BAIL` - Client error handling
   - `SET ECHO` - Client input display

3. **Prompt/UI** (sb_isql only)
   - `SET TIME` - Prompt customization
   - `SET SQLDA_DISPLAY` - Debug output

---

## ScratchBird System Table Mappings

Each `SHOW` command maps to specific ScratchBird catalog tables:

### Core Object Queries

| SHOW | Primary Table | Join Tables | Key Fields |
|------|---------------|-------------|------------|
| TABLE | `sb_tables` | `sb_columns`, `sb_constraints` | table_id, table_name, schema_id |
| INDEX | `sb_indexes` | `sb_tables`, `sb_columns` | index_id, index_name, index_type |
| TRIGGER | `sb_triggers` | `sb_tables` | trigger_id, trigger_name, trigger_timing |
| PROCEDURE | `sb_procedures` | `sb_procedure_params` | procedure_id, procedure_name (type=0) |
| FUNCTION | `sb_procedures` | `sb_procedure_params` | procedure_id, procedure_name (type=1) |
| VIEW | `sb_views` | `sb_columns` | view_id, view_name, definition_oid |
| DOMAIN | `sb_domains` | - | domain_id, domain_name, base_type_oid |
| GENERATOR | `sb_sequences` | - | sequence_id, sequence_name, current_value |
| SCHEMA | `sb_schema` | - | schema_id, schema_name, parent_schema_id |

### Security Queries

| SHOW | Primary Table | Join Tables | Key Fields |
|------|---------------|-------------|------------|
| GRANTS | `sb_permissions` | `sb_column_permissions` | object_id, privileges, grantee_id |
| ROLE | `sb_roles` | `sb_role_members` | role_id, role_name |
| MAPPING | `sb_group_mappings` | `sb_user_mappings` | mapping details |

### Metadata Queries

| SHOW | Primary Table | Join Tables | Key Fields |
|------|---------------|-------------|------------|
| DEPENDENCIES | `sb_dependencies` | - | dependent_object_id, referenced_object_id |
| COMMENTS | `sb_comments` | - | object_id, comment_text_oid |
| COLLATIONS | `sb_collation` | `sb_charset` | collation_id, name, charset_id |
| CHECKS | `sb_constraints` | - | constraint_id (constraint_type=3) |

### Package/UDR Queries

| SHOW | Primary Table | Join Tables | Key Fields |
|------|---------------|-------------|------------|
| PACKAGE | `sb_packages` | `sb_procedures` | package_id, package_name |
| UDR | `sb_udr` | - | udr_id, entry_point |

---

## New AST Node Types

### For SHOW Commands

```cpp
// In ast.h
struct ShowStatement : Statement {
    enum class ObjectType {
        TABLE, INDEX, TRIGGER, PROCEDURE, FUNCTION, VIEW,
        DOMAIN, GENERATOR, SCHEMA, ROLE, GRANTS, COLLATION,
        DATABASE, MAPPING, PACKAGE, SYSTEM, CHECKS, COMMENTS,
        DEPENDENCIES, SQL_DIALECT, VERSION
    };
    ObjectType object_type;
    std::optional<std::string> object_name;  // Optional filter
    bool show_system = false;                // Include system objects
};
```

### For Session SET Commands

```cpp
// In ast.h
struct SetSessionStatement : Statement {
    enum class SessionVar {
        SQL_DIALECT,      // SET SQL DIALECT N
        NAMES,            // SET NAMES 'charset'
        LOCAL_TIMEOUT,    // SET LOCAL_TIMEOUT N
        ROLE,             // SET ROLE 'rolename'
        TRANSACTION       // SET TRANSACTION ...
    };
    SessionVar variable;
    TypedValue value;

    // For SET TRANSACTION
    std::optional<TransactionOptions> transaction_options;
};
```

---

## Priority for Implementation

### High Priority (Parser)
1. `SHOW TABLE/INDEX/TRIGGER/PROCEDURE/FUNCTION/VIEW` - Core metadata
2. `SHOW DATABASE` - Database info
3. `SHOW GRANTS` - Security info
4. `SET SQL DIALECT` - Compatibility
5. `SET TRANSACTION` - Transaction control

### Medium Priority (Parser)
1. `SHOW DOMAIN/GENERATOR/SCHEMA` - Additional metadata
2. `SHOW DEPENDENCIES` - Useful for refactoring
3. `SET NAMES` - Character set support (uses `sb_charset`)
4. `SET LOCAL_TIMEOUT` - Query timeout

### High Priority (Client)
1. `SET TERM` - Script compatibility
2. `SET BAIL` - Error handling
3. `SET COUNT/HEADING` - Basic display

### Medium Priority (Client)
1. `SET PLAN/STATS` - Query analysis
2. `SET LIST/WIDTH` - Display formatting
3. `SET BLOB` - BLOB display

---

## Notes

- All system tables use the `sb_` prefix convention (ScratchBird native)
- UUIDs are used for all object references (16-byte IDs)
- TOAST is used for large text fields (definitions, expressions, etc.)
- MGA semantics: `is_valid=1` for current records, `is_valid=0` for deleted
- Current catalog has 42 system tables
- PostgreSQL `pg_catalog` views are provided for wire protocol compatibility but map to `sb_*` tables
