# Trigger Cheat Sheet

**Last Updated:** 2026-02-03

---

## Basic Syntax

```sql
CREATE TRIGGER trigger_name
BEFORE INSERT ON table_name
FOR EACH ROW
BEGIN
    -- trigger body
END;
```

## Common Variations

- BEFORE INSERT / UPDATE / DELETE
- AFTER INSERT / UPDATE / DELETE
- FOR EACH ROW (row‑level)
- FOR EACH STATEMENT (statement‑level)
- INSTEAD OF (views)

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
