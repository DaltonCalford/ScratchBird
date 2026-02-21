# DDL Object: SEARCH INDEX (Retired Alias Surface)
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Data Storage README](../README.md)

## Summary
- Family: Data Storage
- Lifecycle status: Retired alias surface
- Lifecycle note: Native v3 no longer accepts `SEARCH INDEX` as a command family.
- Runtime note: Use canonical `INDEX` lifecycle with `USING FULLTEXT`.

## Canonical Replacement
- CREATE: `CREATE INDEX <index_name> ON <table_name> USING FULLTEXT (<columns>) [WITH (...)]`
- ALTER: `ALTER INDEX <index_name> REBUILD [ONLINE|OFFLINE] [WITH (...)]`
- DROP: `DROP INDEX <index_name>`

## Active Documentation
- [INDEX README](../index/README.md)
- [INDEX CREATE](../index/create.md)
- [INDEX ALTER](../index/alter.md)
- [INDEX DROP](../index/drop.md)
