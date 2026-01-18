# Sequences

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

Sequences (generators) produce unique numeric values, typically for primary keys
and identity columns. ScratchBird favors SQL-style syntax for sequence access.

## CREATE SEQUENCE

```
CREATE SEQUENCE [IF NOT EXISTS] sequence_name
    [AS data_type]
    [START WITH start_value]
    [INCREMENT BY increment_value]
    [MINVALUE min_value | NO MINVALUE]
    [MAXVALUE max_value | NO MAXVALUE]
    [CACHE cache_size]
    [[NO] CYCLE];
```

Example:

```
CREATE SEQUENCE user_id_seq
    START WITH 1000
    INCREMENT BY 1;
```

## ALTER SEQUENCE

```
ALTER SEQUENCE [IF EXISTS] sequence_name
    [AS data_type]
    [INCREMENT BY increment_value]
    [MINVALUE min_value | NO MINVALUE]
    [MAXVALUE max_value | NO MAXVALUE]
    [RESTART [WITH restart_value]]
    [CACHE cache_size]
    [[NO] CYCLE];
```

## DROP SEQUENCE

```
DROP SEQUENCE [IF EXISTS] sequence_name [, ...] [CASCADE | RESTRICT];
```

## Using sequences

SQL-style access:

- NEXT VALUE FOR sequence_name
- CURRENT VALUE FOR sequence_name
- SET sequence_name TO value

Examples:

```
INSERT INTO users (id, username)
VALUES (NEXT VALUE FOR user_id_seq, 'jdoe');

CREATE TABLE invoices (
  invoice_id BIGINT PRIMARY KEY DEFAULT (NEXT VALUE FOR invoice_id_seq),
  details TEXT
);
```

## Behavior notes

- Sequences are transaction-independent; values are not rolled back.
- Caching improves throughput but can create gaps after crashes or rollbacks.
- Sequences can be linked to identity columns; CASCADE removes dependencies.

## References

- `docs/specifications/ddl/DDL_SEQUENCES.md`
- `docs/specifications/types/UUID_IDENTITY_COLUMNS.md`
- `docs/audit/languages/native/03_indexes_views_sequences.md`
