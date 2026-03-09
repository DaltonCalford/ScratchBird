# Specification: ALTER DATABASE Statement

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | parser |
| **Spec Version** | 1.0.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Team |

## Synopsis

The ALTER DATABASE statement modifies database-level properties including name, owner, aliases, and configuration options.

## Specification

### EBNF Grammar

```ebnf
alter_database_stmt ::=
    "ALTER" "DATABASE" schema_path
    ( "RENAME" "TO" new_name
    | "OWNER" "TO" new_owner
    | "SET" "TABLESPACE" new_tablespace
    | "ADD" "ALIAS" alias_name
    | "DROP" "ALIAS" alias_name
    | "SET" option_name "=" option_value
    )
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1826
enum class AlterDatabaseAction : uint8_t {
    RENAME = 1, SET_OWNER = 2, ADD_ALIAS = 3,
    DROP_ALIAS = 4, SET_OPTIONS = 5
};

struct AlterDatabaseOption {
    StringPool::StringId key = StringPool::INVALID_ID;
    StringPool::StringId value = StringPool::INVALID_ID;
};

class AlterDatabaseStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterDatabaseStmt; }
    
    AlterDatabaseAction action = AlterDatabaseAction::RENAME;
    SchemaPath database_path;
    StringPool::StringId new_name = StringPool::INVALID_ID;
    StringPool::StringId owner = StringPool::INVALID_ID;
    StringPool::StringId alias = StringPool::INVALID_ID;
    std::vector<AlterDatabaseOption> options;
};
```

## Examples

```sql
-- Rename database
ALTER DATABASE old_name RENAME TO new_name;

-- Change owner
ALTER DATABASE app_db OWNER TO app_admin;

-- Add alias
ALTER DATABASE production_db ADD ALIAS prod;

-- Drop alias
ALTER DATABASE production_db DROP ALIAS prod;

-- Set option
ALTER DATABASE app_db SET enable_logging = true;
```

## Related Specifications

- [stmt_create_database.md](./stmt_create_database.md) - Database creation
- [stmt_drop_database.md](./stmt_drop_database.md) - Database removal

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
