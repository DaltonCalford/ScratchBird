# Targeted Sweep: Section 24 SBLR Alignment (2026-02-11)

## Scope
- Canonical docs only (excluded `legacy_imports/` and `source_copies/`).
- Focus area:
  - `24_Catalog_Model_and_Virtual_Overlays`
  - Cross-check with `23_SBLR_VM_Compiler_and_Executor`.

## Findings Addressed
1. Section 24 did not include statement-level normalization evidence persistence.
2. Section 24 inventory did not list `sb_sblr_statement_norm`.
3. Section 24 used `created_time/retired_time/last_used_time` while section 23 used `created_at/retired_at/last_used_at`.
4. Section 24 declared `cat_enum_artifact_state` and `cat_enum_queue_state` without corresponding enum-kind registry entries.
5. Section 23 cache key used stale `payload_schema_id` module-cache key shape.

## Corrections Applied
- Updated `CATALOG_TABLE_SCHEMA_SBLR_EXECUTION_ARTIFACTS.md`:
  - added `normalization_evidence_hash` and `statement_norm_count` to `sb_sblr_module`;
  - added `normalization_evidence_hash` to `sb_sblr_plan`;
  - added new table `sb_sblr_statement_norm`;
  - aligned time field names to `*_at`;
  - aligned `sb_sblr_plan_dependency` to `object_kind` + unique `(plan_uuid, object_uuid)`;
  - added explicit normalization persistence and invalidation rules.
- Updated `CATALOG_TABLE_INVENTORY.md`:
  - added `sb_sblr_statement_norm`;
  - clarified normalization coupling in module/plan rows.
- Updated `CATALOG_ENUMS.md`:
  - added enum `artifact_state`;
  - added enum `queue_state`.
- Updated `CATALOG_SYSTEM_DOMAINS.md`:
  - registered enum kinds `artifact_state` and `queue_state`;
  - added explicit mapping for normalization hash/checksum columns.
- Updated section 23 consistency:
  - `EXECUTION_CACHE_AND_INVALIDATION.md` module-cache key now uses `result_shape_id`, `capability_profile_version`, and `normalization_evidence_hash`;
  - `CATALOG_REQUIREMENTS_FOR_EXECUTION_ARTIFACTS.md` now includes `payload_schema_id` in `sb_sblr_module`.

## Residual Issues
- None found in the targeted scope.

