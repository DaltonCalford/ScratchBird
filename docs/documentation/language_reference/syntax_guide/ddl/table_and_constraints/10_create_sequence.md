<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# CREATE SEQUENCE

[Prev](./09_drop_view.md) | [Next](./11_alter_sequence.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

## Synopsis

Creates a sequence object for generating unique numeric identifiers.

## Syntax

```sql
CREATE [ TEMPORARY | TEMP ] SEQUENCE [ IF NOT EXISTS ] sequence_name
    [ AS data_type ]
    [ START [ WITH ] start ]
    [ INCREMENT [ BY ] increment ]
    [ MINVALUE minvalue | NO MINVALUE ]
    [ MAXVALUE maxvalue | NO MAXVALUE ]
    [ CACHE cache ]
    [ [ NO ] CYCLE ]
    [ OWNED BY { table_name.column_name | NONE } ]
```

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `TEMPORARY`, `TEMP` | keyword | - | Session-local sequence |
| `IF NOT EXISTS` | keyword | - | Skip if exists (no error) |
| `sequence_name` | identifier | - | Sequence name. Supports paths. |
| `data_type` | type | BIGINT | SMALLINT, INTEGER, BIGINT |
| `START` | integer | 1 | First value to return |
| `INCREMENT` | integer | 1 | Increment between values |
| `MINVALUE` | integer | data type min | Minimum value |
| `MAXVALUE` | integer | data type max | Maximum value |
| `CACHE` | integer | 1 | Values to preallocate |
| `CYCLE` | boolean | NO CYCLE | Restart at min after max reached |
| `OWNED BY` | column | NONE | Auto-drop when column dropped |

## Description

Sequences generate unique numbers, commonly used for:
- Surrogate primary keys
- Order numbers
- Ticket/issue numbers
- Batch identifiers

### Sequence Functions

| Function | Description |
|----------|-------------|
| `nextval('sequence_name')` | Get next value, advance sequence |
| `currval('sequence_name')` | Get current session's last value |
| `lastval()` | Get last nextval in session (any sequence) |
| `setval('sequence_name', value)` | Set sequence value |

## Examples

### Basic Sequence

```sql
-- Default sequence (1, 2, 3, ...)
CREATE SEQUENCE order_id_seq;

-- Use in INSERT
INSERT INTO orders (id, amount) 
VALUES (nextval('order_id_seq'), 100.00);
```

### Custom Sequence

```sql
-- Start at 1000, increment by 5
CREATE SEQUENCE ticket_number_seq
    START 1000
    INCREMENT 5;

-- Result: 1000, 1005, 1010, ...
```

### Bounded Sequence

```sql
-- Limited range with cycling
CREATE SESSION small_cycle_seq
    AS SMALLINT
    START 1
    INCREMENT 1
    MAXVALUE 100
    CYCLE;

-- Result: 1, 2, ..., 100, 1, 2, ...
```

### High-Performance Sequence

```sql
-- Cache for reduced contention
CREATE SEQUENCE high_freq_seq
    CACHE 1000;

-- 1000 values preallocated per session
```

### Owned Sequence (Auto-Drop)

```sql
-- Auto-drop when column is dropped
CREATE SEQUENCE users_id_seq
    OWNED BY users.id;

-- Applied as DEFAULT
ALTER TABLE users 
    ALTER COLUMN id 
    SET DEFAULT nextval('users_id_seq');
```

### Negative Increment

```sql
-- Countdown sequence
CREATE SEQUENCE countdown_seq
    START 1000
    INCREMENT -1
    MINVALUE 1;

-- Result: 1000, 999, 998, ..., 1
```

## Data Type Limits

| Type | Min | Max |
|------|-----|-----|
| SMALLINT | -32768 | 32767 |
| INTEGER | -2147483648 | 2147483647 |
| BIGINT | -9223372036854775808 | 9223372036854775807 |

## Parser Acceptance Cases

```sql
CREATE SEQUENCE s1;
CREATE SEQUENCE IF NOT EXISTS s1;
CREATE SEQUENCE s1 START 100;
CREATE SEQUENCE s1 INCREMENT 10 CACHE 100;
CREATE TEMPORARY SEQUENCE s1;
```

## Parser Rejection Cases

```sql
-- MINVALUE > MAXVALUE
CREATE SEQUENCE s1 MINVALUE 100 MAXVALUE 1;  -- Error: invalid

-- START out of range
CREATE SEQUENCE s1 AS SMALLINT START 100000;  -- Error: out of range
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `duplicate_sequence` | Sequence exists (no IF NOT EXISTS) |
| `invalid_sequence_definition` | Conflicting options |

## See Also

- [ALTER SEQUENCE](11_alter_sequence.md)
- [DROP SEQUENCE](12_drop_sequence.md)
- [CREATE TABLE](01_create_table.md) - IDENTITY columns
