# Specification: ALTER SEQUENCE Statement

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

The ALTER SEQUENCE statement modifies sequence generator properties including increment, min/max values, and restart points.

## Specification

### EBNF Grammar

```ebnf
alter_sequence_stmt ::=
    "ALTER" "SEQUENCE" [ "IF" "EXISTS" ]
    schema_path
    [ sequence_option ... ]
    [ "RESTART" [ "WITH" start_value ] ]

sequence_option ::=
    "AS" data_type
  | "INCREMENT" [ "BY" ] integer
  | "MINVALUE" integer | "NO" "MINVALUE"
  | "MAXVALUE" integer | "NO" "MAXVALUE"
  | "START" [ "WITH" ] integer
  | "CACHE" integer
  | [ "NO" ] "CYCLE"
  | "OWNED" "BY" ( schema_path "." column | "NONE" )
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:931
class AlterSequenceStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterSequenceStmt; }
    
    SchemaPath sequence_path;

    std::optional<int64_t> increment_by;
    std::optional<int64_t> min_value;
    std::optional<int64_t> max_value;
    std::optional<int64_t> restart_with;
    std::optional<int64_t> cache;
    std::optional<bool> cycle;
};
```

## Examples

```sql
-- Change increment
ALTER SEQUENCE order_seq INCREMENT BY 10;

-- Set new max value
ALTER SEQUENCE user_id_seq MAXVALUE 1000000;

-- Restart sequence
ALTER SEQUENCE order_seq RESTART WITH 1;

-- Disable cycling
ALTER SEQUENCE small_seq NO CYCLE;

-- Change cache size
ALTER SEQUENCE high_volume_seq CACHE 100;

-- Change ownership
ALTER SEQUENCE user_id_seq OWNED BY users.id;
```

## Related Specifications

- [stmt_create_sequence.md](./stmt_create_sequence.md) - Sequence creation
- [stmt_drop_sequence.md](./stmt_drop_sequence.md) - Sequence removal

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
