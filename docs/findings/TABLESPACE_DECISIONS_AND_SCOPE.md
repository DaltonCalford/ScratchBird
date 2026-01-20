# Tablespace Decisions and Scope (Alpha)

## Decisions (Locked)
- IDs: tablespace 0 is primary, tablespace 1 is reserved (future), custom tablespaces are 2..65535.
- TablespaceHeader: expand name to 63 chars and bump header format (Alpha only).
- DDL scope: only V2 parser can create/alter physical tablespaces.
  - Emulated parsers do not create files. Firebird tablespace DDL is rejected for parity.
- Placement: CREATE TABLE/INDEX honors TABLESPACE clause and uses schema default if omitted.
- DML: INSERT/SELECT/UPDATE/DELETE are fully tablespace-aware (GPID path).
- Root pages: replace root_page with root_gpid everywhere.
- Startup: strict by default; allow missing tablespaces only with explicit recovery mode.
- ATTACH: strict database UUID by default; allow FORCE / ALLOW_MISMATCH override.
- Migration: normal DML never moves tuples; offline/online migrations rewrite pages.
- Multi-file: implement now. DDL uses ALTER TABLESPACE ... ADD FILE / ADD DATAFILE.
- Counters: maintain per-tablespace table_count and index_count.
- Backup/restore: include all tablespace files.

## Implications (Spec Updates Required)
- Tablespace design, DDL, and operational behavior must be aligned to the above decisions.
- Emulated parser specs must explicitly reject or emulate tablespace DDL without file creation.
- Catalog and on-disk structures must shift to root_gpid and header v2.

## Spec Update Targets
- `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md`
  - ID policy, header v2, multi-file model, ADD FILE/DATAFILE DDL, attach validation, recovery mode,
    migration wording, schema default usage.
- `ScratchBird/docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`
  - Replace root_page with root_gpid for tables/indexes.
- `ScratchBird/docs/specifications/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
  - CREATE/ALTER/DROP/ATTACH/DETACH TABLESPACE and ADD FILE/DATAFILE grammar for V2.
- `ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `ScratchBird/docs/specifications/parser/MYSQL_PARSER_SPECIFICATION.md`
  - Emulated parser behavior: no physical tablespace files; TABLESPACE DDL is rejected or
    mapped to schema-level emulation only.
- `ScratchBird/docs/specifications/parser/FIREBIRD_PARSER_SPECIFICATION.md`
  - Reject tablespace DDL for strict Firebird parity.
- `ScratchBird/docs/specifications/ddl/DDL_TABLES.md`
- `ScratchBird/docs/specifications/ddl/DDL_INDEXES.md`
  - Placement precedence: TABLESPACE clause > schema default > primary.
- `ScratchBird/docs/specifications/BACKUP_AND_RESTORE.md`
  - Enumerate all tablespace files in backup/restore.

## Implementation Checklist (by area)
### Parser + SBLR
- V2: implement CREATE/ALTER/DROP/ATTACH/DETACH TABLESPACE and ADD FILE/DATAFILE parsing.
- V2: propagate TABLESPACE for CREATE TABLE to bytecode (remove empty string stub).
- Emulated parsers: reject tablespace DDL and document parity.

### Catalog + On-disk Structures
- Convert root_page fields in table/index records to root_gpid.
- Update TablespaceHeader layout for v2 name length (new version marker).
- Implement multi-file tablespace catalog (pg_tablespace_files) and loader.
- Enforce tablespace ID allocation: skip 1, allow 2..65535 only.
- Maintain tablespace table_count / index_count on create/drop/migrate.

### Storage + Buffer + DML
- Add findFreePageInTablespace and allocateHeapPageInTablespace using GPID.
- Use GPID path for INSERT/SELECT/UPDATE/DELETE (not only migration).
- Ensure heap/index scans resolve page GPIDs for non-primary tablespaces.

### Startup / Recovery / Attach
- On startup, open all tablespace files from catalog.
- Add recovery mode config to allow missing tablespaces (strict by default).
- Implement ATTACH ... FORCE / ALLOW_MISMATCH behavior (UUID mismatch policy).

### Backup / Restore
- Backup/restore loops over all tablespace datafiles, including multi-file ranges.

## Notes
- Firebird parity requires rejecting TABLESPACE DDL rather than emulating physical files.
- V2 parser remains the only path that can create or manage tablespace files.
