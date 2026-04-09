# B1-02-004 Evidence Note

## Closure summary

Metadata DDL schema evolution and online change closure for package `02` is complete.

This ticket:
- added durable `schema_change_plan`, `schema_change_event`,
  `schema_change_backfill_progress`, and `schema_change_cutover_guard` catalog
  families
- extended transactional DDL commit handling so committed schema mutation
  emits schema-change plan and event records together with schema epochs
- classified bounded Beta 1 schema changes across both legacy and V3 ALTER
  TABLE paths
- treated validated nullable-to-not-null promotion as
  `EXPAND_BACKFILL_CUTOVER` with durable validation and cutover evidence
- admitted bounded metadata-only `ALTER COLUMN TYPE` widening while refusing
  `DROP COLUMN` and incompatible rewrite-requiring type changes as
  `REWRITE_REQUIRED` operations in the Beta 1 model

## Canonical files updated

- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/METADATA_AND_STATISTICS_SUBSYSTEM_CONTRACT.md`
- `docs/specifications/37_Statistics_Metadata_and_Schema_DDL/README.md`
- `docs/specifications/37_Statistics_Metadata_and_Schema_DDL/ONLINE_SCHEMA_CHANGE_AND_BACKFILL_MODEL.md`
- `docs/specifications/37_Statistics_Metadata_and_Schema_DDL/TEST_CONTRACT.md`
- `docs/work-plans/02-Catalog_UUID_Metadata_DDL_Schema/README.md`
- `docs/work-plans/02-Catalog_UUID_Metadata_DDL_Schema/MASTER_TRACKER.md`
- `docs/work-plans/02-Catalog_UUID_Metadata_DDL_Schema/MASTER_TRACKER.csv`
- `docs/work-plans/02-Catalog_UUID_Metadata_DDL_Schema/ORDERED_TASK_TICKETS.csv`
- `docs/work-plans/02-Catalog_UUID_Metadata_DDL_Schema/BOUNDED_TICKET_SET.md`
- `docs/work-plans/02-Catalog_UUID_Metadata_DDL_Schema/CANONICAL_GAP_REGISTER.md`
- `docs/work-plans/02-Catalog_UUID_Metadata_DDL_Schema/RISK_DECISION_LOG.md`
- `docs/work-plans/02-Catalog_UUID_Metadata_DDL_Schema/SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`

## Verification

- preserved lane-B proof log: `evidence/B1-02-004/lane_b_schema_change.log`
- preserved dedicated column-mod proof log:
  `evidence/B1-02-004/alter_table_column_mods.log`
- verified command filter:
  `ForensicReplaySessionTest.ReplayResolvesHistoricalSchemaAcrossCommittedDdl:ForensicReplaySessionTest.SchemaChangeCatalogTracksMetadataOnlyAndValidatedPromotion:AlterTableColumnModsTest.SetNotNullRejectsExistingNulls:AlterTableColumnModsTest.SetAndDropDefault:AlterTableColumnModsTest.AlterColumnPositionAffectsSelectStarOrder:AlterTableColumnModsTest.CompatibleWideningChangesSucceedWhileRewriteRequiredPathsFailClosed`
- result in the current `scratchbird_tests` binary: `2` compiled `ForensicReplaySessionTest` cases passed on `2026-03-30`
- result in the dedicated `test_alter_table_column_mods` executable: `4`
  `AlterTableColumnModsTest` cases passed on `2026-03-30`

## Result

- package `02` no longer has a lane-B ambiguity around transactional DDL
  publication, durable schema-change state, or Beta 1 online-schema-change
  classification
