# Filespace Operations

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The reviewed code proves meaningful tablespace lifecycle substrate, but Beta 1
requires a broader operator lifecycle than the currently closed implementation
surface. The canonical operator contract is defined by
`TABLESPACE_DDL_AND_OPERATOR_LIFECYCLE_MODEL.md`; the current code is treated as
partial substrate plus implementation drift, not as a limit on required Beta 1
behavior.

## Current implementation substrate
- Database creation builds and validates a fixed bootstrap-page map for the primary `.sbdb` file.
- The page manager can allocate pages in a specific tablespace and will use tablespace-local FSM state.
- Tablespace autoextend behavior is real inside the allocation path.
- Header decoding supports both legacy and current tablespace header versions.
- Internal tablespace lifecycle methods exist for create, open, close, extend, preallocate, and header update.
- Database-level file-descriptor registration and unregister flows exist for non-primary tablespaces.
- Catalog-manager orchestration exists for `createTablespace`, `updateTablespace`, `renameTablespace`, `dropTablespace`, and for binding or rebinding placement metadata through tablespace UUIDs.
- Catalog-manager attach and detach internals are real:
  - `attachTablespace` validates the file path and header, checks page size and optional UUID compatibility, resolves name conflicts, allocates a new tablespace ID, registers the file, opens the tablespace, writes catalog rows, and caches the result
  - `detachTablespace` enforces non-primary rules, counts resident tables and indexes, optionally migrates tables back to primary on `FORCE`, flushes dirty pages, closes the tablespace, deletes catalog rows, and removes cache state
- Live migration resolution exists through `TIDResolver`, which redirects tuple lookup to source or target tablespace depending on migration evidence.
- Movement-sensitive publication behavior is fenced conservatively across filespaces in the transaction manager.
- Executor-backed alter and drop runtime paths are real:
  - `ALTER TABLESPACE` currently applies rename, autoextend toggle, autoextend size, and maxsize through catalog-manager methods
  - `DROP TABLESPACE` currently enforces non-primary restrictions, emptiness or `FORCE` handling, closes physical filespaces, deletes file paths, and removes catalog rows
- Parser and SBLR front doors exist for `CREATE TABLESPACE`, `ALTER TABLESPACE`,
  `DROP TABLESPACE`, `ATTACH TABLESPACE`, and `DETACH TABLESPACE`.
- Attach/detach front doors are Beta 1 required operator behavior.
  Parser/AST/emitter/lowerer/opcode presence is not sufficient unless the live
  executor dispatch routes those operations fail-closed into the canonical
  handlers.
- Shrink/compaction, split/cutover, and durable lifecycle-history publication
  are Beta 1 required behavior owned by this section even where current closure
  is incomplete.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `src/core/page_manager.cpp` | `decodeTablespaceHeader` | `105` | Secondary tablespace header validation and version decode |
