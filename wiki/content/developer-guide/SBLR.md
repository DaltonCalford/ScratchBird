# SBLR and BLR Mapping

**Purpose:** Documents ScratchBird's internal bytecode format (SBLR) - the universal language that all SQL dialects compile to.

**Status:** Alpha documentation (in progress)

---

## Overview

SBLR (ScratchBird Language Representation) is the internal bytecode executed by the engine. All SQL dialects (PostgreSQL, MySQL, Firebird, native) compile to SBLR. The engine only understands SBLR - it has no knowledge of SQL dialects.

```
┌─────────────────────────────────────────────────────────────┐
│                    SQL DIALECTS                              │
│  PostgreSQL │ MySQL │ Firebird │ Native ScratchBird         │
└──────┬────────┬────────┬────────┬───────────────────────────┘
       │        │        │        │
       └────────┴────────┴────────┘
                    │
                    ▼
            ┌───────────────┐
            │  SBLR Bytecode│ ← Universal internal format
            │  (500+ opcodes)│
            └───────┬───────┘
                    │
                    ▼
            ┌───────────────┐
            │ Engine Core   │ ← Only understands SBLR
            └───────────────┘
```

---

## Bytecode Format

### Encoding Rules

- **Base opcodes:** 1 byte (0x00-0xFE)
- **END opcode:** 0x00
- **VERSION opcode:** 0x01
- **Extended prefix:** 0xFF

Extended opcodes use a 3-byte encoding:
```
0xFF <ext_opcode_lo> <ext_opcode_hi> [payload...]
```

Extended opcode value is 16-bit little-endian.

### Stream Structure

```
┌──────────────────────────────────────────────────────────┐
│ VERSION (0x01) │ version_byte                             │
├──────────────────────────────────────────────────────────┤
│ OPCODE_1       │ [operands...]                            │
├──────────────────────────────────────────────────────────┤
│ OPCODE_2       │ [operands...]                            │
├──────────────────────────────────────────────────────────┤
│ ...                                                       │
├──────────────────────────────────────────────────────────┤
│ END (0x00)                                                │
└──────────────────────────────────────────────────────────┘
```

---

## Opcode Categories

### Range Grouping Policy

| Range | Category |
|-------|----------|
| 0x00-0x0F | Stream and VM control |
| 0x10-0x1F | DDL and transaction control |
| 0x20-0x2F | Type markers |
| 0x30-0x3F | Literals and constants |
| 0x40-0x4F | References and assignments |
| 0x50-0x5F | Arithmetic |
| 0x60-0x6F | Comparisons |
| 0x70-0x7F | Boolean logic |
| 0x80-0x8F | Built-in functions |
| 0x90-0x9F | Constraints and modifiers |
| 0xA0-0xAF | Query structure |
| 0xB0-0xBF | Extended data types |
| 0xC0-0xCF | Optimizer hints and joins |
| 0xD0-0xDF | Sorting, limits, windows |
| 0xE0-0xEF | Stack and control flow |
| 0xF0-0xFE | Reserved for base opcodes |
| 0xFF | Extended prefix |

---

## Common Opcodes

### Stream Control

| Opcode | Value | Description |
|--------|-------|-------------|
| `OP_END` | 0x00 | End of bytecode stream |
| `OP_VERSION` | 0x01 | Bytecode version marker |
| `OP_NOP` | 0x02 | No operation |

### DDL Operations

| Opcode | Value | Description |
|--------|-------|-------------|
| `OP_CREATE_TABLE` | 0x10 | Create table definition |
| `OP_DROP_TABLE` | 0x11 | Drop table |
| `OP_ALTER_TABLE` | 0x12 | Alter table structure |
| `OP_CREATE_INDEX` | 0x13 | Create index |
| `OP_DROP_INDEX` | 0x14 | Drop index |
| `OP_CREATE_VIEW` | 0x15 | Create view |

### Transaction Control

| Opcode | Value | Description |
|--------|-------|-------------|
| `OP_BEGIN` | 0x18 | Begin transaction |
| `OP_COMMIT` | 0x19 | Commit transaction |
| `OP_ROLLBACK` | 0x1A | Rollback transaction |
| `OP_SAVEPOINT` | 0x1B | Create savepoint |

### Literals

