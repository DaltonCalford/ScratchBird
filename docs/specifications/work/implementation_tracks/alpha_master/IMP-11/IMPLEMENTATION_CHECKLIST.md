# IMP-11 Implementation Checklist

## Ticket
- ID: IMP-11
- Section: 11_TOAST_and_LOB_Storage
- Gate Contract: docs/specifications/11_TOAST_and_LOB_Storage/TEST_CONTRACT.md

## Inputs
- docs/specifications/11_TOAST_and_LOB_Storage/SPEC_OUTLINE.md
- docs/specifications/11_TOAST_and_LOB_Storage/TOAST_POINTER_FORMAT.md
- docs/specifications/11_TOAST_and_LOB_Storage/LOB_PAGE_LAYOUTS.md
- docs/specifications/11_TOAST_and_LOB_Storage/LOB_IO_SEMANTICS.md
- docs/specifications/11_TOAST_and_LOB_Storage/LOB_FILESPACE_RELOCATION.md
- docs/specifications/11_TOAST_and_LOB_Storage/TEST_CONTRACT.md

## Ordered Tasks
1. Implement deterministic LOB/TOAST page layouts and pointer envelope format.
2. Implement LOB create/read/write/append/truncate streaming semantics.
3. Implement MGA-aware LOB version visibility and GC/sweep cleanup.
4. Implement online filespace relocation with watermark catch-up and atomic pointer swap.
5. Implement offline relocation flow and deterministic restart/recovery handling.
6. Implement pointer integrity checks before and after relocation.
7. Implement rollback behavior for failures before and during pointer swap.
8. Implement required, negative, performance, and compatibility test suites and evidence capture.

## Exit Criteria
- Required tests pass.
- Gate result is pass.
- Traceability maps requirements to deterministic artifacts.
