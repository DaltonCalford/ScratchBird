# Implementation Notes

- Replaced legacy TOAST pointer layout with canonical 32-byte `ToastPointer` (`lob_uuid`, `total_len`, `chunk_size`, `compression`, `flags`).
- Rewrote TOAST pointer validation to canonical field checks and UUIDv7 plausibility checks.
- Reworked `toastValue` / `detoastValue` to use canonical compression/flags semantics.
- Updated heap/GC/catalog/storage paths and unit tests to canonical field names.
