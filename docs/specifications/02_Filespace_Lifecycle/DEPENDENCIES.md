# 02 Filespace Lifecycle Dependencies

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
Section `02` depends on stable identity, on-disk page contracts, allocator or free-space behavior, and migration-aware execution paths. The reviewed implementation is more coupled than the older short dependency note suggested.

## Current dependency map
### Upstream authorities
- `00_Governance_and_Invarients`
  - Stable identity and MGA-era correctness assumptions constrain placement and relocation behavior.
- `01_Configuration_Subsystem`
  - Runtime path and service configuration influence where the primary database file is created or opened, even though the current configuration system is still file-backed rather than catalog-backed.
- `05_Page_Taxonomy_and_Binary_Layouts`
  - `DatabaseHeader`, shared `PageHeader`, checksum, and GPID rules are prerequisites for section `02` file layout.

### Primary implementation dependencies
- `include/scratchbird/core/tablespace.h`
  - durable tablespace header and catalog record authority
- `include/scratchbird/core/page_manager.h`
  - declared internal tablespace lifecycle surface: create, open, close, extend, preallocate, and header update
- `src/core/database.cpp`
  - primary database bootstrap-page creation and validation
- `src/core/page_manager.cpp`
  - tablespace allocation, FSM integration, and autoextend
- `include/scratchbird/core/catalog_manager.h`
  - schema, table, index, and filespace placement metadata authority
- `src/core/catalog_manager.cpp`
  - tablespace UUID or ID binding, filespace statistics lookup, migration metadata updates, and create-tablespace orchestration
- `src/core/tid_resolver.cpp`
  - source or target tablespace resolution during migration
- `include/scratchbird/core/storage_engine.h`
  - tuple access paths that consume migration-aware placement resolution
- `src/core/transaction_manager.cpp`
  - conservative all-filespace publication and fencing behavior during movement-sensitive operations
- `src/parser/postgresql/pg_parser_ddl.cpp`
  - parser front-door for `CREATE TABLESPACE`, `ALTER TABLESPACE`, and `DROP TABLESPACE`
- `src/sblr/executor.cpp`
  - SBLR runtime dispatch for tablespace opcodes

### Downstream consumers
- `03_Disk_Allocator_and_Free_Space`
  - per-filespace free-space and allocation policy
- `10_GC_and_Sweep`
  - reclaim and relocation safety depend on placement truth
- `18_Index_Framework`
  - relocation cleanup and index rewrite closure depend on filespace movement legality
- `24_Catalog_Model_and_Virtual_Overlays`
  - migration metadata and publication surfaces depend on placement ownership being explicit

## Non-blocking expansion candidates
- A machine-readable dependency matrix for section `02`
- A single documented owner for durable placement metadata versus runtime migration metadata
- A clean dependency contract that separates parser or opcode surfaces from actual engine-side lifecycle guarantees for attach, detach, shrink, and split

## Suggestions
- Use this file as the single dependency authority for placement and relocation.
- When later sections depend on filespace behavior, reference this section instead of restating partial local assumptions.
- Add a machine-readable dependency ledger row for section `02` once exact line closure is finished.
