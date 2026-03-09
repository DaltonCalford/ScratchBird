# ALTER INDEX

[Prev](./04_create_index.md) | [Next](./06_drop_index.md) | [Topic README](./README.md)

## Synopsis

Modifies an existing index.

## Syntax

```sql
ALTER INDEX [ IF EXISTS ] name RENAME TO new_name
ALTER INDEX [ IF EXISTS ] name SET ( storage_parameter = value [, ... ] )
ALTER INDEX [ IF EXISTS ] name RESET ( storage_parameter [, ... ] )
ALTER INDEX ALL IN TABLESPACE name [ OWNED BY role_name [, ... ] ] SET TABLESPACE new_tablespace [ NOWAIT ]
```

## Parameters

| Parameter | Description |
|-----------|-------------|
| `RENAME TO` | Change index name |
| `SET` | Change storage parameters |
| `RESET` | Reset parameters to defaults |
| `SET TABLESPACE` | Move to different tablespace |

## Examples

### Rename Index

```sql
ALTER INDEX idx_users_email RENAME TO idx_users_email_address;
```

### Change Fillfactor

```sql
ALTER INDEX idx_orders_date SET (fillfactor = 70);
```

### Move to Different Tablespace

```sql
ALTER INDEX idx_large_table SET TABLESPACE fast_ssd;
```

### Move All Indexes in Tablespace

```sql
ALTER INDEX ALL IN TABLESPACE pg_default SET TABLESPACE archive;
```

## See Also

- [CREATE INDEX](04_create_index.md)
- [DROP INDEX](06_drop_index.md)
