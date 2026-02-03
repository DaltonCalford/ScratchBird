# Indexes, Views, Sequences

**Last Updated:** 2026-02-03

---

```sql
CREATE INDEX idx_users_email ON users (email);
CREATE VIEW active_users AS SELECT * FROM users WHERE active = 1;
```

MySQL uses AUTO_INCREMENT instead of standalone sequences.

## Differences

- MySQL FULLTEXT and SPATIAL indexes are mapped to ScratchBird’s index
  subsystem when supported.
- Index hints are parsed but may not influence ScratchBird’s planner.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
