<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# DROP VIEW

[Prev](./08_alter_view.md) | [Next](./10_create_sequence.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

## Synopsis

Removes one or more views from the database.

## Syntax

```sql
DROP VIEW [ IF EXISTS ] view_name [, ...] [ CASCADE | RESTRICT ]
```

## Parameters

| Parameter | Description |
|-----------|-------------|
| `IF EXISTS` | Suppress error if view does not exist |
| `view_name` | Name of view to drop. Supports qualified paths. |
| `CASCADE` | Drop dependent objects (views that reference this view) |
| `RESTRICT` | Refuse if dependent objects exist (default) |

## Description

`DROP VIEW` removes views from the database. By default, fails if other views depend on the view being dropped.

### Dependency Behavior

| Mode | Dependent Views Exist | Result |
|------|----------------------|--------|
| RESTRICT (default) | No | View dropped |
| RESTRICT (default) | Yes | Error: dependent objects exist |
| CASCADE | Yes | View and all dependent views dropped |

## Examples

### Basic Drop

```sql
-- Drop view
DROP VIEW order_summary;

-- With IF EXISTS
DROP VIEW IF EXISTS order_summary;
```

### Cascade Drop

```sql
-- Drop view and dependent views
DROP VIEW base_view CASCADE;
```

### Multiple Views

```sql
-- Drop multiple views
DROP VIEW view1, view2, view3;
```

### Qualified Path

```sql
-- Absolute path
DROP VIEW !:prod.reporting.sales_summary;
```

## Parser Acceptance Cases

```sql
DROP VIEW v1;
DROP VIEW IF EXISTS v1;
DROP VIEW v1 CASCADE;
DROP VIEW v1, v2 CASCADE;
```

## Parser Rejection Cases

```sql
-- Dependent views exist (RESTRICT default)
-- (view 'dependent_view' depends on 'base_view')
DROP VIEW base_view;  -- Error: dependent objects exist
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `undefined_view` | View doesn't exist (no IF EXISTS) |
| `dependent_objects` | Dependent views exist (RESTRICT) |

## See Also

- [CREATE VIEW](07_create_view.md)
- [ALTER VIEW](08_alter_view.md)
