# Tablespace Remediation Plan (Alpha)

## Purpose
Implement full tablespace functionality per Alpha decisions and close all gaps listed in:
- `ScratchBird/docs/findings/TABLESPACE_DECISIONS_AND_SCOPE.md`
- `ScratchBird/docs/findings/TABLESPACE_IMPLEMENTATION_AUDIT.md`

## Guiding Decisions (Locked)
- IDs: 0 = primary, 1 = reserved (future), 2..65535 = custom.
- Header: expand tablespace name to 63 chars and bump header format.
- DDL: V2 parser only; emulated parsers reject tablespace DDL for parity.
- Placement: TABLESPACE clause honored; default tablespace from schema if omitted.
- DML: GPID-aware for INSERT/SELECT/UPDATE/DELETE.
- Root pages: replace root_page with root_gpid.
- Startup: strict by default; recovery mode can allow missing tablespaces.
- Attach: strict UUID by default; FORCE / ALLOW_MISMATCH override.
- Multi-file tablespaces: implement now with ADD FILE / ADD DATAFILE.
- Counters: maintain table_count/index_count.
- Backup/restore: include all tablespace files.

## Phase 0: Specification Alignment
**TS-P0-01** Update tablespace spec to match decisions (ID policy, header v2, multi-file model, DDL).
- Files: `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md`

**TS-P0-02** Update on-disk format spec for header v2 and root_gpid.
- Files: `ScratchBird/docs/specifications/ON_DISK_FORMAT.md`

**TS-P0-03** Update catalog and DDL specs for root_gpid, default tablespace precedence, DDL syntax.
- Files: `ScratchBird/docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`,
  `ScratchBird/docs/specifications/ddl/DDL_TABLES.md`,
  `ScratchBird/docs/specifications/ddl/DDL_INDEXES.md`

**TS-P0-04** Update parser specs for V2 tablespace DDL; emulated parsers reject.
- Files: `ScratchBird/docs/specifications/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`,
  `ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`,
  `ScratchBird/docs/specifications/parser/MYSQL_PARSER_SPECIFICATION.md`,
  `ScratchBird/docs/specifications/parser/FIREBIRD_PARSER_SPECIFICATION.md`

**TS-P0-05** Update backup/restore spec for multi-file tablespaces.
- Files: `ScratchBird/docs/specifications/BACKUP_AND_RESTORE.md`

Acceptance: specs reflect decisions and no longer contradict implementation intent.

## Phase 1: On-Disk Structures + Catalog
**TS-P1-01** Bump TablespaceHeader layout (name length 63, new version marker).
- Files: `ScratchBird/include/scratchbird/core/tablespace.h`, `ScratchBird/src/core/page_manager.cpp`

**TS-P1-02** Replace root_page with root_gpid in table/index records.
- Files: `ScratchBird/include/scratchbird/core/catalog_manager.h`,
  `ScratchBird/src/core/catalog_manager.cpp`,
  `ScratchBird/src/core/storage_engine.cpp`

**TS-P1-03** Enforce tablespace ID allocation: skip 1, allow 2..65535.
- Files: `ScratchBird/src/core/catalog_manager.cpp`

**TS-P1-04** Implement multi-file tablespace catalog (pg_tablespace_files) and loader.
- Files: `ScratchBird/include/scratchbird/core/tablespace.h`,
  `ScratchBird/src/core/catalog_manager.cpp`

**TS-P1-05** Maintain table_count/index_count on create/drop/migrate/attach/detach.
- Files: `ScratchBird/src/core/catalog_manager.cpp`

Acceptance: catalog records and on-disk headers are consistent with new format; root_gpid is used.

## Phase 2: Parser + SBLR
**TS-P2-01** V2 parser: CREATE/ALTER/DROP/ATTACH/DETACH TABLESPACE; ADD FILE/DATAFILE.
- Files: `ScratchBird/src/parser/parser_v2.cpp`

**TS-P2-02** V2 bytecode: emit tablespace DDL opcodes and TABLESPACE clause for CREATE TABLE.
- Files: `ScratchBird/src/sblr/bytecode_generator_v2.cpp`

**TS-P2-03** Emulated parsers reject tablespace DDL with parity errors.
- Files: `ScratchBird/src/parser/postgresql/pg_parser_ddl.cpp`,
  `ScratchBird/src/parser/mysql/mysql_parser.cpp`,
  Firebird parser implementation file

Acceptance: V2 can execute tablespace DDL; emulated parsers reject per parity.

## Phase 3: Storage + DML GPID Wiring
**TS-P3-01** Add findFreePageInTablespace/allocateHeapPageInTablespace using GPID.
- Files: `ScratchBird/src/core/storage_engine.cpp`

**TS-P3-02** Make INSERT/SELECT/UPDATE/DELETE tablespace-aware end-to-end (GPID).
- Files: `ScratchBird/src/core/storage_engine.cpp`, `ScratchBird/src/core/buffer_pool.cpp`

**TS-P3-03** Update heap/index access to handle non-primary tablespace TIDs.
- Files: `ScratchBird/src/core/btree.cpp`, `ScratchBird/src/core/bitmap_index.cpp`,
  `ScratchBird/src/core/gin_index.cpp`

Acceptance: standard DML works on tables outside primary tablespace.

## Phase 4: Startup + Recovery + Attach
**TS-P4-01** Open all tablespaces at startup; strict by default.
- Files: `ScratchBird/src/core/database.cpp`, `ScratchBird/src/core/catalog_manager.cpp`

**TS-P4-02** Add recovery mode config (storage.tablespace_recovery_mode = strict|allow_missing).
- Files: config handling + `ScratchBird/docs/specifications/configuration/sb_server.conf.md`

**TS-P4-03** Implement ATTACH ... FORCE / ALLOW_MISMATCH semantics (UUID validation override).
- Files: `ScratchBird/src/core/catalog_manager.cpp`, `ScratchBird/src/sblr/executor.cpp`

Acceptance: missing tablespaces block startup unless recovery mode is enabled; attach override works.

## Phase 5: Backup/Restore
**TS-P5-01** Include all tablespace datafiles in backup/restore and verify maps.
- Files: `ScratchBird/src/core/backup_manager.cpp`, `ScratchBird/docs/specifications/BACKUP_AND_RESTORE.md`

Acceptance: backups include all datafiles; restore refuses missing datafiles unless recovery mode.

## Phase 6: Tests + Validation
**TS-P6-01** Unit tests: header v2 read/write, ID allocation, multi-file allocation.
**TS-P6-02** Integration tests: create tablespace, add datafile, create table/index in tablespace,
             DML on tablespace object, backup/restore coverage.
**TS-P6-03** Negative tests: missing tablespace strict mode, attach mismatch without FORCE.
**TS-P6-04** Parser tests: V2 DDL parse; emulated parser rejection.

Acceptance: test suite covers core tablespace flows and failure modes.

## Dependencies / Notes
- ScratchBird is currently treated as read-only in this workspace; code work should start
  after explicit approval to modify the engine.
- Root_gpid change affects on-disk format; bump version and ensure no silent mixing.
