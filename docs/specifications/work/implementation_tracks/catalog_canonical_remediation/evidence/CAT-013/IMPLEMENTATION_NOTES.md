# Implementation Notes

Status: `Completed`

## Completed in this pass
- Added CAT-013 catalog root fields and persistence wiring:
  - `resource_bundles_page`
  - `resource_artifacts_page`
  - `timezone_transitions_page`
  - `timezone_leap_seconds_page`
- Added bootstrap allocation and legacy backfill allocation for all four CAT-013 tables.
- Added on-disk record contracts in `CatalogManager`:
  - `ResourceBundleRecord`
  - `ResourceArtifactRecord`
  - `TimezoneTransitionRecord`
  - `TimezoneLeapSecondRecord`
- Added full CAT-013 CRUD/public APIs in `CatalogManager` for:
  - `resource_bundle`
  - `resource_artifact`
  - `timezone_transition`
  - `timezone_leap_second`
- Enforced deterministic contracts:
  - `resource_bundle`:
    - required `bundle_kind`, `bundle_name`, `bundle_version`, `content_hash`.
    - unique `(bundle_kind, bundle_name, bundle_version, content_hash)`.
    - at most one active bundle per `bundle_kind`.
  - `resource_artifact`:
    - required `bundle_uuid`, `artifact_kind`, `artifact_path`, `content_blob`, `content_hash`.
    - `content_size_bytes` must match payload size.
    - unique `(bundle_uuid, artifact_path)`.
  - `timezone_transition`:
    - required `timezone_uuid`, `bundle_uuid`, `abbr`.
    - FK-like checks for referenced timezone and bundle.
    - unique `(timezone_uuid, sequence_no)`.
    - unique `(timezone_uuid, effective_utc_epoch, utc_offset_seconds, is_dst, abbr)`.
  - `timezone_leap_second`:
    - required `bundle_uuid`.
    - unique `(bundle_uuid, effective_utc_epoch)`.
- Added CAT-013 bootstrap persistence gate and CRUD contract tests.
