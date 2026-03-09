# Specification: ALTER INDEX Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1956`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:8300`

## Synopsis

The ALTER INDEX statement modifies index properties, enables/disables indexes, rebuilds indexes, and manages index options.

## Specification

### EBNF Grammar

```ebnf
alter_index_stmt ::=
    "ALTER" "INDEX" [ "IF" "EXISTS" ]
    schema_path
    ( "RENAME" "TO" new_name
    | "SET" "TABLESPACE" tablespace_name
    | "SET" "(" index_option_list ")"
    | "RESET" "(" option_name_list ")"
    | "REBUILD" [ "ONLINE" | "OFFLINE" ]
    | "ACTIVE"
    | "INACTIVE"
    | "REBALANCE"
    | "RELOCATE"
    | "LIGHT" "SCAN"
    | "DIAGNOSTIC" "SCAN"
    )
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1956
class AlterIndexStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterIndexStmt; }
    
    SchemaPath index_path;
    AlterIndexAction action = AlterIndexAction::ACTIVE;
    IndexOptions options;
    std::vector<IndexOptionAssignment> option_assignments;
    std::vector<StringPool::StringId> reset_options;

    bool defaults_scope = false;
    IndexType defaults_index_type = IndexType::BTREE;
    StringPool::StringId defaults_index_type_name = StringPool::INVALID_ID;

    bool has_target_filespace = false;
    SchemaPath target_filespace;
    IndexMaintenanceMode mode = IndexMaintenanceMode::DEFAULT;
};

enum class AlterIndexAction : uint8_t {
    ACTIVE, INACTIVE, SET_OPTIONS, RESET_OPTIONS, REBUILD,
    REBALANCE, RELOCATE, LIGHT_SCAN, DIAGNOSTIC_SCAN
};

enum class IndexMaintenanceMode : uint8_t {
    DEFAULT, ONLINE, OFFLINE
};
```

## Examples

```sql
-- Rename index
ALTER INDEX idx_old_name RENAME TO idx_new_name;

-- Rebuild index
ALTER INDEX idx_users_email REBUILD;

-- Rebuild online
ALTER INDEX idx_large_table REBUILD ONLINE;

-- Set option
ALTER INDEX idx_orders SET (bloom_filter_enabled = true, bloom_fpr = 0.01);

-- Disable index
ALTER INDEX idx_temp INACTIVE;

-- Enable index
ALTER INDEX idx_temp ACTIVE;

-- Move to different tablespace
ALTER INDEX idx_data SET TABLESPACE fast_storage;
```

## Related Specifications

- [stmt_create_index.md](./stmt_create_index.md) - Index creation
- [stmt_drop_index.md](./stmt_drop_index.md) - Index removal

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
