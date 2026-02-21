# DDL SEARCH INDEX: CREATE (Retired)
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Family README](../README.md)
- [Object README](README.md)

## Coverage
- Status: Not supported
- Command lifecycle note: `CREATE SEARCH INDEX` is rejected by native v3 parser.

## Parser Surface
```sql
-- Retired in native v3:
-- CREATE SEARCH INDEX ...
```

## Canonical Replacement
```sql
CREATE INDEX <index_name> ON <table_name> USING FULLTEXT (<search_columns>) [WITH (...)] ;
```
