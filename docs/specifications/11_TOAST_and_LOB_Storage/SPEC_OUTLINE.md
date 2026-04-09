# 11 TOAST and LOB Storage: Spec Outline

## Canonical subsection map

1. `TOAST_POINTER_FORMAT.md`
Defines the packed 32-byte pointer layout, flag vocabulary, validation rules, and tuple-level pointer detection rules.

2. `LOB_PAGE_LAYOUTS.md`
Defines the normative TOAST chunk-row storage contract, chunk lifecycle states, diagnostic classes, and the limited role of `LOB`-named page support.

3. `LOB_IO_SEMANTICS.md`
Defines `ToastManager` initialization, TOAST-table creation, strategy selection, out-of-line write semantics, detoast semantics, delete semantics, and retire semantics.

4. `LOB_FILESPACE_RELOCATION.md`
Defines the current fail-closed boundary for standalone LOB relocation and the only current relocation-adjacent truth: MGA-safe deferred cleanup of old oversized values.

5. `DEPENDENCIES.md`
Defines the code-owned and cross-section dependencies consumed by this section.

6. `TEST_CONTRACT.md`
Defines the minimum proof surface for TOAST integration, visibility, cleanup ordering, diagnostics, and layout validation.

## Normative scope

Per-table TOAST table creation and ownership.

Packed `ToastPointer` layout and validation.

MGA-aware chunk lifecycle classification.

TOAST write, read, delete, and retire semantics.

Chunk diagnostics and sequence validation.

## Explicit exclusions

This section does not define a generic standalone LOB streaming API.

This section does not define standalone LOB relocation operations.

This section does not define standalone operator-facing LOB control surfaces.
