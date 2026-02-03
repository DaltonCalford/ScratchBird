# Types and Domains

**Last Updated:** 2026-02-03

---

Common types include:

- Integers (SMALLINT, INTEGER, BIGINT, INT128)
- Unsigned integers (UINT8/16/32/64)
- Floating‑point (REAL, DOUBLE PRECISION)
- Fixed‑point (DECIMAL, NUMERIC, MONEY)
- Strings (CHAR, VARCHAR, TEXT)
- Binary (BYTEA, BINARY, BLOB)
- Date/Time (DATE, TIME, TIMESTAMP, INTERVAL)
- JSON/JSONB, UUID, XML
- Arrays, ranges, vectors, spatial types

### Domains

```sql
CREATE DOMAIN positive_int AS INTEGER CHECK (VALUE > 0);
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
