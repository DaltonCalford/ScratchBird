# HOME_SEARCH_PATH_RUNTIME_BINDING_TESTS

Ticket: `CAT-009`
Gate: `CAT-GATE-02`
Status: `PASS`

## Normative Assertions
1. Home schema resolution follows strict precedence: user -> role -> group -> public.
2. Group tie-break is deterministic: lower `precedence` first, then lower `group_uuid` bytes.
3. Search path profile/entries persist and are reloaded into runtime session context.
4. Runtime receives ordered search path and current schema from catalog-derived session state.

## Evidence Mapping
- Precedence and tie-break: `tests/unit/test_home_schema_resolution.cpp`
- Persistent profile binding: `tests/unit/test_home_schema_resolution.cpp`
- Runtime session binding:
  - `src/server/server_session.cpp`
  - `src/sblr/executor.cpp`

## Executed Suite
- `build/tests/scratchbird_tests --gtest_filter='HomeSchemaResolutionTest.*:CatalogParentageAndNameUniquenessTest.*:CatalogDatabaseBootstrapTest.*'`

## Verdict
- `PASS` (10/10 tests in targeted gate set)
