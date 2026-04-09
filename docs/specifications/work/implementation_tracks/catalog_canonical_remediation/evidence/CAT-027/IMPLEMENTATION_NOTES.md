# Implementation Notes

Status: `Completed`

## Completed in this pass
- Implemented CAT-027 physical catalog wiring in `CatalogManager`:
  - Added 11 replication runtime/conflict page slots to `CatalogRootPage`.
  - Wired root read/write, initialize-time allocation, and legacy backfill paths.
- Added CAT-027 enum model + validators:
  - `ReplicationDirection`, `ReplicationChannelState`, `ReplicationMemberRole`,
    `ReplicationCursorState`, `ReplicationTxnState`, `ReplicationRetryState`,
    `ReplicationDdlPolicy`, `ReplicationConflictPolicy`,
    `ReplicationConflictKind`, `ReplicationResolutionState`,
    `ReplicationEventKind`.
- Added full deterministic CRUD APIs for all CAT-027 families in
  `src/core/catalog_manager.cpp` and declarations in
  `include/scratchbird/core/catalog_manager.h`:
  - `replication_channel`
  - `replication_channel_member`
  - `replication_origin`
  - `replication_cursor`
  - `replication_origin_progress`
  - `replication_txn_batch`
  - `replication_apply_log`
  - `replication_retry_queue`
  - `replication_conflict`
  - `replication_split_brain_event`
  - `replication_error`

## Contract enforcement highlights
- Referential checks across channel/member/origin/batch/error relationships.
- Deterministic uniqueness checks on normative keys (channel name, member tuple,
  cursor tuple, progress tuple, batch tuple, apply target tuple, retry batch tuple).
- Enum and temporal consistency guards return `Status::PAGE_CORRUPT` on invalid stored state.
- Soft-delete semantics (`is_valid=0`) across all CAT-027 rows.
- One-way member role constraints (no peers, max one active publisher/subscriber).