| ScratchBird | `src/core/page_manager.cpp` | `allocatePageInTablespace` | `957` | Current allocation, FSM, and autoextend authority |
| ScratchBird | `include/scratchbird/core/page_manager.h` | `createTablespace`, `openTablespace`, `closeTablespace` | `146`, `168`, `188` | Declared internal lifecycle surface for tablespace files |
| ScratchBird | `include/scratchbird/core/page_manager.h` | `extendTablespace`, `preallocatePages`, `updateTablespaceHeader` | `208`, `233`, `249` | Declared resize and header-update surface |
| ScratchBird | `src/core/page_manager.cpp` | `PageManager::createTablespace` | `1321` | Creates and initializes a tablespace file |
| ScratchBird | `src/core/page_manager.cpp` | `PageManager::openTablespace` | `1598` | Opens and validates an existing tablespace file |
| ScratchBird | `src/core/page_manager.cpp` | `PageManager::closeTablespace` | `1788` | Closes a non-primary tablespace file |
| ScratchBird | `src/core/page_manager.cpp` | `PageManager::extendTablespace` | `1916` | Explicit tablespace extension path |
| ScratchBird | `src/core/page_manager.cpp` | `PageManager::updateTablespaceHeader` | `2188` | Persists tablespace header changes |
| ScratchBird | `src/core/page_manager.cpp` | `PageManager::preallocatePages` | `2308` | Preallocates pages for a new tablespace |
| ScratchBird | `include/scratchbird/core/database.h` | `registerTablespaceFile`, `unregisterTablespaceFile` | `904`, `917` | Database-owned file-descriptor registration surface |
| ScratchBird | `src/core/database.cpp` | `Database::registerTablespaceFile` | `6894` | Registers non-primary tablespace files |
| ScratchBird | `src/core/database.cpp` | `Database::unregisterTablespaceFile` | `6928` | Unregisters and closes non-primary tablespace files |
| ScratchBird | `include/scratchbird/core/catalog_manager.h` | `CatalogManager::createTablespace` | `11607` | Catalog-layer create surface |
| ScratchBird | `include/scratchbird/core/catalog_manager.h` | `CatalogManager::dropTablespace` | `11612` | Catalog-layer drop surface |
| ScratchBird | `include/scratchbird/core/catalog_manager.h` | `CatalogManager::updateTablespace`, `CatalogManager::renameTablespace` | `11624`, `11628` | Catalog-layer alter surface |
| ScratchBird | `include/scratchbird/core/catalog_manager.h` | `CatalogManager::attachTablespace`, `CatalogManager::detachTablespace` | `11679`, `11716` | Catalog-layer attach/detach surface |
| ScratchBird | `src/core/catalog_manager.cpp` | `CatalogManager::createTablespace` | `26380` | Catalog-layer tablespace creation orchestration |
| ScratchBird | `src/core/catalog_manager.cpp` | `pm->createTablespace(...)` call | `26467` | Catalog-layer handoff into page-manager creation |
| ScratchBird | `src/core/catalog_manager.cpp` | `CatalogManager::dropTablespace` | `26530` | Drop runtime with non-primary check, `FORCE` cleanup, file close, file delete, and catalog delete |
| ScratchBird | `src/core/catalog_manager.cpp` | `CatalogManager::updateTablespace` | `26781` | Alters autoextend and maxsize settings and updates durable header state |
| ScratchBird | `src/core/catalog_manager.cpp` | `CatalogManager::renameTablespace` | `26894` | Alters tablespace name through catalog-manager runtime |
| ScratchBird | `src/core/catalog_manager.cpp` | `CatalogManager::attachTablespace` | `27054` | Attach runtime with header validation, optional UUID mismatch override, ID allocation, file registration, open, and catalog insertion |
| ScratchBird | `src/core/catalog_manager.cpp` | `CatalogManager::detachTablespace` | `27303` | Detach runtime with non-primary check, optional offline migration, flush, close, and catalog deletion |
| ScratchBird | `src/core/tid_resolver.cpp` | `TIDResolver::recordMigration` | `179` | Per-table migration evidence publication |
| ScratchBird | `src/core/tid_resolver.cpp` | `TIDResolver::resolveTablespace` | `216` | Source or target tablespace resolution during live migration |
| ScratchBird | `include/scratchbird/core/storage_engine.h` | `StorageEngine::getTuple(uint32_t, uint16_t, ...)` | `176` | Direct tuple lookup surface that feeds movement-aware access |
| ScratchBird | `include/scratchbird/core/storage_engine.h` | `StorageEngine::getTuple(const ID&, const TID&, ...)` | `181` | Movement-aware tuple lookup entry point |
| ScratchBird | `src/core/transaction_manager.cpp` | `TransactionManager::flushTransactionState` | `3750` | Terminal all-filespace durability fence |
| ScratchBird | `src/core/transaction_manager.cpp` | `TransactionManager::flushTransactionPublicationState` | `3800` | Publication ordering and all-filespace forced-write fence |
| ScratchBird | `src/core/database.cpp` | `Database::create_reserved_bootstrap_page` | `4234` | Primary-file bootstrap page creation |
| ScratchBird | `src/core/database.cpp` | `Database::init_header_page` | `4281` | Primary header initialization |
| ScratchBird | `src/core/database.cpp` | `Database::create` | `4392` | Primary-file lifecycle bootstrap |
| ScratchBird | `src/parser/postgresql/pg_parser_ddl.cpp` | `Parser::parseCreateTablespace` | `4090` | PostgreSQL parser front-door for create tablespace |
| ScratchBird | `src/parser/postgresql/pg_parser_ddl.cpp` | `Parser::parseAlterTablespace` | `6703` | PostgreSQL parser front-door for alter tablespace |
| ScratchBird | `src/parser/postgresql/pg_parser_ddl.cpp` | `Parser::parseDropTablespace` | `6803` | PostgreSQL parser front-door for drop tablespace |
| ScratchBird | `src/parser/parser_v3.cpp` | `Parser::parseAttachTablespace` | `5082` | Current parser front-door for attach tablespace |
| ScratchBird | `src/parser/parser_v3.cpp` | `Parser::parseDetachTablespace` | `5117` | Current parser front-door for detach tablespace |
| ScratchBird | `include/scratchbird/parser/ast_v3.h` | `AttachTablespaceStmt`, `DetachTablespaceStmt` | `1619`, `1633` | Current AST surfaces for attach/detach |
| ScratchBird | `src/parser/v3_emitter.cpp` | `SBLR3_ATTACH_TABLESPACE`, `SBLR3_DETACH_TABLESPACE` emission | `2898`, `2911` | V3 emitter surfaces for attach/detach opcodes |
| ScratchBird | `src/sblr/ast_sblr_lowerer.cpp` | `SBLR3_ATTACH_TABLESPACE`, `SBLR3_DETACH_TABLESPACE` lowering | `2830`, `2843` | AST-to-SBLR lowering surfaces for attach/detach opcodes |
| ScratchBird | `include/scratchbird/sblr/v3_opcodes.generated.h` | `SBLR3_ATTACH_TABLESPACE`, `SBLR3_DETACH_TABLESPACE` | `11`, `22` | Opcode definitions exist for attach/detach |
| ScratchBird | `include/scratchbird/sblr/executor.h` | `executeAttachTablespace`, `executeDetachTablespace` | `741`, `742` | Executor declares dedicated attach/detach handlers |
| ScratchBird | `src/sblr/executor.cpp` | `Executor::executeAttachTablespace` | `18286` | Executor handler invoking catalog-manager attach |
| ScratchBird | `src/sblr/executor.cpp` | `Executor::executeDetachTablespace` | `18344` | Executor handler invoking catalog-manager detach |
| ScratchBird | `src/sblr/executor.cpp` | V3 drop tablespace dispatch | `91629` | V3 drop surface normalizes flags and hands off to the legacy executor |
| ScratchBird | `src/sblr/executor.cpp` | V3 alter tablespace dispatch | `91982` | V3 alter surface builds the legacy alteration stream |
| ScratchBird | `src/sblr/executor.cpp` | `Executor::executeAlterTablespace` | `12105` | Runtime alter path for rename and sizing or autoextend changes |
| ScratchBird | `src/sblr/executor.cpp` | `Executor::executeDropTablespace` | `18171` | Runtime drop path invoking catalog-manager drop |
| ScratchBird | `src/sblr/executor.cpp` | `SBLR3_CREATE_TABLESPACE` dispatch | `90911`, `12064` | SBLR runtime dispatch reaches catalog create surface |
| ScratchBird | `src/sblr/executor.cpp` | `SBLR3_DROP_TABLESPACE` dispatch | `91562` | Drop tablespace opcode has executor dispatch surface |
| ScratchBird | `src/sblr/executor.cpp` | `SBLR3_ALTER_TABLESPACE` dispatch | `91632` | Alter tablespace opcode has executor dispatch surface |

