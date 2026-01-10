# Plan 01 - Core Storage and GC Completion

## Scope
Close core storage gaps: columnstore durability/GC, index GC coverage, heap page ownership, table migration correctness.

## Priority
P0 (blocks correctness and long-term durability).

## References
- `docs/specifications/STORAGE_ENGINE_MAIN.md`
- `docs/specifications/STORAGE_ENGINE_PAGE_MANAGEMENT.md`
- `docs/specifications/INDEX_GC_PROTOCOL.md`
- `docs/specifications/COLUMNSTORE_SPEC.md`
- `docs/specifications/HEAP_TOAST_INTEGRATION.md`

## Order of Implementation
1) Heap page ownership metadata (table_id in PageHeader).
2) Columnstore persistence + segment catalog.
3) Columnstore GC and vacuum.
4) Index GC coverage for all index types.
5) Table migration end-to-end correctness and index TID updates.

## Decision Gates (Must Be Resolved Before Coding)
- **Heap page ownership**: **A) PageHeader table_id** (insert immediately after `database_uuid`, PageHeader grows to 80 bytes; bump on-disk format; no upgrade path required).
- **Columnstore catalog persistence**: **dual meta page** with generation counter + CRC32C checksum; newest valid wins on open; if both invalid, mark index failed and require rebuild.
- **Migration safety**: **shadow index rebuild + versioned swap**:
  - Build new index while old index remains active.
  - Switch new transactions to the new index only after build completes.
  - Old transactions keep using the old index until they finish, then old index is GC’d.
  - If migration fails mid-build, new index is never made visible.

## Implementation Tasks
- Add table ownership metadata for heap pages (PageHeader table_id).
- Update enumerateTablePages to return exact table pages.
- Persist columnstore segment catalog and implement read/write path.
- Implement ColumnstoreIndexSimple::removeDeadEntries and vacuum compaction.
- Extend GarbageCollector to open and clean FULLTEXT, GIST, RTREE, SPGIST, BITMAP, COLUMNSTORE, LSM.
- Implement shadow index rebuild + versioned swap for all index types (preferred migration path).
- Keep/update `updateIndexTIDs` only if required for offline in-place migration; otherwise route migration to shadow rebuild.
- Reconcile moveTableToTablespace stub messaging with actual logic.

## Known Stub Locations (Must Be Replaced)
- `src/core/catalog_manager.cpp` `CatalogManager::updateIndexTIDs` has NOT IMPLEMENTED cases for VECTOR/HNSW, FULLTEXT, GIN, GIST, BRIN, RTREE and is missing switch cases for SPGIST, BITMAP, COLUMNSTORE, LSM.
- `src/core/catalog_manager.cpp` `CatalogManager::moveTableToTablespace` contains STUB migration logic (logs warn and skip copying).
- `src/core/columnstore_index.cpp` `ColumnstoreIndexSimple::removeDeadEntries` and `vacuum` are stubbed.
- `src/core/fulltext_index.cpp` has no GC interface; must delegate GC/TID updates to underlying GIN.
- `src/core/gist_index.cpp` GC signature uses `oldest_active_xid` (not IndexGCInterface) and must be reconciled.

## Required Data/Schema Changes
- Heap page metadata must include table ownership UUID in the PageHeader (no sidecar mapping table).
- Columnstore segment catalog must be persisted and versioned.
- Index GC interface must be implemented for all IndexType values in CatalogManager.

## Completion Checklist (Developer)
- [ ] Heap pages have table ownership metadata.
- [ ] enumerateTablePages returns only target table pages.
- [ ] Columnstore segment catalog is persisted and loaded on open.
- [ ] Columnstore GC removes dead entries; vacuum compacts segments.
- [ ] GC supports all index types in catalog IndexType enum.
- [ ] Table migration updates all index types and is transactionally safe.
- [ ] Logs and error paths match actual migration behavior.

