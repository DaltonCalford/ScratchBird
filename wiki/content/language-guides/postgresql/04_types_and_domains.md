# Types and Domains

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Type/Feature | Status | Source | Notes |
|--------------|--------|--------|-------|
| Integer types (SMALLINT/INTEGER/BIGINT) | ScratchBird tracked | Core type system | Mapped to ScratchBird integer types. |
| Numeric/Decimal | ScratchBird tracked | Core type system | Mapped to ScratchBird numeric types. |
| Floating types (REAL/DOUBLE) | ScratchBird tracked | Core type system | Mapped to ScratchBird float types. |
| Text types (TEXT/VARCHAR/CHAR) | ScratchBird tracked | Core type system | Length constraints enforced by ScratchBird. |
| BYTEA | ScratchBird tracked | Core type system | Binary storage mapped to ScratchBird blob/bytea. |
| Date/Time types | ScratchBird tracked | Core type system | DATE/TIME/TIMESTAMP/INTERVAL mapped to ScratchBird types. |
| JSON/JSONB | ScratchBird tracked | JSON type system | Stored as ScratchBird JSON with compatible operators. |
| UUID | ScratchBird tracked | Core type system | Mapped to ScratchBird UUID type. |
| Arrays | ScratchBird tracked | Array type system | Supports array literals and operators implemented by ScratchBird. |
| Domains | ScratchBird tracked | Catalog manager + executor | CHECK constraints enforced at DML time. |

## Examples

```sql
CREATE DOMAIN positive_int AS INTEGER CHECK (VALUE > 0);
```

## Differences

- Type modifiers and storage-level details are enforced by ScratchBird’s type
  system, which may differ from PostgreSQL in edge cases.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