| Opcode | Value | Description |
|--------|-------|-------------|
| `OP_LITERAL_NULL` | 0x30 | NULL value |
| `OP_LITERAL_BOOL` | 0x31 | Boolean value |
| `OP_LITERAL_INT32` | 0x32 | 32-bit integer |
| `OP_LITERAL_INT64` | 0x33 | 64-bit integer |
| `OP_LITERAL_FLOAT` | 0x34 | Float value |
| `OP_LITERAL_DOUBLE` | 0x35 | Double value |
| `OP_LITERAL_STRING` | 0x36 | String value |
| `OP_LITERAL_BLOB` | 0x37 | Binary data |

### References

| Opcode | Value | Description |
|--------|-------|-------------|
| `OP_TABLE_REF` | 0x40 | Reference table by name |
| `OP_COLUMN_REF` | 0x41 | Reference column by name |
| `OP_ALIAS` | 0x42 | Assign alias |
| `OP_PARAM` | 0x43 | Parameter placeholder |

### Arithmetic

| Opcode | Value | Description |
|--------|-------|-------------|
| `OP_ADD` | 0x50 | Addition |
| `OP_SUB` | 0x51 | Subtraction |
| `OP_MUL` | 0x52 | Multiplication |
| `OP_DIV` | 0x53 | Division |
| `OP_MOD` | 0x54 | Modulo |
| `OP_NEG` | 0x55 | Negation |

### Comparisons

| Opcode | Value | Description |
|--------|-------|-------------|
| `OP_EQ` | 0x60 | Equal |
| `OP_NE` | 0x61 | Not equal |
| `OP_LT` | 0x62 | Less than |
| `OP_LE` | 0x63 | Less than or equal |
| `OP_GT` | 0x64 | Greater than |
| `OP_GE` | 0x65 | Greater than or equal |
| `OP_IS_NULL` | 0x66 | IS NULL test |
| `OP_BETWEEN` | 0x67 | BETWEEN test |
| `OP_IN` | 0x68 | IN list test |
| `OP_LIKE` | 0x69 | LIKE pattern match |

### Boolean Logic

| Opcode | Value | Description |
|--------|-------|-------------|
| `OP_AND` | 0x70 | Logical AND |
| `OP_OR` | 0x71 | Logical OR |
| `OP_NOT` | 0x72 | Logical NOT |

### Query Structure

| Opcode | Value | Description |
|--------|-------|-------------|
| `OP_SELECT` | 0xA0 | Begin SELECT |
| `OP_INSERT` | 0xA1 | Begin INSERT |
| `OP_UPDATE` | 0xA2 | Begin UPDATE |
| `OP_DELETE` | 0xA3 | Begin DELETE |
| `OP_FROM` | 0xA4 | FROM clause |
| `OP_WHERE` | 0xA5 | WHERE clause |
| `OP_GROUP_BY` | 0xA6 | GROUP BY clause |
| `OP_HAVING` | 0xA7 | HAVING clause |
| `OP_ORDER_BY` | 0xA8 | ORDER BY clause |

---

## Extended Opcodes

### Reserved Extended Opcodes

| Opcode | Value | Description |
|--------|-------|-------------|
| `EXT_RENAME_OBJECT` | 0x0100 | Rename database object |
| `EXT_MOVE_OBJECT` | 0x0101 | Move object to schema |
| `EXT_SET_AUTOCOMMIT` | 0x0102 | Set autocommit mode |
| `EXT_COMMIT_RETAINING` | 0x0103 | Commit and retain transaction |
| `EXT_ROLLBACK_RETAINING` | 0x0104 | Rollback and retain transaction |
| `EXT_PREPARE_TRANSACTION` | 0x0105 | Prepare for 2PC |
| `EXT_COMMIT_PREPARED` | 0x0106 | Commit prepared transaction |
| `EXT_ROLLBACK_PREPARED` | 0x0107 | Rollback prepared transaction |
| `EXT_ALTER_DOMAIN` | 0x010E | Alter domain |
| `EXT_DROP_DOMAIN` | 0x010F | Drop domain |
| `EXT_NULL_SAFE_EQ` | 0x0200 | NULL-safe equality |
| `EXT_LIKE_ESCAPE` | 0x0201 | LIKE with escape |
| `EXT_ILIKE_ESCAPE` | 0x0202 | Case-insensitive LIKE |
| `EXT_PLACEHOLDER` | 0x0203 | Prepared statement placeholder |
| `EXT_SAVEPOINT_BEGIN` | 0x0212 | Begin savepoint |
| `EXT_SAVEPOINT_END` | 0x0213 | End savepoint |

