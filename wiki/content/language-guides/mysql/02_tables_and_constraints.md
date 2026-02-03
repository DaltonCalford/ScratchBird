# Tables and Constraints

**Last Updated:** 2026-02-03

---

## Identical Behavior

```sql
CREATE TABLE users (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    email VARCHAR(255) UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

Constraints include PRIMARY KEY, UNIQUE, FOREIGN KEY, CHECK, and NOT NULL.

## Emulated / Mapped

- ENGINE=InnoDB/MyISAM is accepted as metadata but does not change storage.
- ROW_FORMAT and other storage options may be stored but are not guaranteed to
  affect physical layout.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
