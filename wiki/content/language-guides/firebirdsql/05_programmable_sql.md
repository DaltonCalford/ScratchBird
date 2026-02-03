# Programmable SQL

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| CREATE PROCEDURE | Emulated | PSQL compiler + executor | Firebird PSQL mapped to ScratchBird PSQL. |
| CREATE FUNCTION | Emulated | PSQL compiler + executor | Firebird PSQL mapped to ScratchBird PSQL. |
| TRIGGERS | Emulated | Trigger manager + PSQL | Firebird trigger syntax mapped to ScratchBird triggers. |
| EXECUTE BLOCK | Emulated | PSQL compiler + executor | Mapped to ScratchBird EXECUTE BLOCK. |
| EXCEPTION handling | Emulated | PSQL runtime | Supported where ScratchBird PSQL provides equivalents. |

## Example

```sql
CREATE PROCEDURE bump_all AS
BEGIN
  UPDATE users SET active = 1;
END;
```

## Differences

- Some Firebird PSQL constructs are not supported unless ScratchBird PSQL
  provides equivalent behavior.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
