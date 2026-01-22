# Index Implementation Gaps (Alpha)

## Scope
- Specs referenced:
  - `ScratchBird/docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`
  - `ScratchBird/docs/specifications/ddl/DDL_INDEXES.md`
- Code reviewed:
  - Index migration update path: `ScratchBird/src/core/catalog_manager.cpp`
  - Index cache deletion: `ScratchBird/src/sblr/index_cache.cpp`

## Findings

### F-IDX-001: Non-BTree/Hash index TID updates are not implemented
During table/tablespace migration, several index types log NOT YET IMPLEMENTED
and instruct users to drop/recreate the index. This blocks "full" index support
in Alpha for these types.

- Vector/HNSW: `ScratchBird/src/core/catalog_manager.cpp:11128`
- Full-text: `ScratchBird/src/core/catalog_manager.cpp:11145`
- GIN: `ScratchBird/src/core/catalog_manager.cpp:11162`
- GiST: `ScratchBird/src/core/catalog_manager.cpp:11179`
- BRIN: `ScratchBird/src/core/catalog_manager.cpp:11197`
- R-tree: `ScratchBird/src/core/catalog_manager.cpp:11215`

**Impact:** These index types become invalid after migration and require manual
rebuilds. This conflicts with Alpha’s "fully implemented core" requirement.

### F-IDX-002: GiST index cache deletion is intentionally skipped (memory leak)
GiST index instances are not deleted due to incomplete type integration, leading
to a deliberate leak in the index cache cleanup path.

- `ScratchBird/src/sblr/index_cache.cpp:286`

**Impact:** Long-running sessions can accumulate leaked GiST index objects.

## Remediation Targets (Alpha)
- Implement TID update paths for Vector/HNSW, Full-text, GIN, GiST, BRIN, and R-tree.
- Complete GiST type integration so index cache cleanup can safely delete objects.
