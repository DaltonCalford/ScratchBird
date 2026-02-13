# V3 UUID Architecture Audit and Fixes Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/UUID_ARCHITECTURE_AUDIT_AND_FIXES.md`

## Summary
- Document is labeled **non-authoritative** and reports a UUID usage audit.
- Some claims align with current code (e.g., B-Tree column UUIDs, root schema UUID = database UUID), but other claims conflict with current implementation (fixed system UUIDs, system catalog tables with UUIDs).

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### B-Tree Index Column IDs
[*] `SBBTreeIndex.idx_column_ids` uses `std::vector<ID>` (UUIDs).

Key references:
- `include/scratchbird/core/btree.h:147-154`

### System Schema UUID Behavior
[~] Root schema UUID aligns with database UUID; other base schemas are generated per database creation (not fixed constants).
[~] System catalog entries use 8 base schemas with bracketed names (`[root]`, `[sys]`, etc.), not the 18-schema tree used elsewhere in catalog bootstrap.

Key references:
- `src/core/database.cpp:915-947` (base schema entries, root UUID = DB UUID, others `generateUuidV7()`)
- `include/scratchbird/core/database.h:120-136` (SystemCatalogEntry structure)

### Fixed System UUIDs / system_uuids.h
[ ] Document claims “fixed system UUIDs” and “well-known constants,” but current tree appears to have removed `system_uuids.h` and uses generated UUIDs per database (except root).

Key references:
- Missing `include/scratchbird/core/system_uuids.h` (file not present)
- `src/core/database.cpp:925-936` (generated UUIDs for base schemas)

### System Catalog Tables with UUIDs
[ ] Document claims system catalog tables have UUID identifiers; current implementation still identifies system tables via page numbers and does not register system tables as catalog objects.

Key references:
- `include/scratchbird/core/database.h:100-120` (system catalog page stored in header)
- `src/core/catalog_manager.cpp` (system tables tracked by page IDs, not UUID catalog rows)

### Build Verification
[ ] Build/test success claims were not verified in this review.

## Notes
- This document contains internally conflicting statements (e.g., “fixed system UUIDs” vs. “fixed UUIDs removed and generated per database”).
- The base schema list in `database.cpp` differs from the 18-schema hierarchy established in `catalog_manager.cpp` bootstrap; this mismatch is not addressed in the spec.
