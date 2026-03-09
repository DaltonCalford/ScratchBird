# Specification: CREATE SEQUENCE Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:900`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:4256`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_ddl.cpp`

## Synopsis

The CREATE SEQUENCE statement creates a sequence generator for producing unique numeric values, commonly used for auto-incrementing primary keys.

## Specification

### EBNF Grammar

```ebnf
create_sequence_stmt ::=
    "CREATE" [ "OR" "REPLACE" | "TEMPORARY" | "TEMP" ]
    "SEQUENCE" [ "IF" "NOT" "EXISTS" ]
    schema_path
    [ sequence_options ]

sequence_options ::=
    [ "AS" data_type ]
    [ "START" [ "WITH" ] integer ]
    [ "RESTART" [ "WITH" ] integer ]
    [ "INCREMENT" [ "BY" ] integer ]
    [ "MINVALUE" integer | "NO" "MINVALUE" ]
    [ "MAXVALUE" integer | "NO" "MAXVALUE" ]
    [ "CACHE" integer ]
    [ [ "NO" ] "CYCLE" ]
    [ "OWNED" "BY" ( schema_path "." column | "NONE" ) ]
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:900
class CreateSequenceStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateSequenceStmt; }
    
    bool or_replace = false;
    bool temporary = false;
    bool if_not_exists = false;

    SchemaPath sequence_path;

    // Sequence options
    std::optional<int64_t> start_with;
    std::optional<int64_t> increment_by;
    std::optional<int64_t> min_value;
    std::optional<int64_t> max_value;
    bool no_min_value = false;
    bool no_max_value = false;
    std::optional<int64_t> cache;
    bool cycle = false;

    // Owned by column
    SchemaPath owned_by_table;
    StringPool::StringId owned_by_column = StringPool::INVALID_ID;
    bool has_owned_by = false;
};
```

## Examples

```sql
-- Basic sequence
CREATE SEQUENCE order_seq;

-- Sequence with options
CREATE SEQUENCE invoice_num_seq
    START WITH 1000
    INCREMENT BY 1
    MINVALUE 1000
    MAXVALUE 999999
    NO CYCLE;

-- Sequence owned by column (auto-drop with table)
CREATE SEQUENCE user_id_seq OWNED BY users.id;

-- Temporary sequence
CREATE TEMPORARY SEQUENCE temp_seq CACHE 100;
```

## Related Specifications

- [stmt_alter_sequence.md](./stmt_alter_sequence.md) - Sequence modification
- [stmt_drop_sequence.md](./stmt_drop_sequence.md) - Sequence removal

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
