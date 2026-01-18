# REST API Design

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

A starter checklist for building a REST API backed by ScratchBird.

## Steps

1) Model your resources (tables + constraints).
2) Add indexes for lookup patterns.
3) Wrap writes in explicit transactions.
4) Apply role-based permissions and RLS/CLS if needed.
5) Use parameterized queries in the API layer.

## References

- [Transactions](../user-guides/Transactions.md)
- [Indexes](../user-guides/Indexes.md)
- [Security](../user-guides/Security.md)
- [SQL Syntax](../reference/SQL-Syntax.md)
