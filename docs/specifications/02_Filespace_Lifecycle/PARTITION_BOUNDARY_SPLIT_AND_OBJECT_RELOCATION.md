# Partition Boundary Split and Object Relocation

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status

The reviewed code proves relocation substrate through migration-aware source or
target tablespace resolution and movement-sensitive publication ordering. Beta 1
requires explicit partition-boundary split and cutover orchestration on top of
that substrate.

## Current implementation substrate

- `TIDResolver::recordMigration` records per-table migration evidence.
- `TIDResolver::resolveTablespace` uses bloom-backed and exact migration
  evidence to decide whether a tuple remains on the source tablespace or has
  moved to the target tablespace.
- `CatalogManager::TableInfo` carries `tablespace_id`, `tablespace_uuid`, and
  `migration_target_ts` metadata for relocation state.
- Catalog-manager migration logic updates placement metadata and clears
  migration flags on completion or abort.
- Storage-engine tuple lookup APIs consume that migration-aware placement truth.
- Movement-sensitive publication and fencing behavior exists in the transaction
  manager across all affected filespaces.
- Stable row identity remains anchored in TID, CTID, back-version pointers, and
  row UUID semantics.

## Required Beta 1 relocation and split algorithm

1. Validate source scope, target scope, source/target tablespace state, and
   page-size compatibility.
2. Allocate durable operation/history id and publish `MIGRATING` or
   `SPLIT_PENDING` state for the affected scope.
3. Publish migration-target metadata used by resolver/runtime lookup.
4. Copy/rewrite heap rows plus dependent index and oversized-value state while
   preserving stable row identity.
5. Validate copied scope, cleanup backlog, and derivative/evidence
   prerequisites.
6. Publish `CUTOVER_PENDING` only after validation succeeds.
7. Execute the final cross-filespace publication fence.
8. Switch placement metadata and resolver truth atomically.
9. Clear migration/split flags and publish completion or fail-closed refusal.

## Failure rules

1. Split/cutover may not infer completion from partial copy state.
2. Rollback/refusal must leave durable history and explicit pending/aborted
   state.
3. Source/target ambiguity after restart must fail closed rather than guessing
   which side is authoritative.
4. Resolver routing, catalog metadata, and publication fencing must agree before
   the operation is reported complete.

## Implementation code map

| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `src/core/tid_resolver.cpp` | `TIDResolver::recordMigration` | `179` | Durable or cached migration evidence for a table |
| ScratchBird | `src/core/tid_resolver.cpp` | `TIDResolver::resolveTablespace` | `216` | Source or target tablespace decision path |
| ScratchBird | `include/scratchbird/core/catalog_manager.h` | `TableInfo` | `425` | Catalog metadata carrying placement and migration-target state |
| ScratchBird | `src/core/catalog_manager.cpp` | `CatalogManager::resolveTablespaceBindings` | `4363` | Rebinds table placement from tablespace UUIDs into IDs |
| ScratchBird | `src/core/catalog_manager.cpp` | table migration catalog update | `29076` | Updates table placement metadata after successful migration |
| ScratchBird | `src/core/catalog_manager.cpp` | `table_info.migration_target_ts = target_tablespace_id` | `29234` | Migration target is recorded in table metadata during in-progress relocation |
| ScratchBird | `src/core/catalog_manager.cpp` | migration completion metadata clear | `30192` | Clears migration flags after final placement switch |
| ScratchBird | `include/scratchbird/core/storage_engine.h` | `StorageEngine::getTuple(const ID&, const TID&, ...)` | `181` | Execution path that consumes relocation truth |
| ScratchBird | `include/scratchbird/core/heap_page.h` | `ItemPointer` | `37` | Stable slot identity authority |
| ScratchBird | `include/scratchbird/core/heap_page.h` | `TupleHeader` | `91` | CTID, back-version, row UUID, and moved-flag authority |
| ScratchBird | `src/core/transaction_manager.cpp` | `TransactionManager::flushTransactionState` | `3750` | Terminal durability fence across all affected filespaces |
| ScratchBird | `src/core/transaction_manager.cpp` | `TransactionManager::flushTransactionPublicationState` | `3800` | Publication ordering around movement-sensitive operations |

## Drift and contradictions

- Older prose described a complete partition split controller with exact cutover
  rules, migration-history persistence, and online/offline orchestration.
- The reviewed code proves tuple relocation support, but the dedicated
  partition-boundary split engine, durable history, and explicit cutover state
  remain implementation drift against the Beta 1 contract.
- Older prose described object relocation as a generalized operator-controlled
  lifecycle. That lifecycle is now explicit Beta 1 required behavior rather
  than an unsupported aspirational note.

## Suggestions

- Keep split/cutover under one migration-orchestration authority shared with
  section `24` publication rules.
- Treat resolver routing as substrate, not as the whole operator contract.
- Require exact code-map closure for history publication, cutover fence, and
  refusal handling as implementation progresses.
