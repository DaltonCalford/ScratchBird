# Specification: ALTER TABLE Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1876`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:7800`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_ddl.cpp`

## Synopsis

The ALTER TABLE statement modifies the structure of an existing table, including adding/dropping columns and constraints, renaming objects, and enabling/disabling features like row-level security.

## Specification

### EBNF Grammar

```ebnf
alter_table_stmt ::=
    "ALTER" "TABLE" [ "IF" "EXISTS" ] [ "ONLY" ]
    schema_path
    alter_table_action ("," alter_table_action )*

alter_table_action ::=
    "ADD" [ "COLUMN" ] column_def [ "FIRST" | "AFTER" identifier ]
  | "DROP" [ "COLUMN" ] [ "IF" "EXISTS" ] identifier [ "RESTRICT" | "CASCADE" ]
  | "ALTER" [ "COLUMN" ] column_name alter_column_action
  | "RENAME" [ "COLUMN" ] column_name "TO" new_column_name
  | "ADD" table_constraint
  | "DROP" "CONSTRAINT" [ "IF" "EXISTS" ] constraint_name [ "RESTRICT" | "CASCADE" ]
  | "RENAME" "CONSTRAINT" constraint_name "TO" new_constraint_name
  | "RENAME" "TO" new_table_name
  | "SET" "SCHEMA" schema_name
  | "SET" "TABLESPACE" tablespace_name
  | "ENABLE" "ROW" "LEVEL" "SECURITY"
  | "DISABLE" "ROW" "LEVEL" "SECURITY"
  | "FORCE" "ROW" "LEVEL" "SECURITY"
  | "NO" "FORCE" "ROW" "LEVEL" "SECURITY"
  | "ENABLE" "TRIGGER" [ trigger_name | "ALL" | "USER" ]
  | "DISABLE" "TRIGGER" [ trigger_name | "ALL" | "USER" ]
  | "SET" "STATISTICS" integer
  | "VALIDATE" "CONSTRAINT" constraint_name
  | "INHERIT" schema_path
  | "NO" "INHERIT" schema_path

alter_column_action ::=
    "SET" "DEFAULT" expression
  | "DROP" "DEFAULT"
  | "SET" "NOT" "NULL"
  | "DROP" "NOT" "NULL"
  | "SET" "DATA" "TYPE" type_name [ "COLLATE" collation ]
  | "SET" "STATISTICS" integer
  | "SET" "STORAGE" ( "PLAIN" | "EXTERNAL" | "EXTENDED" | "MAIN" )
  | "SET" "POSITION" integer
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1876
class AlterTableStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterTableStmt; }
    
    bool if_exists = false;
    bool only = false;  // Only this table, not descendants

    SchemaPath table_path;

    // Action details
    AlterTableAction action;

    // For ADD/DROP/ALTER COLUMN
    ColumnDef* column = nullptr;
    StringPool::StringId column_name = StringPool::INVALID_ID;
    Expression* default_expr = nullptr;
    bool has_default_expr = false;
    int32_t position_1_based = 0;
    bool has_position = false;

    // For RENAME COLUMN
    StringPool::StringId new_name = StringPool::INVALID_ID;

    // For ADD/DROP CONSTRAINT
    TableConstraint* constraint = nullptr;
    StringPool::StringId constraint_name = StringPool::INVALID_ID;
    bool cascade = false;

    // For ENABLE/DISABLE TRIGGER
    StringPool::StringId trigger_name = StringPool::INVALID_ID;
    bool trigger_all = false;

    // For SET STATISTICS
    int32_t statistics_target = 0;
    bool has_statistics_target = false;

    // For SET STORAGE
    StringPool::StringId storage_type = StringPool::INVALID_ID;

    // For SET TABLESPACE
    SchemaPath tablespace;

    // For SET SCHEMA
    SchemaPath target_schema;

    // For ATTACH/DETACH PARTITION
    SchemaPath partition_path;
    StringPool::StringId partition_bounds = StringPool::INVALID_ID;
    bool has_partition_bounds = false;

    // For INHERIT/NO INHERIT
    SchemaPath inherit_parent;
    bool has_inherit_parent = false;
};

enum class AlterTableAction : uint8_t {
    ADD_COLUMN, DROP_COLUMN, ALTER_COLUMN, RENAME_COLUMN,
    ADD_CONSTRAINT, DROP_CONSTRAINT, RENAME_CONSTRAINT, RENAME_TABLE,
    SET_TABLESPACE, SET_SCHEMA, ENABLE_RLS, DISABLE_RLS,
    FORCE_RLS, NO_FORCE_RLS, ENABLE_TRIGGER, DISABLE_TRIGGER,
    SET_STATISTICS, SET_STORAGE, INHERIT, NO_INHERIT,
    VALIDATE_CONSTRAINT, ALTER_COLUMN_SET_DEFAULT, ALTER_COLUMN_DROP_DEFAULT,
    ALTER_COLUMN_SET_NOT_NULL, ALTER_COLUMN_DROP_NOT_NULL, ALTER_COLUMN_POSITION,
    ATTACH_PARTITION, DETACH_PARTITION
};
```

## Examples

```sql
-- Add column
ALTER TABLE users ADD COLUMN phone VARCHAR(20);

-- Add column at specific position
ALTER TABLE products ADD COLUMN description TEXT AFTER name;

-- Drop column
ALTER TABLE users DROP COLUMN phone;

-- Add constraint
ALTER TABLE orders ADD CONSTRAINT fk_user
    FOREIGN KEY (user_id) REFERENCES users(id);

-- Rename table
ALTER TABLE old_users RENAME TO users_legacy;

-- Enable row-level security
ALTER TABLE sensitive_data ENABLE ROW LEVEL SECURITY;

-- Multiple actions
ALTER TABLE products
    ADD COLUMN sku VARCHAR(50),
    ADD CONSTRAINT unique_sku UNIQUE (sku),
    DROP COLUMN obsolete_field;

-- Change column type
ALTER TABLE orders ALTER COLUMN total TYPE DECIMAL(12,2);
```

## Related Specifications

- [stmt_create_table.md](./stmt_create_table.md) - Table creation
- [stmt_drop_table.md](./stmt_drop_table.md) - Table removal

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
