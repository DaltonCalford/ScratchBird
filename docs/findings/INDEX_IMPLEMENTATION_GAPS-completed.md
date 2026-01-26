# Index Implementation Gaps (Alpha)

## Scope
- Specs referenced:
  - `ScratchBird/docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`
  - `ScratchBird/docs/specifications/ddl/DDL_INDEXES.md`
- Code reviewed:
  - Index migration update path: `ScratchBird/src/core/catalog_manager.cpp`
  - Index cache deletion: `ScratchBird/src/sblr/index_cache.cpp`

## Findings

### F-IDX-001: Non-BTree/Hash index TID updates are not implemented (RESOLVED)
TID update paths for non-BTree/Hash indexes are now implemented in the migration
path.

- Vector/HNSW: `ScratchBird/src/core/catalog_manager.cpp:11833`
- Full-text: `ScratchBird/src/core/catalog_manager.cpp:11872`
- GIN: `ScratchBird/src/core/catalog_manager.cpp:11909`
- GiST: `ScratchBird/src/core/catalog_manager.cpp:11950`
- BRIN: `ScratchBird/src/core/catalog_manager.cpp:11987`
- R-tree: `ScratchBird/src/core/catalog_manager.cpp:12027`

**Status:** Resolved.

### F-IDX-002: GiST index cache deletion is intentionally skipped (memory leak) (RESOLVED)
GiST index instances are now deleted in the index cache cleanup path.

- `ScratchBird/src/sblr/index_cache.cpp:287`

**Status:** Resolved.

## Remediation Targets (Alpha)
- No remaining gaps for index migration TID updates or GiST cache cleanup.
