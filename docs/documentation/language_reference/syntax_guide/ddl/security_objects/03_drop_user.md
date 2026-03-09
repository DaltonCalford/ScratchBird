# DROP USER

[Prev](./02_alter_user.md) | [Next](./04_create_role.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Synopsis

Removes a database user.

## Syntax

```sql
DROP USER [ IF EXISTS ] user_name [, ...] [ CASCADE | RESTRICT ]
```

## Description

Drops a user and optionally reassigns or drops dependent objects.

## Examples

```sql
DROP USER temp_user;
DROP USER IF EXISTS temp_user;
DROP USER temp_user CASCADE;
```

## See Also

- [CREATE USER](01_create_user.md)
- [REASSIGN OWNED](../../security/README.md)
