# DDL VECTOR INDEX: DROP (Retired)
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Family README](../README.md)
- [Object README](README.md)

## Coverage
- Status: Not supported
- Command lifecycle note: `DROP VECTOR INDEX` is rejected by native v3 parser.

## Parser Surface
```sql
-- Retired in native v3:
-- DROP VECTOR INDEX ...
```

## Canonical Replacement
```sql
DROP INDEX <index_name>;
```
