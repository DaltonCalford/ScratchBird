# Implementation Notes

- Added `HeapToastLobDiagnostics` API for strict page walk and chunk-sequence checks.
- Page walk now emits explicit issue codes for header, slot, tuple-size, and TOAST-pointer contract violations.
- Added unit tests covering good-page pass, invalid payload detection, TOAST flag mismatch detection, and chunk-gap rejection.
