# Heap/TOAST/LOB Page Walk Results

## Diagnostic Cases
1. Valid heap page walk: no issues, `Status::OK`.
2. Invalid tuple payload length: issue `HEAP_PAYLOAD_BOUNDS`, `Status::PAGE_CORRUPT`.
3. TOAST flag/payload mismatch: issue `HEAP_TOAST_POINTER_MISMATCH`, `Status::PAGE_CORRUPT`.
4. Missing chunk index in sequence: issue `LOB_CHUNK_MISSING`, `Status::NOT_FOUND`.
5. Contiguous chunk sequence: no issues, `Status::OK`.

## Source
- `tests/unit/test_heap_toast_lob_diagnostics.cpp`
- `src/core/heap_toast_lob_diagnostics.cpp`
