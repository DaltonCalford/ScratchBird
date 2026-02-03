# MySQL Emulation

**Last Updated:** 2026-02-03

---

ScratchBird provides a MySQL dialect parser for MySQL client compatibility.
Emulation targets MySQL 8.0 syntax where possible, but runtime behavior follows
ScratchBird’s engine core.

This guide documents:
- Features intended to behave identically to MySQL.
- Features that are emulated (metadata‑only).
- Areas where behavior differs (engine model, storage, and admin tooling).

---

## Works Identically (Common Paths)

- SELECT/INSERT/UPDATE/DELETE syntax, including joins and subqueries.
- Backtick identifiers and MySQL quoting rules.
- DDL for tables, views, and indexes.
- Common MySQL functions and operators used by applications.
- information_schema views mapped to ScratchBird catalog.

---

## Emulated or Mapped Behavior

- **CREATE DATABASE** maps to ScratchBird catalog entries under the database
  root; it does not create MySQL data directories.
- **SHOW** commands are mapped to ScratchBird catalog data.
- **information_schema** and **mysql.*** system schemas are mapped as views.

---

## Key Differences

- **Transaction model:** ScratchBird uses MGA with sweep/GC; MySQL uses InnoDB
  MVCC with redo/undo logs.
- **Storage engines:** MySQL storage engines (InnoDB/MyISAM) are not present.
- **File‑based utilities** (e.g., LOAD DATA INFILE) are restricted or mapped to
  ScratchBird COPY semantics.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
