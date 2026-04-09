# Section 05 - Page Taxonomy and Binary Layouts

Status: current_authority

## Current authority

Section `05` is the canonical current-state authority for:
- common durable page-header fields and legality rules
- durable page-type taxonomy and reserved page-family assignments
- heap page base layout
- shared index-page base layout
- checksum, repair-marker, and restart-visible page metadata
- on-disk format inventory and compatibility boundary
- compression and encryption boundary at the page-image level

## Explicit boundaries

Section `05` does not currently claim:
- that every reserved emulation page type has a full writer, reader, repair, and maintenance implementation
- that every page family supports compression or encryption symmetrically
- a single universal compression header embedded directly in every page header

## Governing invariants

- `ondisk.h` is the common durable page contract authority.
- Page-local markers are not transaction truth and do not replace MGA semantics.
- Stored-page integrity is validated over the stored page image.
- Illegal header, generation, checksum, or repair-state combinations fail closed.
- Page-family reservation does not imply implementation completeness.

## Primary audit lookup anchors
- `include/scratchbird/core/ondisk.h` search `PAGE_TYPE_DATABASE_HEADER` for
  the canonical page-type enum root.
- `include/scratchbird/core/ondisk.h` search
  `PageHeader must publish the canonical section-05 field set` for the common
  page-header size and field invariant.
- `include/scratchbird/core/ondisk.h` search `BootstrapSystemStatePage` for the
  section `05` to section `06` bootstrap boundary.

## File Index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [CHECKSUM_AND_INTEGRITY.md](CHECKSUM_AND_INTEGRITY.md)
- [COMPRESSION_AND_ENCRYPTION.md](COMPRESSION_AND_ENCRYPTION.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [EMULATION_STORAGE_PAGE_TYPES.md](EMULATION_STORAGE_PAGE_TYPES.md)
- [HEAP_PAGE_LAYOUT.md](HEAP_PAGE_LAYOUT.md)
- [INDEX_PAGE_BASE_LAYOUT.md](INDEX_PAGE_BASE_LAYOUT.md)
- [ON_DISK_FORMAT_INVENTORY_AND_VERSION_MANIFEST.md](ON_DISK_FORMAT_INVENTORY_AND_VERSION_MANIFEST.md)
- [PAGE_HEADER_LAYOUT.md](PAGE_HEADER_LAYOUT.md)
- [PAGE_TYPE_ENUMS.md](PAGE_TYPE_ENUMS.md)
- [RECOVERY_MARKERS_AND_REPAIRABLE_PAGE_FIELDS.md](RECOVERY_MARKERS_AND_REPAIRABLE_PAGE_FIELDS.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
