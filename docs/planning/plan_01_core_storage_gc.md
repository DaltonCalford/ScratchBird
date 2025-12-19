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
1) Heap page ownership metadata (table_id on heap pages or mapping index).
2) Columnstore persistence + segment catalog.
3) Columnstore GC and vacuum.
4) Index GC coverage for all index types.
5) Table migration end-to-end correctness and index TID updates.

## Implementation Tasks
- Add table ownership metadata for heap pages (PageHeader or separate map).
- Update enumerateTablePages to return exact table pages.
- Persist columnstore segment catalog and implement read/write path.
- Implement ColumnstoreIndexSimple::removeDeadEntries and vacuum compaction.
- Extend GarbageCollector to open and clean FULLTEXT, GIST, RTREE, SPGIST, BITMAP, COLUMNSTORE, LSM.
- Implement index TID update for VECTOR/HNSW, FULLTEXT, GIN, GIST, BRIN, RTREE during migration.
- Reconcile moveTableToTablespace stub messaging with actual logic.

## Required Data/Schema Changes
- Heap page metadata must include table ownership (UUID) or a reliable mapping table.
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

## Acceptance Criteria
- Mixed tablespace enumeration returns only target table pages.
- Columnstore data persists across restart and matches pre-restart reads.
- GC removes dead entries from every index type with no false deletions.
- Table migration produces identical query results before/after move.

## Implementation Notes (Concrete)
- **Heap page ownership**: add `table_id` (UUID) to heap page header or a sidecar mapping table. If sidecar, add `CatalogManager::registerPageOwner(GPID, table_id)` and `getPageOwner(GPID)`.
- **Enumeration API**: `CatalogManager::enumerateTablePages(const ID& table_id, std::vector<GPID>& pages_out, ErrorContext* ctx)` must filter by ownership.
- **Columnstore**: persist segment catalog to meta page; implement `loadSegmentCatalog()` / `saveSegmentCatalog()` to read/write via `PageManager`.
- **GC interface**: every index type implements `IndexGCInterface::removeDeadEntries(const std::vector<TID>&, ...)`.
- **Migration**: `updateIndexTIDs` must handle all `IndexType` values and enforce rollback on failure.

## Expanded API/Schema Details
- **Page ownership field**: extend `PageHeader` with `table_id` for `PAGE_TYPE_HEAP`, or add `sb_page_owners(gpid, table_id)`.
- **CatalogManager**:
  - `enumerateTablePages(const ID& table_id, std::vector<GPID>& pages_out, ErrorContext* ctx)`
  - `copyPageWithTIDRemapping(...)` must set target `table_id`.
  - `rollbackPageMigration(...)` deallocates all target GPIDs in `tid_mapping`.
- **Index GC**: implement `removeDeadEntries(...)` for FULLTEXT, GIST, RTREE, SPGIST, BITMAP, COLUMNSTORE, LSM classes.
- **Columnstore**: `ColumnstoreIndexSimple::create/open/insertColumn/insertRow/flushRowBuffer` must persist segment catalog and data pages.

## Full Implementation Detail (No Ambiguity)
- **On-disk heap page ownership**:
  - Add `UuidV7Bytes table_id` to `PageHeader` for heap pages in `docs/specifications/ON_DISK_FORMAT.md`.
  - Set `table_id` when allocating new heap pages and when copying during migration.
- **Columnstore segment catalog format**:
  - Store catalog in meta page: header (`version`, `segment_count`), followed by fixed-size `ColumnSegment` entries.
  - Each `ColumnSegment` must include `column_id`, `start_row`, `row_count`, `page_number`, `compression_type`, `min_value`, `max_value`.
- **Index GC contract**:
  - Each index class implements `removeDeadEntries(const std::vector<TID>& dead_tids, uint64_t* entries_removed_out, uint64_t* pages_modified_out, ErrorContext* ctx)`.
  - Must be idempotent and safe to call multiple times.
- **Migration TID updates**:
  - `CatalogManager::updateIndexTIDs` must handle all `IndexType` values: BTREE, HASH, HNSW, FULLTEXT, GIN, GIST, BRIN, RTREE, SPGIST, BITMAP, COLUMNSTORE, LSM.
  - On failure: rollback allocated target pages and restore catalog state.

## Concrete Test Cases
- **Heap ownership**: create two tables in same tablespace; verify `enumerateTablePages` returns correct page counts for each table.
- **Columnstore persistence**: insert rows, restart DB, scan columnstore index, verify data and min/max metadata.
- **Index GC**: mark tuples dead, run GC, assert index entries removed for each index type.
- **Migration**: migrate table with multiple index types; run identical SELECTs before/after; verify indexes remain valid.

## Concrete DDL / Structs (Example)
- `sb_page_owners` (if sidecar mapping is used):
  - `gpid BIGINT PRIMARY KEY`
  - `table_id UUID`
- `ColumnSegment` struct (persisted):
  - `uint16 column_id; uint32 start_row; uint32 row_count; uint32 page_number; uint8 compression_type; int64 min_value; int64 max_value;`

## Full Catalog DDL (Required if sidecar mapping used)
```sql
CREATE TABLE sb_page_owners (
  gpid BIGINT PRIMARY KEY,
  table_id UUID NOT NULL
);
```

## Common Failure Patterns
- Implemented only in executor/parser; `CatalogManager` direct calls still bypass logic.
- Cache updates without on-disk persistence or load path; restart loses behavior.
- Switch statements or enum mappings missing new values, producing `<unknown>` and wrong behavior.
- CASCADE/RESTRICT or config gating ignored; dependency checks bypassed or inconsistent.
- Tests cover happy-path only; missing restart, negative, and concurrency/lock-order cases.
- Spec deviations introduced without explicit config flags or documentation.
