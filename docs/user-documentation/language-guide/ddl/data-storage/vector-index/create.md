# DDL VECTOR INDEX: CREATE (Retired)
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Family README](../README.md)
- [Object README](README.md)

## Coverage
- Status: Not supported
- Command lifecycle note: `CREATE VECTOR INDEX` is rejected by native v3 parser.

## Parser Surface
```sql
-- Retired in native v3:
-- CREATE VECTOR INDEX ...
```

## Canonical Replacement
```sql
CREATE INDEX <index_name> ON <table_name> USING HNSW (<vector_column>) WITH (<options>);
```