---

## Example: SELECT Statement

```sql
SELECT id, name FROM users WHERE active = true ORDER BY name;
```

**SBLR Bytecode (simplified):**
```
OP_VERSION 0x01
OP_SELECT
  OP_COLUMN_REF "id"
  OP_COLUMN_REF "name"
OP_FROM
  OP_TABLE_REF "users"
OP_WHERE
  OP_COLUMN_REF "active"
  OP_LITERAL_BOOL true
  OP_EQ
OP_ORDER_BY
  OP_COLUMN_REF "name"
  OP_ASC
OP_END
```

---

## Example: INSERT Statement

```sql
INSERT INTO users (name, email) VALUES ('John', 'john@example.com');
```

**SBLR Bytecode:**
```
OP_VERSION 0x01
OP_INSERT
  OP_TABLE_REF "users"
  OP_COLUMN_REF "name"
  OP_COLUMN_REF "email"
OP_VALUES
  OP_LITERAL_STRING "John"
  OP_LITERAL_STRING "john@example.com"
OP_END
```

---

## Bytecode Generation Pipeline

```
SQL Text
    │
    ▼
┌──────────────────┐
│  Parser          │  Generate AST
│  (per dialect)   │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  AST             │  Abstract Syntax Tree
│  (ast_v2.h)      │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Semantic        │  Type checking, name resolution
│  Analyzer        │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Resolved AST    │  Types resolved, names bound
│  (resolved_ast)  │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Bytecode        │  Generate SBLR opcodes
│  Generator       │
└────────┬─────────┘
         │
         ▼
SBLR Bytecode
```

---

## Source Code Reference

| Component | Header | Implementation |
|-----------|--------|----------------|
| Opcode Definitions | `include/scratchbird/sblr/opcodes.h` | |
| AST | `include/scratchbird/parser/ast_v2.h` | `src/parser/ast_v2.cpp` |
| Resolved AST | `include/scratchbird/sblr/resolved_ast_v2.h` | |
| Semantic Analyzer | `include/scratchbird/sblr/semantic_analyzer_v2.h` | `src/sblr/semantic_analyzer_v2.cpp` |
| Bytecode Generator | `include/scratchbird/sblr/bytecode_generator_v2.h` | `src/sblr/bytecode_generator_v2.cpp` |
| Executor | `include/scratchbird/sblr/executor.h` | `src/sblr/executor.cpp` |

---

## Firebird BLR Mapping

ScratchBird's SBLR is inspired by Firebird's BLR (Binary Language Representation) but is not directly compatible. Key differences:

| Aspect | Firebird BLR | ScratchBird SBLR |
|--------|--------------|------------------|
| Opcode count | ~200 | 500+ |
| Extended opcodes | No | Yes (0xFF prefix) |
| Version scheme | Fixed | Extensible |
| JSON/spatial | No | Native support |

### BLR to SBLR Mapping

For Firebird compatibility mode, BLR can be mapped to SBLR:

| BLR Opcode | SBLR Equivalent |
|------------|-----------------|
| `blr_begin` | `OP_VERSION` + `OP_BEGIN` |
| `blr_end` | `OP_END` |
| `blr_literal` | `OP_LITERAL_*` |
| `blr_field` | `OP_COLUMN_REF` |
| `blr_relation` | `OP_TABLE_REF` |

---

## Validation Rules

The executor validates SBLR before execution:

1. **Version check:** Must start with `OP_VERSION`
2. **Termination:** Must end with `OP_END`
3. **Unknown opcodes:** Rejected (security measure)
4. **Stack balance:** Push/pop operations must balance
5. **Type compatibility:** Operand types must match

---

## Change Process

When adding or modifying opcodes:

1. Add or modify in `include/scratchbird/sblr/opcodes.h` first
2. Update the opcode registry documentation
3. Do NOT reuse retired opcode values
4. Keep opcode comments in header short and unambiguous

---

## Related Documents

- [Core Engine](Core-Engine.md) - SBLR executor details
- [Parsers](Parsers.md) - SQL to SBLR compilation
- [Architecture](Architecture.md) - Overall system design
