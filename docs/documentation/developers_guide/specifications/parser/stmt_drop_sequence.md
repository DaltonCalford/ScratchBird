# Specification: DROP SEQUENCE Statement

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

The DROP SEQUENCE statement removes one or more sequence generators from the database.

## Specification

### EBNF Grammar

```ebnf
drop_sequence_stmt ::=
    "DROP" "SEQUENCE" [ "IF" "EXISTS" ]
    schema_path ("," schema_path )*
    [ "CASCADE" | "RESTRICT" ]
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2051
class DropSequenceStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropSequenceStmt; }
    
    bool if_exists = false;
    std::vector<SchemaPath> sequences;
    bool cascade = false;
};
```

## Examples

```sql
-- Drop sequence
DROP SEQUENCE order_seq;

-- Drop if exists
DROP SEQUENCE IF EXISTS temp_seq;

-- Drop multiple sequences
DROP SEQUENCE seq1, seq2, seq3;

-- Drop with cascade
DROP SEQUENCE user_id_seq CASCADE;
```

## Related Specifications

- [stmt_create_sequence.md](./stmt_create_sequence.md) - Sequence creation
- [stmt_alter_sequence.md](./stmt_alter_sequence.md) - Sequence modification

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
