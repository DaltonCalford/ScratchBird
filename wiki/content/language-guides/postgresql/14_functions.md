# Functions

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Function group | Status | Source | Notes |
|----------------|--------|--------|-------|
| Aggregates (COUNT/SUM/AVG/MIN/MAX) | ScratchBird tracked | Aggregate engine | Standard aggregate semantics. |
| Date/Time (NOW/CURRENT_DATE/EXTRACT) | ScratchBird tracked | Date/time engine | PostgreSQL-compatible results where supported. |
| String (LENGTH/SUBSTRING/LOWER/UPPER/CONCAT) | ScratchBird tracked | String engine | Standard string semantics. |
| JSON (JSON_BUILD_OBJECT/JSONB_* ) | ScratchBird tracked | JSON engine | Supported where ScratchBird JSON functions exist. |
| Array (ARRAY_LENGTH/ARRAY_APPEND/UNNEST) | ScratchBird tracked | Array engine | Supported where ScratchBird array functions exist. |
| Extension-provided | Emulated | Compatibility layer | Available only if ScratchBird implements the function. |

## Differences

- Some server-specific functions return mapped or simplified values.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