## Completion Checklist (Auditor)
- [ ] Verify heap pages include table ownership field and are populated on insert/update.
- [ ] Confirm enumerateTablePages filters correctly in mixed tablespace.
- [ ] Restart test shows columnstore data persists and is queryable.
- [ ] GC removes dead entries from each index type (unit/integration tests).
- [ ] Migration test covers multiple index types and validates post-move queries.
- [ ] Regression: no unexpected logs claiming stub behavior when full logic runs.

## Testing Requirements
- Unit tests for page ownership metadata and enumeration.
- Columnstore persistence tests (create, restart, read).
- GC tests per index type (with dead TIDs).
- Migration tests with multiple indexes and TOAST data.
- Per-index TID update tests (each IndexType exercised at least once).
- Shadow index rebuild tests:
  - New index stays invisible during build.
  - New transactions use new index after swap; old transactions remain on old index.
  - Old index GC after last reader.

## Acceptance Criteria
- Mixed tablespace enumeration returns only target table pages.
- Columnstore data persists across restart and matches pre-restart reads.
- GC removes dead entries from every index type with no false deletions.
- Table migration produces identical query results before/after move, with shadow index swap preserving transaction isolation.

## Implementation Notes (Concrete)
- **Heap page ownership**: add `table_id` (UUID) to heap page header; do not implement sidecar mapping.
- **Enumeration API**: `CatalogManager::enumerateTablePages(const ID& table_id, std::vector<GPID>& pages_out, ErrorContext* ctx)` must filter by ownership.
- **Columnstore (Simple)**: persist segment catalog to dual meta pages owned by `ColumnstoreIndexSimple` (`src/core/columnstore_index.cpp`). This is the runtime columnstore used by executor/storage_engine; do not switch to `ColumnstoreIndex` (`src/core/columnstore.cpp`) in this plan.
- **Columnstore**: implement `loadSegmentCatalog()` / `saveSegmentCatalog()` to read/write via `PageManager`. Use dual meta pages with generation counter + CRC32C checksum (`scratchbird::core::crc32cCompute(data, len, 0xFFFFFFFF) ^ 0xFFFFFFFF`).
- **GC interface**: every index type implements `IndexGCInterface::removeDeadEntries(const std::vector<TID>&, ...)`.
- **Migration**: `updateIndexTIDs` must handle all `IndexType` values and enforce rollback on failure.
- **Migration (preferred)**: shadow index rebuild + versioned swap; `updateIndexTIDs` is fallback only if in-place migration is explicitly enabled.
- **Shadow index rebuild**:
  - New index state: `BUILDING` → `ACTIVE` (for new transactions only) → old index `RETIRED` → GC when no transactions reference it.
  - Index metadata must track `valid_from_xid` (or epoch) and `retired_xid` for visibility.
  - Migration must not switch index visibility until build completes successfully.
  - Maintain a stable `logical_index_id` across rebuilds (derive from `table_id + index_name`, or store explicit `logical_index_id` in catalog).
  - Index names are unique **within a table namespace**; `table_id` is the parent for name resolution (same rule for table triggers).
  - Shadow rebuild must not create a second user-visible name in the same table. Use an internal shadow name (e.g., `__sb_shadow_<logical_id>_<build_xid>`) or a hidden flag, but expose only the logical index name to users.
  - **Index version visibility rule**: choose version where `state == ACTIVE`, `valid_from_xid <= txn_xid`, and (`retired_xid == 0` or `txn_xid < retired_xid`). No per-transaction tracking table required; XID range is the contract.
  - **Old index GC rule**: safe when `retired_xid < TransactionManager::getOldestActiveXid()` and (if snapshot txns exist) `retired_xid < TransactionManager::getOldestSnapshot()`; otherwise OAT alone.

