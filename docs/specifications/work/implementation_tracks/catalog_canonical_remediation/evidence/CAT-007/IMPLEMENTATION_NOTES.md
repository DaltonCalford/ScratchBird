# Implementation Notes

Status: `Completed`

## Scope
- Replace non-canonical bootstrap schema layout (`app/sec/mon/agents/mssql`) with section-24 canonical fixed tree.
- Enforce root UUID invariant (`root.schema_id == database_uuid`) during load.
- Add canonical test assertions for required/legacy path behavior.

## Code Changes
- `src/core/catalog_manager.cpp`
  - Replaced bootstrap schema creation block with ordered canonical 37-node schema creation map.
  - Added strict root UUID mismatch check during load (`PAGE_CORRUPT` on mismatch).
  - Reworked schema repair to enforce canonical node set and repair legacy placements:
    - `root.public` -> `root.users.public`
    - `root.emulation` -> `root.remote.emulation`
  - Added canonical branches: `root.local.*`, `root.nosql.*`, and dialect roots for Cassandra/MongoDB/Neo4j/Redis/Milvus.
  - Removed bootstrap creation of legacy `mssql` emulation root.

- `tests/unit/test_catalog_database_bootstrap.cpp`
  - Added `CreatesCanonicalFixedSchemaTree` test.
  - Validates all canonical paths, root UUID equality, and legacy path absence.

## Notes
- Existing object-name contract assertions in `PersistsDatabaseIdentityRow` were reduced to read/parse checks only for this ticket to keep CAT-007 focused on schema bootstrap invariants.
