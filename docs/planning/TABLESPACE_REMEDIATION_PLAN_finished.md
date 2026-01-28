# Tablespace Remediation Plan (Alpha)

## Purpose

Implement full tablespace functionality per Alpha decisions and close all gaps listed in:

- `ScratchBird/docs/findings/TABLESPACE_DECISIONS_AND_SCOPE.md`
- `ScratchBird/docs/findings/TABLESPACE_IMPLEMENTATION_AUDIT.md`
- `ScratchBird/docs/findings/INDEX_IMPLEMENTATION_GAPS-completed.md`

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

- Status: Complete
- Files: `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md`

**TS-P0-02** Update on-disk format spec for header v2 and root_gpid.

- Status: Complete
- Files: `ScratchBird/docs/specifications/storage/ON_DISK_FORMAT.md`

**TS-P0-03** Update catalog and DDL specs for root_gpid, default tablespace precedence, DDL syntax.

- Status: Complete
- Files: `ScratchBird/docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`,
  `ScratchBird/docs/specifications/ddl/DDL_TABLES.md`,
  `ScratchBird/docs/specifications/ddl/DDL_INDEXES.md`

**TS-P0-04** Update parser specs for V2 tablespace DDL; emulated parsers reject.

- Status: Complete
- Files: `ScratchBird/docs/specifications/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`,
  `ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`,
  `ScratchBird/docs/specifications/parser/MYSQL_PARSER_SPECIFICATION.md`,
  `ScratchBird/docs/specifications/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md`

**TS-P0-05** Update backup/restore spec for multi-file tablespaces.

- Status: Complete
- Files: `ScratchBird/docs/specifications/BACKUP_AND_RESTORE.md`

Acceptance: specs reflect decisions and no longer contradict implementation intent.

## Phase 1: On-Disk Structures + Catalog

**TS-P1-01** Bump TablespaceHeader layout (name length 63, new version marker).

- Files: `ScratchBird/include/scratchbird/core/tablespace.h`, `ScratchBird/src/core/page_manager.cpp`
  - Status: Complete

**TS-P1-02** Replace root_page with root_gpid in table/index records.

- Files: `ScratchBird/include/scratchbird/core/catalog_manager.h`,
  `ScratchBird/src/core/catalog_manager.cpp`,
  `ScratchBird/src/core/storage_engine.cpp`
  - Status: Complete

**TS-P1-03** Enforce tablespace ID allocation: skip 1, allow 2..65535.

- Files: `ScratchBird/src/core/catalog_manager.cpp`
  - Status: Complete (allocator starts at 2 in create/attach)

**TS-P1-04** Implement multi-file tablespace catalog (pg_tablespace_files) and loader.

- Files: `ScratchBird/include/scratchbird/core/tablespace.h`,
  `ScratchBird/src/core/catalog_manager.cpp`
  - Status: Complete

**TS-P1-05** Maintain table_count/index_count on create/drop/migrate/attach/detach.

- Files: `ScratchBird/src/core/catalog_manager.cpp`
  - Status: Complete (updateTablespaceCounts wiring)

Acceptance: catalog records and on-disk headers are consistent with new format; root_gpid is used.

## Phase 2: Parser + SBLR

**TS-P2-01** V2 parser: CREATE/ALTER/DROP/ATTACH/DETACH TABLESPACE; ADD FILE/DATAFILE.

- Status: Complete
- Files: `ScratchBird/src/parser/parser_v2.cpp`

**TS-P2-02** V2 bytecode: emit tablespace DDL opcodes and TABLESPACE clause for CREATE TABLE.

- Status: Complete
- Files: `ScratchBird/src/sblr/bytecode_generator_v2.cpp:1598`

**TS-P2-03** Emulated parsers accept only dialect-supported tablespace DDL and route to emulation
catalog (no on-disk file creation; ScratchBird parser is the only path that creates files).

- Status: Complete
- Files: `ScratchBird/src/parser/postgresql/pg_parser_ddl.cpp`,
  `ScratchBird/src/parser/mysql/mysql_parser.cpp`

Acceptance: V2 can execute tablespace DDL; emulated parsers reject per parity.

## Phase 3: Storage + DML GPID Wiring

**TS-P3-01** Add findFreePageInTablespace/allocateHeapPageInTablespace using GPID.

- Files: `ScratchBird/src/core/storage_engine.cpp`
  - Status: Complete

**TS-P3-02** Make INSERT/SELECT/UPDATE/DELETE tablespace-aware end-to-end (GPID).

