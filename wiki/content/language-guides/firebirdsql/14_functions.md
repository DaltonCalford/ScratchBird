# Functions

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Function group | Status | Source | Notes |
|----------------|--------|--------|-------|
| Aggregates (COUNT/SUM/AVG/MIN/MAX) | ScratchBird tracked | Aggregate engine | Standard aggregate semantics. |
| Date/Time (CURRENT_DATE/NOW/EXTRACT) | ScratchBird tracked | Date/time engine | Firebird-compatible results where supported. |
| String (CHAR_LENGTH/SUBSTRING/LOWER/UPPER) | ScratchBird tracked | String engine | Standard string semantics. |
| Context (RDB$GET_CONTEXT/RDB$SET_CONTEXT) | Emulated | Context variable manager | Mapped to ScratchBird context variables. |
| Math (ABS/POWER/ROUND) | ScratchBird tracked | Expression engine | Standard math semantics. |
| System packages | Emulated | Compatibility layer | Available only if ScratchBird implements the package. |

## Differences

- Some Firebird-specific functions return mapped or simplified values.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
