# Firebird SQL Emulation

**Last Updated:** 2026-02-03

---

ScratchBird provides a Firebird SQL dialect parser so Firebird clients can
connect using familiar syntax. Emulation targets Firebird 5.0 syntax and
result shapes where possible, but runtime behavior follows the ScratchBird
engine core.

This guide documents:
- Features intended to behave identically to Firebird.
- Features that are emulated (metadata-only).
- Areas where behavior differs (engine model, storage, and admin tooling).

## Compatibility Matrix

| Area | Status | Source | Notes |
|------|--------|--------|-------|
| Core SQL (SELECT/INSERT/UPDATE/DELETE) | ScratchBird tracked | ScratchBird parser + executor | Syntax is Firebird-compatible; execution follows ScratchBird planner/executor. |
| DDL (tables, views, indexes, generators) | ScratchBird tracked | Catalog manager + DDL executor | Metadata stored in ScratchBird catalog; storage layout is ScratchBird-native. |
| System catalogs (RDB$/MON$/SEC$) | ScratchBird tracked | FirebirdCatalogHandler | Views map to ScratchBird runtime/catalog; untracked fields return NULL/0. |
| PSQL (procedures, triggers) | Emulated | PSQL compiler + executor | Firebird PSQL maps to ScratchBird PSQL features. |
| External tables/files | Restricted | Compatibility layer | File-backed features are not available in ScratchBird Alpha. |

## Key Differences

- **Transaction model:** Firebird uses MVCC with record versions; ScratchBird
  uses MGA with TIP and sweep/GC. Result semantics are compatible, but
  internal cleanup differs.
- **File-system features:** Firebird external files and OS-level utilities are
  restricted or unavailable.
- **Extensions/UDRs:** Only supported when explicitly implemented in ScratchBird.

## Practical Guidance

- Treat ScratchBird as Firebird-compatible SQL, not a drop-in server.
- Avoid Firebird-specific file system features and assume metadata-only
  behavior for unsupported options.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