## Drift and contradictions
- Older prose described explicit create, attach, detach, online migration, shrink, compaction, and shadow-file procedures as if each were already canonical runtime operations.
- The reviewed implementation proves more lifecycle surface than the older summary gave credit for: create, open, close, extend, preallocate, header update, parser opcodes, and executor dispatch are all present.
- Even with those surfaces, the implementation is still split across parser, SBLR executor, catalog manager, page manager, and database file-registration code instead of one unified lifecycle authority.
- `ALTER TABLESPACE` is implemented, but the V3 compatibility lane can treat some unsupported option keys or empty reset lists as no-op rather than fail closed.
- Attach/detach dispatch reachability, shrink/compaction, split/cutover, and
  durable lifecycle-history publication remain implementation drift against the
  Beta 1 contract defined by this section.

## Implementation closure required by Beta 1
- A canonical single-owner operator API/state machine for tablespace lifecycle
  operations
- A durable migration-history and lifecycle-history register with
  operator-visible progress/refusal state
- Verified live opcode/dispatch reachability for attach/detach across the
  executor switch
- Verified shrink/compaction behavior and its correctness gates
- Verified split/cutover ordering, refusal, and rollback behavior
- Exact fail-closed closure for compatibility-only alter semantics and
  attach/detach dispatcher wiring

## Competitive parity closure requirements

For the competitive-performance parity package, filespace lifecycle must expose
growth and preallocation as a hot-path performance service, not only as
administrative substrate.

Before parity closure, the filespace layer shall:

1. allow admitted batch write paths to request ahead-of-demand growth
   reservation before the first row of the batch is written
2. perform file growth and preallocation in coarse windows sized to the
   admitted batch or locality run rather than one page at a time
3. persist the resulting header and sizing state without forcing the row loop
   to re-enter lifecycle code for every newly needed page
4. publish an explicit refusal or disk-pressure incident if coarse reservation
   cannot be honored legally

The parity package may not close while benchmark-governed load or set-sourced
insert paths still bounce through repeated `extendTablespace` or
`preallocatePages` calls inside the hot row path when the upcoming demand is
already known.

## Suggestions
- Drive missing behavior through one tablespace lifecycle state machine instead
  of scattered helpers.
- Explicitly classify each lifecycle method as parser-only, SBLR-only,
  catalog-orchestration, page-manager internal, or externally guaranteed
  operator behavior.
- Treat missing code closure as Beta 1 implementation drift, not as permission
  to weaken the operator contract.
- Fail closed at runtime: if a lifecycle operation cannot complete its durable
  state transition, publish refusal/history state instead of silently narrowing
  the contract.
