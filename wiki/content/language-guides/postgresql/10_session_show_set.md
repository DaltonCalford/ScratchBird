# Session, SHOW, SET

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| SHOW parameter | ScratchBird tracked | Session/GUC compatibility layer | Supported parameters return values; unsupported return errors or NULL. |
| SET parameter | ScratchBird tracked | Session/GUC compatibility layer | Updates session-local settings. |
| RESET parameter | ScratchBird tracked | Session/GUC compatibility layer | Restores default values. |
| SET LOCAL | ScratchBird tracked | Transaction manager | Applies for current transaction only. |
| SET ROLE / RESET ROLE | ScratchBird tracked | Privilege manager | Role switching supported when configured. |

## Example

```sql
SHOW search_path;
SET search_path = app, public;
RESET search_path;
```

## Differences

- PostgreSQL GUCs are mapped where applicable; unsupported parameters return
  errors or are ignored based on the parser’s compatibility layer.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
