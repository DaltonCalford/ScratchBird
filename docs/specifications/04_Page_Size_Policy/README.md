# 04 Page Size Policy

## Status
- Section status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27
- Primary repo audited: `ScratchBird`

## Current status
- The current engine supports fixed per-database page sizes of `8192`, `16384`, `32768`, `65536`, and `131072` bytes.
- Page size is validated at database creation, persisted in the database header, and revalidated at open time.
- Secondary tablespaces must match the database page size; mixed page-size tablespace sets are rejected.
- Restore-time validation also rechecks page size and requires `block_size` to match the persisted header page size.
- Large-page support is real, but narrower than the old prose suggested: header lower/special offset helpers switch to two-byte units when `page_size > 65535`, and heap item pointers use a 32-bit offset plus a 31-bit length field.

## Major drift now recorded
- The older prose treated `storage.page_size` as a proven canonical configuration control. This pass did not prove that engine-owned config surface; the current authoritative selection surface is the `Database::create(..., page_size, ...)` API.
- The older prose treated `16KB` as an authoritative engine default. The reviewed code proves that `16KB` is a common recommendation and test choice, not a mandatory engine default policy.
- The older prose implied “large page mode” as a broader subsystem. The code proves specific header/unit-scaling and heap-item structural consequences for `64KB` and `128KB`, not a separate runtime mode.

## Section file status
- `README.md`: code-backed section summary, current_authority_with_reconstructed_expansion
- `SPEC_OUTLINE.md`: normalized to current implementation depth, current_authority_with_reconstructed_expansion
- `DECISION_RECORD.md`: normalized to code-backed page-size decisions, current_authority_with_reconstructed_expansion
- `DEPENDENCIES.md`: normalized to current subsystem dependencies, current_authority_with_reconstructed_expansion
- `TEST_CONTRACT.md`: normalized to current code-backed gate expectations, current_authority_with_reconstructed_expansion

## Primary audit lookup anchors
- `src/core/database.cpp` search `Invalid page size in database header` for
  open-path header validation.
- `src/core/page_manager.cpp` search
  `Cannot open tablespace with different page size` for tablespace mismatch
  refusal.

## Section file index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [PAGE_SIZE_SELECTION_COMPATIBILITY_AND_OPERATOR_POLICY_MODEL.md](PAGE_SIZE_SELECTION_COMPATIBILITY_AND_OPERATOR_POLICY_MODEL.md)
- `SECTION_CLOSURE_MATRIX.csv`
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->

## Non-blocking expansion candidates
- An explicit documented policy for future expansion beyond `128KB`, if that remains in scope

## Suggestions
- Keep this section anchored to the fixed-per-database page-size model the code actually implements.
- Treat `16KB` as recommendation language unless the engine acquires a true default-selection authority.
- Keep all future large-page claims tied to exact structure/layout consequences, not to vague “mode” language.
