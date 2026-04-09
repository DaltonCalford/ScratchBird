# Implementation Notes

Status: `Completed`

## Completed in this pass
- Added CAT-012 catalog root fields and persistence wiring:
  - `charset_aliases_page`
  - `collation_tailoring_page`
- Added bootstrap allocation and legacy backfill allocation for both CAT-012 tables.
- Added on-disk record contracts in `CatalogManager`:
  - `CharsetAliasRecord`
  - `CollationTailoringRecord`
- Added full CAT-012 CRUD/public APIs in `CatalogManager` for:
  - `charset_alias`
  - `collation_tailoring`
- Enforced deterministic contracts:
  - `charset_alias`:
    - `charset_uuid` required and must resolve to existing charset.
    - `bundle_uuid` required.
    - `alias_name` required.
    - `normalized_name` canonicalized and unique globally.
  - `collation_tailoring`:
    - `collation_id` required and must resolve.
    - `bundle_uuid` required.
    - valid `tailoring_kind` enum required.
    - at least one of `tailoring_json` or `tailoring_blob` required.
    - unique `(collation_id, tailoring_kind, tailoring_hash)`.
- Added CAT-012 bootstrap persistence gate and CRUD contract tests.
