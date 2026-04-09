# Implementation Notes

Status: `Completed`

## Scope
- Implement persistent catalogs for home schema bindings and search-path profiles/entries.
- Bind resolved home schema + search path into runtime session context.
- Enforce deterministic resolution order and tie-break behavior per canonical spec.

## Code Changes
- `include/scratchbird/core/catalog_manager.h`
  - Extended `SessionInfo` with:
    - `home_schema_id`
    - `search_path_profile_id`
    - `search_path_schema_ids`
    - `search_path`
  - Added CAT-009 helper declarations:
    - `ensureHomeSearchPathCatalogTables`
    - `resolveSessionHomeSchema`
    - `resolveSessionSearchPath`
  - Added table-page IDs for:
    - `home_schema_binding`
    - `search_path_profile`
    - `search_path_entry`

- `src/core/catalog_manager.cpp`
  - Extended `CatalogRootPage` with persistent page IDs for CAT-009 catalogs.
  - Added record structs for binding/profile/entry rows.
  - Implemented deterministic home schema resolver and search-path resolver.
  - Updated `createSession` to persist and return resolved home schema + search-path metadata.
  - Updated `getSession` / session record loading to rehydrate search-path state deterministically.

- `src/server/server_session.cpp`
  - Session auth flow now applies `SessionInfo.search_path` to connection context.
  - Current schema name is set from resolved schema UUID.

- `src/sblr/executor.cpp`
  - Session setup now uses catalog-resolved ordered search path.
  - Role-schema override prepends deterministically when enabled.

- `tests/unit/test_home_schema_resolution.cpp`
  - Added CAT-009 coverage for:
    - role-over-group precedence
    - group tie-break by UUID
    - persisted search-path profile usage

## Notes
- Build warning-only findings observed in unrelated files (`job_scheduler.cpp`, OpenSSL `MD5` deprecation, test warning attributes).
- No CAT-009 functional failures observed in targeted catalog/home-schema gate tests.
