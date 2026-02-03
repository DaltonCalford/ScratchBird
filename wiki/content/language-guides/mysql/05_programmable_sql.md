# Programmable SQL

**Last Updated:** 2026-02-03

---

Stored procedures and triggers use MySQL syntax:

```sql
DELIMITER //
CREATE PROCEDURE bump_all()
BEGIN
    UPDATE users SET active = 1;
END //
DELIMITER ;
```

Triggers:

```sql
CREATE TRIGGER users_bi
BEFORE INSERT ON users
FOR EACH ROW
SET NEW.created_at = CURRENT_TIMESTAMP;
```

## Differences

- Certain MySQL procedural features may map to ScratchBird PSQL equivalents or
  be unsupported.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
