# Programmable SQL

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| CREATE FUNCTION (plpgsql syntax) | Emulated | PSQL compiler + executor | PostgreSQL syntax is accepted and mapped to ScratchBird PSQL. |
| CREATE PROCEDURE | Emulated | PSQL compiler + executor | Procedure body is compiled to ScratchBird PSQL. |
| DO / anonymous blocks | Emulated | PSQL compiler + executor | Mapped to ScratchBird EXECUTE BLOCK. |
| Exception handling | Emulated | PSQL runtime | Supported where ScratchBird PSQL provides equivalents. |
| Cursors | Emulated | Executor | Only cursor forms implemented by ScratchBird are supported. |

## Example

```sql
CREATE FUNCTION add_one(x INTEGER) RETURNS INTEGER AS $$
BEGIN
    RETURN x + 1;
END;$$ LANGUAGE plpgsql;

CREATE PROCEDURE bump_all()
LANGUAGE plpgsql AS $$
BEGIN
    UPDATE users SET active = TRUE;
END;$$;
```

## Differences

- Some PL/pgSQL constructs are not supported unless ScratchBird PSQL provides
  equivalent behavior.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
