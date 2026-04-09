# DATABASE_TABLE_BOOTSTRAP_EVIDENCE

Ticket: `CAT-004`
Status: `Completed`

## Objective
Materialize canonical core identity table family entry `database` as a persisted catalog table with deterministic bootstrap behavior.

## Implemented Contract
1. `CatalogRootPage` stores `database_page` mapping.
2. `CatalogManager` tracks `database_table_page_` and exposes it via `databaseTablePage()`.
3. Bootstrap allocates and initializes `database` catalog heap page.
4. Bootstrap and load/backfill paths call `ensureDatabaseCatalogRecord()`.
5. Identity row fields:
- `database_id` = `db_->uuid()`
- `database_name` = filename-derived stable identifier (UTF-8 truncated to storage)
- `owner_id` = `SYSTEM` UUID at bootstrap/load
- `created_time`, `last_modified_time`, `is_valid`

## Verification Evidence
1. `tests/unit/test_catalog_database_bootstrap.cpp` validates page presence, row identity, owner binding, and no-duplication on reopen.
2. Catalog regression slice passed with no failures after change.

## Changed Code Paths
1. `include/scratchbird/core/catalog_manager.h`
2. `src/core/catalog_manager.cpp`
3. `tests/unit/test_catalog_database_bootstrap.cpp`
