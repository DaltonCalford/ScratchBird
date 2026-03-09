# Specification: CREATE DATABASE Statement

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | parser |
| **Spec Version** | 1.0.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1522`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:4550`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_ddl.cpp`

## Synopsis

The CREATE DATABASE statement creates a new database instance with specified options, character sets, and aliases.

## Specification

### EBNF Grammar

```ebnf
create_database_stmt ::=
    "CREATE" "DATABASE" [ "IF" "NOT" "EXISTS" ]
    schema_path
    [ database_options ]
    [ "ALIAS" "AS" identifier ("," identifier )* ]

database_options ::=
    [ "PAGE_SIZE" "=" integer ]
    [ "DEFAULT" "CHARACTER" "SET" identifier ]
    [ "DIFFERENCE" "FILE" "=" string ]
    [ "COLLATION" "=" identifier ]
    [ "LENGTH" "=" integer ]
    [ "USER" "=" string ]
    [ "PASSWORD" "=" string ]
    [ "ROLE" "=" string ]
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1522
class CreateDatabaseStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateDatabaseStmt; }
    
    bool if_not_exists = false;
    SchemaPath database_path;
    StringPool::StringId source_spec = StringPool::INVALID_ID;
    std::vector<DatabaseOption> options;
    std::vector<StringPool::StringId> aliases;
};

struct DatabaseOption {
    StringPool::StringId key = StringPool::INVALID_ID;
    StringPool::StringId value = StringPool::INVALID_ID;
};
```

## Examples

```sql
-- Create basic database
CREATE DATABASE production;

-- Create database with options
CREATE DATABASE orders_db
    PAGE_SIZE = 8192
    DEFAULT CHARACTER SET UTF8
    COLLATION = UTF8_GENERAL_CI;

-- Create database with aliases
CREATE DATABASE main_db ALIAS AS primary, master;
```

## Related Specifications

- [stmt_alter_database.md](./stmt_alter_database.md) - Database modification
- [stmt_drop_database.md](./stmt_drop_database.md) - Database removal

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
