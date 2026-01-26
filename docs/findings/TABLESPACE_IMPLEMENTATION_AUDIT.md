# Tablespace Spec vs Implementation Audit

## Scope
- Specs reviewed:
  - `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md`
  - `ScratchBird/docs/specifications/storage/STORAGE_ENGINE_MAIN.md`
  - `ScratchBird/docs/specifications/ddl/DDL_TABLES.md`
  - `ScratchBird/docs/specifications/ddl/DDL_INDEXES.md`
  - `ScratchBird/docs/specifications/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
  - `ScratchBird/docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`
  - `ScratchBird/docs/specifications/BACKUP_AND_RESTORE.md`
  - `ScratchBird/docs/specifications/TEMPORARY_TABLES_SPECIFICATION.md`
- Code reviewed:
  - GPID and tablespace structs: `ScratchBird/include/scratchbird/core/gpid.h`, `ScratchBird/include/scratchbird/core/tablespace.h`
  - Tablespace lifecycle + FSM: `ScratchBird/src/core/page_manager.cpp`
  - Tablespace catalog + migration: `ScratchBird/src/core/catalog_manager.cpp`
  - GPID I/O + FD registry: `ScratchBird/src/core/database.cpp`
  - Buffer pool GPID and flush: `ScratchBird/src/core/buffer_pool.cpp`
  - DDL executor: `ScratchBird/src/sblr/executor.cpp`
  - Parsers/semantic/bytecode: `ScratchBird/src/parser/parser_v2.cpp`, `ScratchBird/src/parser/postgresql/pg_parser_ddl.cpp`,
    `ScratchBird/src/sblr/semantic_analyzer_v2.cpp`, `ScratchBird/src/sblr/bytecode_generator_v2.cpp`
  - Storage engine and index GC: `ScratchBird/src/core/storage_engine.cpp`, `ScratchBird/src/core/btree.cpp`,
    `ScratchBird/src/core/bitmap_index.cpp`, `ScratchBird/src/core/gin_index.cpp`

## Implemented (code-truth)
- GPID encoding and primary tablespace constants.
  - Code: `ScratchBird/include/scratchbird/core/gpid.h:10-116`
- Tablespace on-disk header, catalog record, and in-memory info structs.
  - Code: `ScratchBird/include/scratchbird/core/tablespace.h:39-209`
- Tablespace file lifecycle, FSM, allocation/freeing, extension, and metrics.
  - Code: `ScratchBird/src/core/page_manager.cpp:596-1750`, `ScratchBird/include/scratchbird/core/page_manager.h:50-285`
- GPID read/write and tablespace FD registry in Database.
  - Code: `ScratchBird/src/core/database.cpp:1452-1852`
- BufferPool GPID read/write path and per-tablespace flush.
  - Code: `ScratchBird/src/core/buffer_pool.cpp:785-898`, `ScratchBird/src/core/buffer_pool.cpp:437-479`
- Catalog tablespace operations: create/alter/rename/drop, attach/detach, stats update.
  - Code: `ScratchBird/src/core/catalog_manager.cpp:9434-10586`
- DDL executor supports tablespace opcodes (create/alter/drop/attach/detach, alter table set tablespace).
  - Code: `ScratchBird/src/sblr/executor.cpp:1676-1754`, `ScratchBird/src/sblr/executor.cpp:7076-11361`
- Table/index root pages allocated in target tablespace; tablespace counts updated on create/drop/migration.
  - Code: `ScratchBird/src/core/catalog_manager.cpp:6565-6760`, `ScratchBird/src/core/catalog_manager.cpp:7183-7360`,
    `ScratchBird/src/core/catalog_manager.cpp:10398-10436`
- Offline table migration plumbing (page copy + TID remap), with TID resolver hooks.
  - Code: `ScratchBird/src/core/catalog_manager.cpp:11246-11802`, `ScratchBird/src/core/tid_resolver.cpp:145-271`
- TOAST creation/migration uses parent tablespace id.
  - Code: `ScratchBird/src/core/toast.cpp:327-352`

## Missing or Partial vs Spec

### F-TS-001 Tablespace DDL is not parsed for native/PG/MySQL
- Spec: `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:521-568`
- Spec: `ScratchBird/docs/specifications/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md:75-92`
- Code: CREATE dispatch does not include TABLESPACE; ALTER only supports rename/move.
  `ScratchBird/src/parser/parser_v2.cpp:216-321`, `ScratchBird/src/parser/parser_v2.cpp:2290-2345`
- Code: PG parser only captures TABLESPACE clause on CREATE TABLE/INDEX, not CREATE/ALTER/DROP TABLESPACE.
  `ScratchBird/src/parser/postgresql/pg_parser_ddl.cpp:493-540`, `ScratchBird/src/parser/postgresql/pg_parser_ddl.cpp:1039-1059`
- Status: Missing (DDL surface exists in executor/catalog but is unreachable from SQL parsers)

### F-TS-002 Native CREATE TABLE ignores TABLESPACE clause
- Spec: `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:570-586`
- Code: parser records TABLESPACE, but semantic/bytecode drop it (empty string emitted).
  `ScratchBird/src/parser/parser_v2.cpp:459-462`,
  `ScratchBird/src/sblr/bytecode_generator_v2.cpp:1357-1358`
- Status: Missing

### F-TS-003 Core DML path still operates on tablespace 0 only
- Spec: Tablespace placement should route heap/page access by tablespace.
  `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:231-236`
- Code: INSERT/SELECT paths use legacy findFreePage/pinPage (tablespace 0 only).
  `ScratchBird/src/core/storage_engine.cpp:465-488`,
  `ScratchBird/src/core/storage_engine.cpp:859-863`
- Status: Partial (GPID path exists but not used for normal DML)

### F-TS-004 Table/index root pages always allocated in primary file
- Spec: objects placed in their specified tablespace.
  `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:172-176`
- Code: createTable/createIndex allocate via allocatePageInTablespace.
  `ScratchBird/src/core/catalog_manager.cpp:6565-6715`,
  `ScratchBird/src/core/catalog_manager.cpp:7265-7285`
- Status: Resolved

### F-TS-005 Online migration rejected
- Spec: `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:705-710`
- Code: ONLINE flag returns NOT_SUPPORTED.
  `ScratchBird/src/core/catalog_manager.cpp:11260-11267`
- Status: Missing

### F-TS-006 Index TID updates are incomplete across index types
- Spec: `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:1122-1135`
- Code: TID updates implemented for B-Tree, Hash, HNSW, FULLTEXT/GIN, GiST, BRIN, R-tree, SP-GiST,
  Bitmap, Columnstore, LSM (per index type case handling).
  `ScratchBird/src/core/catalog_manager.cpp:11895-12443`
- Status: Resolved

### F-TS-007 Tablespace header transaction fields are never synchronized
- Spec: per-tablespace header tracks OIT/latest XID.
  `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:379-399`
- Code: header fields set to 0 on create, no update path.
  `ScratchBird/src/core/page_manager.cpp:1035-1038`
- Status: Missing

### F-TS-008 Tablespace files are not reopened on database startup
- Spec: database init must read tablespace catalog and open all files.
  `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:1009-1012`
- Code: openTablespace only invoked on attach path.
  `ScratchBird/src/core/catalog_manager.cpp:10317-10321`
- Status: Missing

### F-TS-009 tablespace catalog counts are not maintained
- Spec: per-tablespace `table_count` / `index_count` tracked.
  `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:439-440`
- Code: updateTablespaceCounts invoked on create/drop/migrate; counts persisted in sb_tablespace.
  `ScratchBird/src/core/catalog_manager.cpp:6565-6760`,
  `ScratchBird/src/core/catalog_manager.cpp:7183-7360`,
  `ScratchBird/src/core/catalog_manager.cpp:10398-10436`
- Status: Resolved

### F-TS-010 Multi-file tablespace catalog is unused
- Spec: pg_tablespace_files table and multi-file support.
  `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:452-468`
- Code: struct exists but no use site.
  `ScratchBird/include/scratchbird/core/tablespace.h:135-173`
- Status: Missing

### F-TS-011 moveIndexToTablespace API is not implemented
- Spec: `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:711-714`
- Code: no CatalogManager::moveIndexToTablespace in codebase.
- Status: Missing

### F-TS-012 PageManager API mismatches spec
- Spec: `extendTablespace` takes explicit page count; `getTablespaceStats` and `flushTablespace` exist.
  `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:645-657`
- Code: extendTablespace uses autoextend config only, no getTablespaceStats/flushTablespace in PageManager.
  `ScratchBird/include/scratchbird/core/page_manager.h:50-210`
- Status: Partial

### F-TS-013 Tablespace ID allocation is inconsistent with spec
- Spec: 0 = primary, 1-65535 = custom tablespaces.
  `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:218-219`
- Code: GPID defines 0=primary, 1=reserved; create/attach allocate from 2.
  `ScratchBird/include/scratchbird/core/gpid.h:19-21`,
  `ScratchBird/src/core/catalog_manager.cpp:9924-9942`,
  `ScratchBird/src/core/catalog_manager.cpp:11120-11147`
- Status: Resolved (allocator aligns to reserved ID=1 policy)

### F-TS-014 Tablespace name length mismatch (catalog vs header)
- Spec: header uses 32-byte name, catalog uses 64-byte name.
  `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:379-421`
- Code: CatalogManager allows 1-63 chars; PageManager rejects >31 chars.
  `ScratchBird/src/core/catalog_manager.cpp:9581-9586`,
  `ScratchBird/src/core/page_manager.cpp:952-957`
- Status: Mismatch

### F-TS-015 Schema default tablespace not applied
- Spec: schema has `default_tablespace_id`.
  `ScratchBird/docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md:123-146`
- Code: executor defaults to tablespace_id=0 unless explicit TABLESPACE clause is provided.
  `ScratchBird/src/sblr/executor.cpp:5492-5496`
- Status: Missing

### F-TS-016 Backup/restore does not enumerate tablespace files
- Spec: backup loops over all tablespaces and their pages.
  `ScratchBird/docs/specifications/BACKUP_AND_RESTORE.md:599-623`
- Code: backup manager contains no tablespace handling.
  `ScratchBird/src/core/backup_manager.cpp:1-200`
- Status: Missing

### F-TS-017 Attach does not validate database_uuid
- Spec: ATTACH should warn/validate database UUID.
  `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:902-905`
- Code: attach validates magic + page_size only.
  `ScratchBird/src/core/catalog_manager.cpp:10235-10253`
- Status: Partial

### F-TS-018 Spec says heap tuples never move between tablespaces
- Spec: "Heap tuples never move between tablespaces."
  `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:221-224`
- Code: moveTableToTablespace copies pages and remaps TIDs.
  `ScratchBird/src/core/catalog_manager.cpp:11497-11579`,
  `ScratchBird/src/core/catalog_manager.cpp:11737-11756`
- Status: Spec mismatch (behavior needs reconciliation)

## Spec Gaps / Updates Recommended
- Update spec to reflect reserved ID=1 policy (code now allocates from 2..65535).
  `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:218-219`
- Clarify name length limits (header vs catalog) and expected truncation or validation rules.
  `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:379-421`
- Define how schema `default_tablespace_id` is applied during CREATE TABLE/INDEX and how it
  interacts with explicit TABLESPACE clauses.
  `ScratchBird/docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md:123-146`
- Specify root page allocation policy (primary vs target tablespace) and whether system pages
  are allowed to reside outside the primary file.
- Add a startup step for reopening all tablespaces from catalog and the expected error path
  when files are missing.
  `ScratchBird/docs/specifications/storage/TABLESPACE_SPECIFICATION.md:1009-1012`
- Capture extension metrics (`TablespaceMetrics`) and monitoring exposure in the spec.
  `ScratchBird/include/scratchbird/core/page_manager.h:238-262`
