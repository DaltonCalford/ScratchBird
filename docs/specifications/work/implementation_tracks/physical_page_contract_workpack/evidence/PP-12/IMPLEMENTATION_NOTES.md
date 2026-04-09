# Implementation Notes

- Added canonical TOAST/LOB layout structs in `ondisk.h` with strict size assertions.
- Added reusable validation helpers for chunk payload bounds and contiguous chunk-index range rules.
- Added `LobPageLayoutContractTest` to enforce enum split, struct sizes, and chunk validation behavior.
