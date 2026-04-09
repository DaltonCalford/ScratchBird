# Implementation Notes

Status: `Completed`
Ticket: `CAT-006`

## Work Performed
1. Added canonical `object_name` page state to `CatalogManager` and exposed `objectNameTablePage()` for test contract validation.
2. Extended `CatalogRootPage` contract with `object_name_page` and wired read/write persistence.
3. Added packed on-disk `ObjectNameRecord` contract for authoritative default-language name rows.
4. Updated catalog initialization and load/backfill paths to allocate and persist `object_name` table page.
5. Implemented `ensureObjectNameCatalogRecord(...)` with update-or-insert semantics:
- key: `(object_id, language_code)`
- preserves prior `name_id` and `created_time`
- stores `canonical_name_text` from resolver normalization.
6. Extended `syncObjectCatalogFromCaches(...)` to:
- materialize default-language `object_name` rows for database + resolver objects
- persist parent scope and schema path
- invalidate stale default-language name rows no longer expected.
7. Extended bootstrap unit contract to assert:
- `object_name` page exists
- exactly one default database name row exists
- reopen does not duplicate canonical database default name row.

## Code Artifacts
- `include/scratchbird/core/catalog_manager.h`
- `src/core/catalog_manager.cpp`
- `tests/unit/test_catalog_database_bootstrap.cpp`

## Notes
- Non-default language rows are left untouched by this ticket; CAT-006 scope enforces deterministic default-language canonical rows needed by baseline name resolution.
- Existing root-schema/database UUID sharing behavior remains guarded; schema identity normalization continues in `CAT-007`.
