# Implementation Notes

- Added explicit `TupleHeader::getLastEditTxidSystem()` to codify `[sb_col]last_edit_txid` semantics.
- Added `HeapPage::extractSystemColumns()` as the engine-side deterministic extractor for `[sb_col]row_uuid` and `[sb_col]last_edit_txid`.
- Extended `HeapRecordContractTest` with live-row and tombstone system-column extraction checks.
