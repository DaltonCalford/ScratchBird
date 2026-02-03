# Session, SHOW, SET

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| SET TRANSACTION | ScratchBird tracked | Transaction manager | Firebird transaction options mapped to ScratchBird. |
| SET TIME ZONE | Emulated | Session context | Supported when ScratchBird timezone mapping is enabled. |
| SET ROLE | ScratchBird tracked | Privilege manager | Role switching supported when configured. |
| SHOW/GET CONTEXT | Emulated | Context variable manager | `RDB$GET_CONTEXT`/`RDB$SET_CONTEXT` map to ScratchBird context variables. |

## Example

```sql
SET TRANSACTION READ COMMITTED;
SELECT RDB$GET_CONTEXT('SYSTEM', 'ENGINE_VERSION') FROM RDB$DATABASE;
```

## Differences

- Firebird session parameters are mapped where applicable; unsupported values
  return errors or are ignored.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
