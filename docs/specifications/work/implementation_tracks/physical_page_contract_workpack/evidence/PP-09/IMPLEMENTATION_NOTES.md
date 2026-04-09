# Implementation Notes

- Added canonical record contract fields to `TupleHeader` and enforced contract propagation in `HeapPage` mutation paths.
- Insert now always normalizes `row_uuid`, `record_format`, `payload_len`, and canonical record flags.
- Delete updates tuple header contract flags before line-pointer tombstoning.
- Update and overwrite preserve stable logical `row_uuid` across version chains and keep flags synchronized.
- Added `HeapRecordContractTest` as the PP-09 executable contract gate.
