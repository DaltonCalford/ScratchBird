# Implementation Notes

Status: `Completed`

## Completed in this pass
- Added CAT-016 catalog root fields and persistence wiring:
  - `index_columns_page`
  - `index_opclass_page`
  - `index_opclass_fn_page`
  - `index_options_page`
  - `index_access_methods_page`
  - `index_maintenance_page`
  - `index_maintenance_deltas_page`
  - `index_build_deltas_page`
- Added bootstrap allocation and legacy backfill allocation for all CAT-016 families.
- Added on-disk record contracts in `CatalogManager`:
  - `IndexAccessMethodRecord`
  - `IndexOpclassRecord`
  - `IndexOpclassFunctionRecord`
  - `IndexColumnRecord`
  - `IndexOptionRecord`
  - `IndexMaintenanceRecord`
  - `IndexMaintenanceDeltaRecord`
  - `IndexBuildDeltaRecord`
- Added full CAT-016 CRUD/public APIs for:
  - `index_access_method`
  - `index_opclass`
  - `index_opclass_fn`
  - `index_column`
  - `index_option`
  - `index_maintenance`
  - `index_maintenance_delta`
  - `index_build_delta`
- Enforced deterministic constraints:
  - canonical index type name validation.
  - unique access method names.
  - unique opclass identity by `(name, index_type, owner_schema_uuid)`.
  - unique opclass function identity by `(opclass_uuid, fn_kind, support_number)`.
  - unique per-index column position and include-column ordering.
  - column/expression mutual exclusion for index key definitions.
  - unique option keys per index.
  - at-most-one active maintenance row per index.
  - unique maintenance/build delta identities by `(maintenance_uuid, delta_uuid)` and `(index_uuid, delta_uuid)`.
- Added CAT-016 bootstrap persistence and index metadata contract tests.
