# Implementation Notes

Status: `Completed`

Date: `2026-02-14`

Implemented:
- Added CAT-031 text-search catalog contracts in `CatalogManager` API surface:
  - `ts_parser`
  - `ts_template`
  - `ts_dictionary`
  - `ts_config`
  - `ts_config_map`
- Added root slot fields, private page members, and accessor methods for all CAT-031 pages.
- Added physical record structs and deterministic CRUD implementations with validation/constraints:
  - unique name constraints for parser/template/dictionary/config
  - `ts_config_map` unique `(config_uuid, token_type)`
  - referential checks (`template`, `parser`, `dictionary`, `config`)
  - TOAST-backed payload handling for dictionary init options and ordered dictionary UUID lists.
- Added bootstrap allocation and load-time backfill mapping for all CAT-031 table pages.
- Added bootstrap page contract test in `tests/unit/test_catalog_database_bootstrap.cpp`.
- Added end-to-end CAT-031 contract test in `tests/unit/test_catalog_text_search_extension_contract.cpp`.
