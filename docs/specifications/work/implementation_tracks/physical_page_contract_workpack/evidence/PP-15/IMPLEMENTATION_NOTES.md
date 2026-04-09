# Implementation Notes

- Added canonical `IndexPageHeader` (32 bytes) in `ondisk.h` to represent the required index special-area base contract.
- Added explicit index-page flag constants and reserved-bit validation helper.
- Added base header validation helper for `opaque_len`/`reserved` and UUID round-trip helper functions.
- Added `IndexPageBaseLayoutContractTest` to enforce struct layout, flag rules, header checks, and UUID stability.