- Files: `ScratchBird/src/core/storage_engine.cpp`, `ScratchBird/src/core/buffer_pool.cpp`
  - Status: Complete

**TS-P3-03** Update heap/index access to handle non-primary tablespace TIDs.

- Files: `ScratchBird/src/core/btree.cpp`, `ScratchBird/src/core/bitmap_index.cpp`,
  `ScratchBird/src/core/gin_index.cpp`
  - Status: Complete

**TS-P3-04** Implement index TID updates for all non-BTree/Hash types used in migrations.

- Scope: Vector/HNSW, Full-text, GIN, GiST, SP-GiST, BRIN, R-tree
- Files: `ScratchBird/src/core/catalog_manager.cpp`,
  index implementations under `ScratchBird/src/core/`
  - Status: Complete (updateIndexTIDs per index type)

**TS-P3-05** Fix GiST index cache cleanup (remove deliberate leak once types are fully integrated).

- Status: Complete
- Files: `ScratchBird/src/sblr/index_cache.cpp`

Acceptance: standard DML works on tables outside primary tablespace.

## Phase 4: Startup + Recovery + Attach

**TS-P4-01** Open all tablespaces at startup; strict by default.

- Status: Complete
- Files: `ScratchBird/src/core/database.cpp`, `ScratchBird/src/core/catalog_manager.cpp`

**TS-P4-02** Add recovery mode config (storage.tablespace_recovery_mode = strict|allow_missing).

- Status: Complete
- Files: config handling + `ScratchBird/docs/user-documentation/configuration/sb_server.conf.md`

**TS-P4-03** Implement ATTACH ... FORCE / ALLOW_MISMATCH semantics (UUID validation override).

- Status: Complete
- Files: `ScratchBird/src/core/catalog_manager.cpp`, `ScratchBird/src/core/page_manager.cpp`, `ScratchBird/src/sblr/executor.cpp`

Acceptance: missing tablespaces block startup unless recovery mode is enabled; attach override works.

## Phase 5: Backup/Restore

**TS-P5-01** Include all tablespace datafiles in backup/restore and verify maps.

- Status: Complete
- Files: `ScratchBird/src/core/backup_manager.cpp`, `ScratchBird/include/scratchbird/core/backup_manager.h`,
  `ScratchBird/docs/specifications/BACKUP_AND_RESTORE.md`

Acceptance: backups include all datafiles; restore refuses missing datafiles unless recovery mode.

## Phase 6: Tests + Validation

**TS-P6-01** Unit tests: header v2 read/write, ID allocation, multi-file allocation.

- Status: Complete
- Files: `ScratchBird/tests/unit/test_tablespace_autoextend.cpp`,
  `ScratchBird/tests/unit/test_backup_tablespace_manifest.cpp`,
  `ScratchBird/tests/unit/test_tablespace_header_and_files.cpp`

**TS-P6-02** Integration tests: create tablespace, add datafile, create table/index in tablespace,
             DML on tablespace object, backup/restore coverage.

- Status: Complete
- Files: `ScratchBird/tests/integration/test_tablespace_flow.cpp`

**TS-P6-03** Negative tests: missing tablespace strict mode, attach mismatch without FORCE.

- Status: Complete
- Files: `ScratchBird/tests/unit/test_tablespace_recovery.cpp`

**TS-P6-04** Parser tests: V2 DDL parse; emulated parser rejection.

- Status: Complete
- Files: `ScratchBird/tests/unit/test_parser_v2_ddl.cpp`,
  `ScratchBird/tests/unit/test_postgresql_parser.cpp`

Acceptance: test suite covers core tablespace flows and failure modes.

## Recent Progress

- 2026-01-21: Added sb_tablespace_files catalog page allocation/backfill plus load/persist helpers for tablespace file paths.
- 2026-01-21: Maintained tablespace table_count/index_count on table/index create/drop and table migration.
- 2026-01-21: Added tablespace-aware heap allocation hooks in StorageEngine; findFreePage still
  limited to primary tablespace (see F-TS-003).
- 2026-01-21: Heap scans now iterate GPID-based pages for custom tablespaces and emit correct TIDs.
- 2026-01-21: deleteTuple legacy path honors tablespace ID overrides from TIDs.
- 2026-01-22: Wired tablespace startup open/recovery mode, ATTACH FORCE/ALLOW_MISMATCH,
  CREATE TABLE tablespace bytecode, and added parser/negative/integration tablespace tests.
