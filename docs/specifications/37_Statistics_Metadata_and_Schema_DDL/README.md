# Section 37 Statistics Metadata and Schema DDL

Status: current_authority_with_reconstructed_expansion
Current implementation state: partial for broader statistics breadth, but
transactional DDL publication and the bounded Beta 1 online-schema-change
surface are implemented with explicit fail-closed boundaries around
rewrite-required or unsupported operations.

This section owns the audit-facing subject that combines statistics, metadata ownership, schema lifecycle, DDL visibility, invalidation, and concurrent DDL/DML boundaries.

## Section scope

- statistics collection and freshness
- system catalog and metadata ownership
- metadata cache and visibility boundary
- schema DDL state machine
- dependency invalidation and concurrent DDL/DML
- non-guarantee and partial schema behavior

## Primary audit lookup anchors

- `src/core/connection_context.cpp` search `appendSchemaEpochCatalogEntry(` for
  commit-bound schema publication and transactional DDL lineage emission.
- `include/scratchbird/core/catalog_manager.h` search
  `SchemaEpochCatalogInfo` for the current durable schema-epoch row-family
  payload.
- `include/scratchbird/core/catalog_manager.h` search
  `appendSchemaEpochCatalogEntry(` for the durable schema-epoch catalog API
  owned by the catalog layer.
- `include/scratchbird/core/catalog_manager.h` search
  `appendSchemaChangePlanCatalogEntry(` for the durable schema-change plan API
  owned by the catalog layer.
- `src/core/connection_context.cpp` search
  `Failed to persist transactional DDL lineage/schema epoch` for the fail-closed
  runtime boundary when epoch publication cannot be durably recorded.
- `src/sblr/executor.cpp` search `classifySchemaChangeClassForSql(` for the
  fail-closed online-schema-change classifier used by the live ALTER TABLE
  execution paths.

<!-- AUTO-GENERATED:FILE-LIST:START -->
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [DEPENDENCY_INVALIDATION_AND_CONCURRENT_DDL_DML.md](DEPENDENCY_INVALIDATION_AND_CONCURRENT_DDL_DML.md)
- [INDEX_FAMILY_METRICS_PUBLICATION_FRESHNESS_AND_INVALIDATION_MODEL.md](INDEX_FAMILY_METRICS_PUBLICATION_FRESHNESS_AND_INVALIDATION_MODEL.md)
- [METADATA_CACHE_AND_VISIBILITY_BOUNDARY.md](METADATA_CACHE_AND_VISIBILITY_BOUNDARY.md)
- [NON_GUARANTEE_AND_PARTIAL_SCHEMA_BEHAVIOR.md](NON_GUARANTEE_AND_PARTIAL_SCHEMA_BEHAVIOR.md)
- [ONLINE_SCHEMA_CHANGE_AND_BACKFILL_MODEL.md](ONLINE_SCHEMA_CHANGE_AND_BACKFILL_MODEL.md)
- [ROW_UUID_ALIAS_COLUMN_BINDING_AND_SCHEMA_DDL_MODEL.md](ROW_UUID_ALIAS_COLUMN_BINDING_AND_SCHEMA_DDL_MODEL.md)
- [SCHEMA_DDL_STATE_MACHINE.md](SCHEMA_DDL_STATE_MACHINE.md)
- `SECTION_CLOSURE_MATRIX.csv`
- [SECURITY_POLICY_EPOCH_AND_PERMISSION_CACHE_MODEL.md](SECURITY_POLICY_EPOCH_AND_PERMISSION_CACHE_MODEL.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [STATISTICS_COLLECTION_AND_FRESHNESS.md](STATISTICS_COLLECTION_AND_FRESHNESS.md)
- [SYSTEM_CATALOG_AND_METADATA_OWNERSHIP.md](SYSTEM_CATALOG_AND_METADATA_OWNERSHIP.md)
- [SYSTEM_ROW_UUID_CLUSTER_TRACKING_AND_USER_UUID_ALIASING_MODEL.md](SYSTEM_ROW_UUID_CLUSTER_TRACKING_AND_USER_UUID_ALIASING_MODEL.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