## Index GC + Migration Implementation Map (Explicit)
See `docs/archive/2026-01-04/planning/plan_01_index_gc_clarifications.md` for per-index GC traversal/removal specifics.
- **BTREE**: `src/core/btree.cpp` `BTree::removeDeadEntries` and `BTree::updateTIDsAfterMigration`.
- **HASH**: `src/core/hash_index.cpp` `HashIndex::removeDeadEntries` and `HashIndex::updateTIDsAfterMigration`.
- **HNSW/VECTOR**: `src/core/hnsw_index.cpp` `HnswIndex::removeDeadEntries` and `HnswIndex::updateTIDsAfterMigration` (wire into `CatalogManager::updateIndexTIDs`).
- **FULLTEXT**: `src/core/fulltext_index.cpp` must expose GC/TID updates by delegating to `GinIndex`.
- **GIN**: `src/core/gin_index.cpp` `GinIndex::removeDeadEntries` and `GinIndex::updateTIDsAfterMigration`.
- **GIST**: `src/core/gist_index.cpp` must implement IndexGCInterface-compatible `removeDeadEntries(const std::vector<TID>&, ...)` or GC adapter; add migration update method.
- **BRIN**: `src/core/brin_index.cpp` `BrinIndex::removeDeadEntries`; add migration update method.
- **RTREE**: `src/core/rtree_index.cpp` `RTreeIndex::removeDeadEntries`; add migration update method.
- **SPGIST**: `src/core/spgist_index.cpp` `SPGiSTIndex::removeDeadEntries`; add migration update method.
- **BITMAP**: `src/core/bitmap_index.cpp` `BitmapIndex::removeDeadEntries`; add migration update method.
- **COLUMNSTORE**: `src/core/columnstore_index.cpp` `ColumnstoreIndexSimple::removeDeadEntries` (implement), `vacuum` (implement), add migration update method if columnstore stores TIDs.
- **LSM**: `src/core/lsm_tree_index.cpp` must implement IndexGCInterface `removeDeadEntries` and migration update method.

## Expanded API/Schema Details
- **Page ownership field**: extend `PageHeader` with `table_id` for `PAGE_TYPE_HEAP` (placed immediately after `database_uuid`; header size becomes 80 bytes).
- **CatalogManager**:
  - `enumerateTablePages(const ID& table_id, std::vector<GPID>& pages_out, ErrorContext* ctx)`
  - `copyPageWithTIDRemapping(...)` must set target `table_id`.
  - `rollbackPageMigration(...)` deallocates all target GPIDs in `tid_mapping`.
- **Index GC**: implement `removeDeadEntries(...)` for FULLTEXT, GIST, RTREE, SPGIST, BITMAP, COLUMNSTORE, LSM classes.
- **Columnstore**: `ColumnstoreIndexSimple::create/open/insertColumn/insertRow/flushRowBuffer` must persist segment catalog and data pages.

## Full Implementation Detail (No Ambiguity)
- **On-disk heap page ownership**:
  - Add `UuidV7Bytes table_id` to `PageHeader` for heap pages in `docs/specifications/ON_DISK_FORMAT.md`, immediately after `database_uuid` (PageHeader becomes 80 bytes).
  - Set `table_id` when allocating new heap pages and when copying during migration.
  - Bump on-disk format version; no upgrade path required (tests recreate databases).
  - If a heap page is encountered with zero/invalid `table_id`, treat as corruption and hard error.
- **Columnstore segment catalog format**:
  - **Dual meta pages**: `meta_page_a = index_info.root_page` and `meta_page_b` allocated during `ColumnstoreIndexSimple::create()` and stored in the meta header as `peer_page_id`. Always read both pages using `meta_page_a` and its `peer_page_id` (even if `meta_page_a` checksum fails).
  - Store catalog in meta page: header (`magic`, `version`, `generation`, `segment_count`, `peer_page_id`, `checksum`), followed by fixed-size `ColumnSegment` entries.
  - Use CRC32C over the catalog payload for meta page validation.
  - If both meta pages are invalid, mark index failed and require rebuild.
  - `generation` is `uint64_t`; no wrap handling required for alpha (if it ever wraps, treat as corruption and rebuild).
  - Each `ColumnSegment` must include `column_id`, `start_row`, `row_count`, `page_number`, `compression_type`, `min_value`, `max_value`.
