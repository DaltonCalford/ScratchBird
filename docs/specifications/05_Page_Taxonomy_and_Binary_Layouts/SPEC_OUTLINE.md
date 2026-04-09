# Section 05 Specification Outline

## Objective

Define the implementation-ready current durable page contract so low-level page creation, validation, publication, restart handling, and compatibility checks can be implemented without inferring page semantics from scattered source code.

## Active authority files

- `PAGE_HEADER_LAYOUT.md`
- `PAGE_TYPE_ENUMS.md`
- `HEAP_PAGE_LAYOUT.md`
- `INDEX_PAGE_BASE_LAYOUT.md`
- `CHECKSUM_AND_INTEGRITY.md`
- `RECOVERY_MARKERS_AND_REPAIRABLE_PAGE_FIELDS.md`
- `ON_DISK_FORMAT_INVENTORY_AND_VERSION_MANIFEST.md`
- `COMPRESSION_AND_ENCRYPTION.md`
- `EMULATION_STORAGE_PAGE_TYPES.md`
- `DEPENDENCIES.md`
- `TEST_CONTRACT.md`

## Scope

- fixed common page header
- durable page-type taxonomy
- family-local heap and shared index base layout
- checksum and integrity rules
- page-local repair and generation markers
- compression and encryption page-image boundary
- format compatibility inventory

## Non-goals

- full physical implementation closure for every reserved emulation page family
- universal compression or encryption parity across every page family
- WAL-style restart semantics
