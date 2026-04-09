# Implementation Notes

Status: `Completed`

## Completed in this pass
- Added CAT-018 catalog root fields and persistence wiring:
  - `filespace_stats_page`
  - `lob_page`
  - `lob_page_map_page`
  - `backup_history_page`
- Added bootstrap allocation and legacy backfill allocation for all CAT-018 families.
- Added on-disk record contracts in `CatalogManager`:
  - `FilespaceStatsRecord`
  - `LobCatalogRecord`
  - `LobPageCatalogRecord`
  - `BackupHistoryRecord`
- Added full CAT-018 CRUD/public APIs for:
  - `filespace_stats`
  - `lob`
  - `lob_page`
  - `backup_history`
- Enforced deterministic constraints:
  - `filespace_stats`: bounded usage/free/high-water invariants and non-zero filespace UUID existence checks.
  - `lob`: encrypted records require non-null `encryption_key_uuid`.
  - `lob_page`: parent `lob` must exist, UNIQUE(`lob_uuid`, `page_index`) enforced, and `chunk_bytes` bounded by page payload capacity.
  - `backup_history`: enum validation with status-dependent field requirements (`SUCCESS` requires completion time, `FAILED` requires error text).
- Added/updated CAT-018 bootstrap persistence and storage extension contract tests.