- **Index GC contract**:
  - Each index class implements `removeDeadEntries(const std::vector<TID>& dead_tids, uint64_t* entries_removed_out, uint64_t* pages_modified_out, ErrorContext* ctx)`.
  - Must be idempotent and safe to call multiple times.
- **Migration TID updates**:
  - `CatalogManager::updateIndexTIDs` must handle all `IndexType` values: BTREE, HASH, HNSW, FULLTEXT, GIN, GIST, BRIN, RTREE, SPGIST, BITMAP, COLUMNSTORE, LSM.
  - On failure: rollback allocated target pages and restore catalog state.

## Checkpoints (Required for Low-Capability AI)
- **Checkpoint A**: Decision gates resolved and recorded in this plan.
- **Checkpoint B**: Heap ownership mechanism implemented + unit test + restart test.
- **Checkpoint C**: Columnstore catalog persistence implemented + restart test.
- **Checkpoint D**: GC implemented for all index types + per-index tests.
- **Checkpoint E**: Shadow index rebuild + version swap implemented + regression tests before/after.

## Concrete Test Cases
- **Heap ownership**: create two tables in same tablespace; verify `enumerateTablePages` returns correct page counts for each table.
- **Columnstore persistence**: insert rows, restart DB, scan columnstore index, verify data and min/max metadata.
- **Index GC**: mark tuples dead, run GC, assert index entries removed for each index type.
- **Migration**: migrate table with multiple index types; run identical SELECTs before/after; verify indexes remain valid.

## Concrete DDL / Structs (Example)
- `ColumnSegment` struct (persisted):
  - `uint16 column_id; uint32 start_row; uint32 row_count; uint32 page_number; uint8 compression_type; int64 min_value; int64 max_value;`
- **Index version metadata (required for shadow rebuild + swap)**:
  - `sys.catalog.index_versions` (tracks visibility of physical index instances):
    - `logical_index_id UUID NOT NULL`  -- stable id for (table_id + index_name), or explicit stored logical ID
    - `index_id UUID NOT NULL`          -- physical index instance id
    - `state SMALLINT NOT NULL`         -- 0=BUILDING, 1=ACTIVE, 2=RETIRED, 3=FAILED
    - `valid_from_xid BIGINT NOT NULL`  -- XID at which new txns can use this index
    - `retired_xid BIGINT`              -- XID after which no new txns use this index
    - `build_started_time BIGINT NOT NULL`
    - `build_completed_time BIGINT`
    - `created_time BIGINT NOT NULL`
  - Index name uniqueness is enforced within a table namespace; shadow rebuild uses internal names or hidden flags to avoid user-visible name conflicts.

## Full Catalog DDL (Index version metadata)
```sql
CREATE TABLE sys.catalog.index_versions (
  logical_index_id UUID NOT NULL,
  index_id UUID NOT NULL,
  state SMALLINT NOT NULL,
  valid_from_xid BIGINT NOT NULL,
  retired_xid BIGINT,
  build_started_time BIGINT NOT NULL,
  build_completed_time BIGINT,
  created_time BIGINT NOT NULL,
  PRIMARY KEY (logical_index_id, index_id)
);

CREATE INDEX index_versions_state_idx
  ON sys.catalog.index_versions(logical_index_id, state);
```
```

## Common Failure Patterns
- Implemented only in executor/parser; `CatalogManager` direct calls still bypass logic.
- Cache updates without on-disk persistence or load path; restart loses behavior.
- Switch statements or enum mappings missing new values, producing `<unknown>` and wrong behavior.
- CASCADE/RESTRICT or config gating ignored; dependency checks bypassed or inconsistent.
- Tests cover happy-path only; missing restart, negative, and concurrency/lock-order cases.
- Spec deviations introduced without explicit config flags or documentation.
