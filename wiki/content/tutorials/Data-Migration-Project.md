# Data Migration Project

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

A high-level migration plan for moving data into ScratchBird.

## Steps

1) Inventory source schema and data volumes.
2) Map types and constraints to ScratchBird.
3) Create target schema and indexes.
4) Bulk load data in batches.
5) Validate row counts and checksums.
6) Run functional verification queries.
7) Cut over and monitor.

## References

- [Migration Overview](../migration/Migration-Overview.md)
- [From Firebird](../migration/From-Firebird.md)
- [From PostgreSQL](../migration/From-PostgreSQL.md)
- [From MySQL](../migration/From-MySQL.md)
- [Performance Tuning](../user-guides/Performance-Tuning.md)
