# Plan 01 Clarifications Appendix (Copy/Paste)

This appendix restates the resolved clarifications so a low-capability agent can copy/paste without interpretation.

## Columnstore (Simple) Meta Pages
- Implementation target is `ColumnstoreIndexSimple` in `src/core/columnstore_index.cpp` (do NOT switch to `ColumnstoreIndex` in `src/core/columnstore.cpp` for this plan).
- Dual meta pages:
  - `meta_page_a = index_info.root_page`.
  - `meta_page_b` is allocated during `ColumnstoreIndexSimple::create()` and its page ID is stored in the meta header as `peer_page_id`.
  - Always read both pages: read `meta_page_a`, then use its `peer_page_id` to read `meta_page_b` (even if `meta_page_a` checksum fails).
- Meta header layout: `magic`, `version`, `generation`, `segment_count`, `peer_page_id`, `checksum`.
- CRC32C: `scratchbird::core::crc32cCompute(data, len, 0xFFFFFFFF) ^ 0xFFFFFFFF`.
- Winner selection: newest valid `generation`. If both invalid -> mark index failed and require rebuild.
- `generation` is `uint64_t`. No wrap handling in alpha; if it ever wraps, treat as corruption and rebuild.

## Index Version Visibility and GC
- Index visibility rule (no per-transaction tracking table):
  - Choose version where `state == ACTIVE`,
  - `valid_from_xid <= txn_xid`,
  - and (`retired_xid == 0` or `txn_xid < retired_xid`).
- Old index GC rule:
  - Safe when `retired_xid < TransactionManager::getOldestActiveXid()`,
  - and if snapshot transactions exist, also require `retired_xid < TransactionManager::getOldestSnapshot()`.
- Index names are unique within a table namespace. Shadow rebuild must not create a second user-visible name in the same table.

