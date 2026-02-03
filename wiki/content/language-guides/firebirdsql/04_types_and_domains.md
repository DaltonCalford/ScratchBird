# Types and Domains

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Type/Feature | Status | Source | Notes |
|--------------|--------|--------|-------|
| Integer types | ScratchBird tracked | Core type system | SMALLINT/INTEGER/BIGINT mapped to ScratchBird types. |
| Numeric/Decimal | ScratchBird tracked | Core type system | NUMERIC/DECIMAL mapped to ScratchBird numeric types. |
| Floating types | ScratchBird tracked | Core type system | FLOAT/DOUBLE PRECISION mapped to ScratchBird. |
| Text types | ScratchBird tracked | Core type system | CHAR/VARCHAR/BLOB SUB_TYPE TEXT mapped to ScratchBird. |
| Binary/BLOB | ScratchBird tracked | Core type system | BLOB/BINARY mapped to ScratchBird storage. |
| Date/Time types | ScratchBird tracked | Core type system | DATE/TIME/TIMESTAMP mapped to ScratchBird types. |
| BOOLEAN | ScratchBird tracked | Core type system | BOOLEAN mapped to ScratchBird boolean. |
| Domains | ScratchBird tracked | Catalog manager + executor | CHECK constraints enforced at DML time. |

## Example

```sql
CREATE DOMAIN positive_int AS INTEGER CHECK (VALUE > 0);
```

## Differences

- Type modifiers and storage-level details are enforced by ScratchBird’s type
  system, which may differ from Firebird in edge cases.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
