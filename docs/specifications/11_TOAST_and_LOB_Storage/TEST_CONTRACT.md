# Test Contract: TOAST and LOB Storage

Status: current_authority

Last code-audit date: 2026-03-30

This file defines the minimum proof surface for section 11.

## Current implementation-backed proof

- `test_storage_engine.cpp` proves savepoint restore, delete lifecycle, orphan
  detection, and storage-engine TOAST orchestration paths
- `test_toast_gc_contract.cpp` proves TIP-aware cleanup of committed, aborted,
  and active TOAST delete paths
- `test_toast_tip_visibility.cpp` proves chunk visibility remains TIP-aware and
  MGA-compliant
- `test_heap_toast_integration.cpp` and
  `integration/test_toast_crash_recovery_mga.cpp` prove heap or TOAST
  integration and restart-sensitive TOAST visibility behavior

## Required proof artifacts

- test_heap_toast_integration.cpp shall prove heap and TOAST integration behavior.
- test_toast_cleanup.cpp shall prove cleanup paths.
- test_toast_cleanup_ordering.cpp shall prove cleanup-ordering behavior.
- test_toast_gc_contract.cpp shall prove GC-facing TOAST contract behavior.
- test_toast_tip_visibility.cpp shall prove TIP-based TOAST chunk visibility.
- test_heap_page_toast_api.cpp shall prove heap-page TOAST API behavior.
- test_heap_toast_lob_diagnostics.cpp shall prove heap and TOAST and LOB diagnostics.
- test_toast_operations.cpp shall prove ToastManager operational behavior.
- test_lob_page_layout_contract.cpp shall prove the currently shipped oversized-value layout and diagnostic expectations for TOAST and LOB-named page-family handling.
- test_page_manager_overflow.cpp and test_overflow_fix.cpp are adjacent proof surfaces for oversized-value lifecycle interactions and overflow handling boundaries.

## Required behavioral assertions

- The proof surface shall show that oversized values are currently stored through TOAST-first runtime paths.
- The proof surface shall show that chunk visibility is TIP-aware and MGA-compliant.
- The proof surface shall show that delete behavior is soft delete through xmax, not immediate physical removal.
- The proof surface shall show that cleanup ordering and deferred cleanup remain aligned with MGA visibility.
- The proof surface shall show that diagnostics detect invalid TOAST pointers, flag mismatch, and missing or invalid chunk sequences.

## Explicit negative requirements

- This section shall not be treated as proving a generic standalone streaming LOB API.
- This section shall not be treated as proving standalone LOB relocation behavior.
- This section shall not be treated as proving standalone operator-facing LOB control surfaces.
