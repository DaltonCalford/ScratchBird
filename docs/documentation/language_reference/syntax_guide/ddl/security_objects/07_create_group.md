# CREATE GROUP

[Prev](./06_drop_role.md) | [Next](./08_alter_group.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

## Synopsis

Creates a group - a legacy-compatible role container for backward compatibility with Firebird/MySQL.

## Syntax

```sql
CREATE GROUP [ environment_path ] group_name
    [ [ WITH ] option [ ... ] ]

where option is same as CREATE ROLE
```

## Description

`CREATE GROUP` is a legacy synonym for `CREATE ROLE` with `NOLOGIN`. Groups exist for compatibility with:
- Firebird GROUP syntax
- MySQL GROUP syntax
- Legacy authorization models

In modern ScratchBird, prefer `CREATE ROLE` for new designs.

## User, Role, Group Relationship

```
┌─────────────────────────────────────────────────────────────┐
│                    ENVIRONMENT-SCOPED                        │
│                                                              │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐     │
│   │    USER     │    │    ROLE     │    │   GROUP     │     │
│   │  (LOGIN)    │    │  (NOLOGIN)  │    │  (NOLOGIN)  │     │
│   │             │    │             │    │             │     │
│   │ Individual  │    │ Privilege   │    │ Legacy      │     │
│   │ identity    │    │ grouping    │    │ grouping    │     │
│   └──────┬──────┘    └──────┬──────┘    └──────┬──────┘     │
│          │                  │                  │             │
│          └──────────────────┴──────────────────┘             │
│                     All can be members                        │
│                     of each other                             │
└─────────────────────────────────────────────────────────────┘
```

## Examples

### Legacy Group

```sql
-- Firebird-compatible group
CREATE GROUP accounting;

-- Add users to group
ALTER GROUP accounting ADD USER john;
ALTER GROUP accounting ADD USER jane;
```

### Equivalent Modern Role

```sql
-- Same functionality with ROLE
CREATE ROLE accounting;
GRANT accounting TO john;
GRANT accounting TO jane;
```

## See Also

- [CREATE ROLE](04_create_role.md)
- [DROP GROUP](09_drop_group.md)
