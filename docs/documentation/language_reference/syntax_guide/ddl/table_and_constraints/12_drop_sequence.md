<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# DROP SEQUENCE

[Prev](./11_alter_sequence.md) | [Next](../routines_and_code/README.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

## Synopsis

Removes one or more sequences from the database.

## Syntax

```sql
DROP SEQUENCE [ IF EXISTS ] sequence_name [, ...] [ CASCADE | RESTRICT ]
```

## Parameters

| Parameter | Description |
|-----------|-------------|
| `IF EXISTS` | Suppress error if sequence does not exist |
| `sequence_name` | Name of sequence to drop. Supports paths. |
| `CASCADE` | Drop dependent objects (default columns, etc.) |
| `RESTRICT` | Refuse if dependent objects exist (default) |

## Description

`DROP SEQUENCE` removes sequences. Automatically dropped when owned column is dropped (if OWNED BY set).

## Examples

### Basic Drop

```sql
DROP SEQUENCE order_id_seq;
DROP SEQUENCE IF EXISTS temp_seq;
```

### Cascade Drop

```sql
-- Drop even if used as DEFAULT
DROP SEQUENCE users_id_seq CASCADE;
```

### Multiple Sequences

```sql
DROP SEQUENCE seq1, seq2, seq3;
```

## Parser Acceptance Cases

```sql
DROP SEQUENCE s1;
DROP SEQUENCE IF EXISTS s1;
DROP SEQUENCE s1 CASCADE;
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `undefined_sequence` | Sequence doesn't exist |
| `dependent_objects` | Used by columns (RESTRICT) |

## See Also

- [CREATE SEQUENCE](10_create_sequence.md)
- [ALTER SEQUENCE](11_alter_sequence.md)
