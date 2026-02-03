# PostgreSQL Emulation

**Last Updated:** 2026-02-03

---

ScratchBird provides a PostgreSQL dialect parser so PostgreSQL clients can
connect using familiar syntax. Emulation targets PostgreSQL 16 syntax and
result shapes where possible, but runtime behavior follows the ScratchBird
engine core.

This guide documents:
- Features intended to behave identically to PostgreSQL.
- Features that are emulated (metadata-only).
- Areas where behavior differs (engine model, storage, and admin tooling).

## Compatibility Matrix

| Area | Status | Source | Notes |
|------|--------|--------|-------|
| Core SQL (SELECT/INSERT/UPDATE/DELETE) | ScratchBird tracked | ScratchBird SQL parser + executor | Syntax is PostgreSQL-compatible; execution follows ScratchBird planner/executor. |
| DDL (tables, views, indexes, sequences) | ScratchBird tracked | Catalog manager + DDL executor | Metadata stored in ScratchBird catalog; storage layout is ScratchBird-native. |
| System catalogs (pg_catalog/pg_stat) | ScratchBird tracked | Virtual catalog views | All views are exposed; columns not tracked return NULL/0. |
| Extensions | Emulated (metadata-only) | Catalog metadata | CREATE EXTENSION is accepted when configured; behavior depends on ScratchBird features. |
| File-backed features (COPY PROGRAM, file paths) | Restricted | Compatibility layer | Disallowed or limited to STDIN/STDOUT streaming. |
| Transaction model | ScratchBird tracked | Core MGA engine | MGA + TIP with sweep/GC; not PostgreSQL MVCC/WAL. |

## Key Differences

- **Transaction Model:** PostgreSQL uses MVCC with WAL and VACUUM; ScratchBird
  uses Firebird-style MGA with TIP and sweep/GC.
- **WAL:** Not required for correctness in ScratchBird Alpha.
- **Server-level file features:** PostgreSQL file operations (COPY PROGRAM,
  file-path COPY) are restricted or unavailable.
- **Extensions:** CREATE EXTENSION is metadata-only unless explicitly supported.

## Practical Guidance

- Treat ScratchBird as PostgreSQL-compatible SQL, not a drop-in server.
- Avoid PostgreSQL file-system features and extension assumptions.
- Validate behaviors that depend on WAL, VACUUM, or pg_stat internals.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
