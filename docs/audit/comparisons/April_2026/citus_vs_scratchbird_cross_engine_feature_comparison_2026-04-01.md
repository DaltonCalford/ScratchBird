# Citus vs ScratchBird: Cross-Engine Feature Comparison

Report ID: `april-2026-citus-vs-scratchbird`

- report_date: `2026-04-01`
- report_family_version: `april_2026_v6`
- comparison_intent: `EMULATION_FEASIBILITY`
- assessment_lanes: `CURRENT_STATE`, `SPECIFIED_TARGET_STATE`
- scope_profile: `full feature-to-feature comparison of donor surface against ScratchBird database-environment surface; includes engine core, parser boundary, catalog overlays, donor front door, and cluster/lineage behavior; current-state plus specified-target-state lanes`
- subject_a.engine_key: `citus`
- subject_a.display_name: `Citus`
- subject_a.version_or_release: `formal reference pack capture as of 2026-04-01`
- subject_a.edition_or_variant: `reference donor baseline`
- subject_a.deployment_mode: `donor-defined deployment surface`
- subject_b.engine_key: `scratchbird`
- subject_b.display_name: `ScratchBird`
- subject_b.version_or_release: `canonical spec tree and current code evidence as of 2026-04-01`
- subject_b.edition_or_variant: `database environment with engine, parser families, overlays, and cluster substrate`
- subject_b.deployment_mode: `embedded-first environment with listener and parser front doors`
- scratchbird_role: `IMPLEMENTATION_CANDIDATE`
- evidence_cutoff_date: `2026-04-01`
- generator_or_author: `Codex`
- excluded_domains: `Benchmark-only claims, byte-for-byte on-disk parity, ecosystem-only tooling, and unproven full upstream-harness closure remain excluded unless a row cites bounded evidence directly.`
- source_pack: `docs/specifications/Reference_Documentation_specification.md + docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus + ScratchBird canonical specs + compatibility evidence where applicable`

## 1. Purpose and Scope

This report compares `Citus` against the full ScratchBird platform surface rather than against a flattened generic SQL-engine profile. The comparison keeps environment-level ScratchBird features visible, including parser separation, virtual catalog overlays, SBLR, UUID lineage, and cluster-facing identity or retention behavior.

## 2. Executive Summary

- current_state_verdict: `NOT_CURRENTLY_FEASIBLE`
- specified_target_state_verdict: `PARTIAL_WITH_GAPS`
- feature_matrix_rows: `48`
- current_blocker_feature_keys: `catalog.donor_catalog_overlay_and_system_table_emulation, catalog.system_views_and_statistics_surface, types.donor_type_mapping_and_emulation_surface, ddl.donor_schema_mapping_and_search_path_behavior, dml.donor_query_semantics_emulation_surface, planner.path_taxonomy_plan_control_and_explain_surface, protocol.dedicated_donor_front_door, protocol.prepared_parameter_error_and_result_mapping, data_movement.cdc_migration_replay_and_change_capture, replication.sharding_partitioning_and_topology_control`
- target_blocker_feature_keys: `replication.apply_lag_failover_and_observability`
- current_mapping_feature_keys: `architecture.storage_engine_toast_and_oversized_value_model, architecture.cluster_ready_identity_and_retained_replication_model, catalog.uuid_identity_and_row_lineage_keys, catalog.metadata_publication_and_schema_epoch_rules, types.native_scalar_and_temporal_surface, types.complex_type_and_specialized_value_surface, types.custom_domains_and_user_defined_type_control_plane, ddl.object_lifecycle_and_dependency_publication, dml.query_and_mutation_surface, indexes.runtime_family_surface`
- target_mapping_feature_keys: `architecture.storage_engine_toast_and_oversized_value_model, architecture.cluster_ready_identity_and_retained_replication_model, catalog.uuid_identity_and_row_lineage_keys, catalog.metadata_publication_and_schema_epoch_rules, types.native_scalar_and_temporal_surface, types.complex_type_and_specialized_value_surface, types.custom_domains_and_user_defined_type_control_plane, ddl.object_lifecycle_and_dependency_publication, dml.query_and_mutation_surface, planner.path_taxonomy_plan_control_and_explain_surface`
- donor_role: `Distributed Postgres donor`

## 3. Subject Identity and Scope Assumptions

- donor_rank_and_phase: `rank 39`, `Beta 2`
- why_included: Best donor for coordinator/worker planning, reference tables, tenant sharding
- authoritative_topics: Colocated joins, rebalance, shard movement, coordinator/worker split
- non_authoritative_topics: Coordinator bottlenecks or tenant sharding as universal strategy
- ScratchBird interpretation rule: compare ScratchBird as a database environment with a separate parser/front-door layer, canonical catalogs, SBLR execution, and cluster-aware lineage semantics.
- Donor-tool evidence rule: direct client or upstream harness closure is cited only where the repo contains bounded execution evidence; it is not inferred from parser presence alone.

## 4. Overall Category Scorecard

### CURRENT_STATE

| Metric | Count |
| --- | ---: |
| in-scope feature rows | 48 |
| IDENTICAL | 0 |
| FUNCTIONALLY_EQUIVALENT | 0 |
| COMPATIBLE_WITH_MAPPING | 21 |
| EMULATABLE_WITHOUT_MATERIAL_LOSS | 0 |
| PARTIAL_PARITY | 12 |
| directional-only rows | 15 |
| blocker rows | 11 |
| roadmap-only rows | 0 |

### SPECIFIED_TARGET_STATE

| Metric | Count |
| --- | ---: |
| in-scope feature rows | 48 |
| IDENTICAL | 0 |
| FUNCTIONALLY_EQUIVALENT | 0 |
| COMPATIBLE_WITH_MAPPING | 24 |
| EMULATABLE_WITHOUT_MATERIAL_LOSS | 7 |
| PARTIAL_PARITY | 1 |
| directional-only rows | 16 |
| blocker rows | 1 |
| roadmap-only rows | 0 |

## 5. Architecture, Storage, and Transaction Core

### `architecture.product_model_and_execution_boundary` Product model and execution boundary

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `architecture.product_model_and_execution_boundary` | `architecture.product_model_and_execution_boundary` |
| Feature Name | Product model and execution boundary | Product model and execution boundary |
| Citus State | `DATABASE_ENGINE` | `DATABASE_ENGINE` |
| ScratchBird State | `DATABASE_ENVIRONMENT` | `DATABASE_ENVIRONMENT` |
| Outcome | `NOT_COMPARABLE` | `NOT_COMPARABLE` |
| Delta Class | `ARCHITECTURALLY_DIFFERENT` | `ARCHITECTURALLY_DIFFERENT` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare a donor database product against ScratchBird's database-environment model rather than flattening both subjects into one generic engine label. Donor baseline is `DATABASE_ENGINE`. ScratchBird current lane is `DATABASE_ENVIRONMENT`. | Compare a donor database product against ScratchBird's database-environment model rather than flattening both subjects into one generic engine label. Donor baseline is `DATABASE_ENGINE`. ScratchBird specified target lane is `DATABASE_ENVIRONMENT`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/29_Listener_and_Server_Orchestration/PARSER_AGENT_EXECUTABLE_COMPOSITION_AND_RUNTIME_STACK_MODEL.md`<br>`docs/specifications/28_Parser_Implementations/README.md`<br>`docs/specifications/25_Runtime_Modes/EMBEDDED_DIRECT_ENGINE_AND_LOCAL_SHARED_SERVER_RUNTIME_SELECTION_MODEL.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/29_Listener_and_Server_Orchestration/PARSER_AGENT_EXECUTABLE_COMPOSITION_AND_RUNTIME_STACK_MODEL.md`<br>`docs/specifications/28_Parser_Implementations/README.md`<br>`docs/specifications/25_Runtime_Modes/EMBEDDED_DIRECT_ENGINE_AND_LOCAL_SHARED_SERVER_RUNTIME_SELECTION_MODEL.md` |

### `architecture.storage_engine_toast_and_oversized_value_model` Storage engine, TOAST, and oversized-value model

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `architecture.storage_engine_toast_and_oversized_value_model` | `architecture.storage_engine_toast_and_oversized_value_model` |
| Feature Name | Storage engine, TOAST, and oversized-value model | Storage engine, TOAST, and oversized-value model |
| Citus State | `DONOR_LARGE_VALUE_SURFACE` | `DONOR_LARGE_VALUE_SURFACE` |
| ScratchBird State | `TOAST_FIRST_NATIVE` | `TOAST_FIRST_NATIVE` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare large-value storage, off-row or chunked storage, and ScratchBird's TOAST-first storage contract. Donor baseline is `DONOR_LARGE_VALUE_SURFACE`. ScratchBird current lane is `TOAST_FIRST_NATIVE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare large-value storage, off-row or chunked storage, and ScratchBird's TOAST-first storage contract. Donor baseline is `DONOR_LARGE_VALUE_SURFACE`. ScratchBird specified target lane is `TOAST_FIRST_NATIVE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/11_TOAST_and_LOB_Storage/README.md`<br>`docs/specifications/11_TOAST_and_LOB_Storage/LOB_PAGE_LAYOUTS.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/11_TOAST_and_LOB_Storage/README.md`<br>`docs/specifications/11_TOAST_and_LOB_Storage/LOB_PAGE_LAYOUTS.md` |

### `architecture.transaction_context_lifecycle` Transaction-context lifecycle

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `architecture.transaction_context_lifecycle` | `architecture.transaction_context_lifecycle` |
| Feature Name | Transaction-context lifecycle | Transaction-context lifecycle |
| Citus State | `EXPLICIT_OR_AUTOCOMMIT_TRANSACTION_MODEL` | `EXPLICIT_OR_AUTOCOMMIT_TRANSACTION_MODEL` |
| ScratchBird State | `ALWAYS_IN_TRANSACTION` | `ALWAYS_IN_TRANSACTION` |
| Outcome | `NOT_COMPARABLE` | `NOT_COMPARABLE` |
| Delta Class | `ARCHITECTURALLY_DIFFERENT` | `ARCHITECTURALLY_DIFFERENT` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare whether the platform is always in a transaction and how commit or rollback transitions the next execution boundary. Donor baseline is `EXPLICIT_OR_AUTOCOMMIT_TRANSACTION_MODEL`. ScratchBird current lane is `ALWAYS_IN_TRANSACTION`. | Compare whether the platform is always in a transaction and how commit or rollback transitions the next execution boundary. Donor baseline is `EXPLICIT_OR_AUTOCOMMIT_TRANSACTION_MODEL`. ScratchBird specified target lane is `ALWAYS_IN_TRANSACTION`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/29_Listener_and_Server_Orchestration/CONNECTION_AND_SESSION_LIFECYCLE.md`<br>`docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/29_Listener_and_Server_Orchestration/CONNECTION_AND_SESSION_LIFECYCLE.md`<br>`docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md` |

### `architecture.non_destructive_version_lineage` Non-destructive version lineage

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `architecture.non_destructive_version_lineage` | `architecture.non_destructive_version_lineage` |
| Feature Name | Non-destructive version lineage | Non-destructive version lineage |
| Citus State | `MVCC_OR_UNDO_VARIANT` | `MVCC_OR_UNDO_VARIANT` |
| ScratchBird State | `MGA_BACK_VERSIONED` | `MGA_BACK_VERSIONED` |
| Outcome | `NOT_COMPARABLE` | `NOT_COMPARABLE` |
| Delta Class | `ARCHITECTURALLY_DIFFERENT` | `ARCHITECTURALLY_DIFFERENT` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare update or delete lineage semantics, retained back versions, and whether mutation is destructive or version-preserving. Donor baseline is `MVCC_OR_UNDO_VARIANT`. ScratchBird current lane is `MGA_BACK_VERSIONED`. | Compare update or delete lineage semantics, retained back versions, and whether mutation is destructive or version-preserving. Donor baseline is `MVCC_OR_UNDO_VARIANT`. ScratchBird specified target lane is `MGA_BACK_VERSIONED`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/LOGICAL_ROW_UUID_CLUSTER_TRACKING_AND_VERSION_LINEAGE_OBSERVABILITY_MODEL.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/LOGICAL_ROW_UUID_CLUSTER_TRACKING_AND_VERSION_LINEAGE_OBSERVABILITY_MODEL.md` |

### `architecture.recovery_truth_and_derivative_lanes` Recovery truth and derivative lanes

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `architecture.recovery_truth_and_derivative_lanes` | `architecture.recovery_truth_and_derivative_lanes` |
| Feature Name | Recovery truth and derivative lanes | Recovery truth and derivative lanes |
| Citus State | `LOG_OR_WAL_PRIMARY_RECOVERY_MODEL` | `LOG_OR_WAL_PRIMARY_RECOVERY_MODEL` |
| ScratchBird State | `MGA_PRIMARY_DERIVATIVE_SECONDARY` | `MGA_PRIMARY_DERIVATIVE_SECONDARY` |
| Outcome | `NOT_COMPARABLE` | `NOT_COMPARABLE` |
| Delta Class | `ARCHITECTURALLY_DIFFERENT` | `ARCHITECTURALLY_DIFFERENT` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare whether recovery truth comes from durable MGA state or from log-centric replay and how derivative lanes are treated. Donor baseline is `LOG_OR_WAL_PRIMARY_RECOVERY_MODEL`. ScratchBird current lane is `MGA_PRIMARY_DERIVATIVE_SECONDARY`. | Compare whether recovery truth comes from durable MGA state or from log-centric replay and how derivative lanes are treated. Donor baseline is `LOG_OR_WAL_PRIMARY_RECOVERY_MODEL`. ScratchBird specified target lane is `MGA_PRIMARY_DERIVATIVE_SECONDARY`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/MGA_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/MGA_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md` |

### `architecture.cluster_ready_identity_and_retained_replication_model` Cluster-ready identity and retained replication model

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `architecture.cluster_ready_identity_and_retained_replication_model` | `architecture.cluster_ready_identity_and_retained_replication_model` |
| Feature Name | Cluster-ready identity and retained replication model | Cluster-ready identity and retained replication model |
| Citus State | `DISTRIBUTED_OR_REPLICATED_PLATFORM_MODEL` | `DISTRIBUTED_OR_REPLICATED_PLATFORM_MODEL` |
| ScratchBird State | `UUID_CLUSTER_SUBSTRATE_WITH_RETAINED_LINEAGE` | `UUID_CLUSTER_SUBSTRATE_WITH_RETAINED_LINEAGE` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare whether identity, retained lineage, and replication-friendly record history are part of the core platform model. Donor baseline is `DISTRIBUTED_OR_REPLICATED_PLATFORM_MODEL`. ScratchBird current lane is `UUID_CLUSTER_SUBSTRATE_WITH_RETAINED_LINEAGE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare whether identity, retained lineage, and replication-friendly record history are part of the core platform model. Donor baseline is `DISTRIBUTED_OR_REPLICATED_PLATFORM_MODEL`. ScratchBird specified target lane is `UUID_CLUSTER_SUBSTRATE_WITH_RETAINED_LINEAGE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/UUID_IDENTITY_AND_COLLISION_RULES.md`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_IDENTITY_SECURITY_QUORUM_AND_WRITE_FENCE_OBSERVABILITY_MODEL.md`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_ROUTING_FENCING_AND_SHARD_COMMIT_LOG_OBSERVABILITY.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/UUID_IDENTITY_AND_COLLISION_RULES.md`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_IDENTITY_SECURITY_QUORUM_AND_WRITE_FENCE_OBSERVABILITY_MODEL.md`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_ROUTING_FENCING_AND_SHARD_COMMIT_LOG_OBSERVABILITY.md` |

## 6. Catalog, Metadata, and Object Identity

### `catalog.uuid_identity_and_row_lineage_keys` UUID identity and row-lineage keys

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `catalog.uuid_identity_and_row_lineage_keys` | `catalog.uuid_identity_and_row_lineage_keys` |
| Feature Name | UUID identity and row-lineage keys | UUID identity and row-lineage keys |
| Citus State | `ENGINE_OBJECT_IDENTITY_MODEL` | `ENGINE_OBJECT_IDENTITY_MODEL` |
| ScratchBird State | `UUIDV7_DATABASE_OBJECT_AND_ROW_IDENTITY` | `UUIDV7_DATABASE_OBJECT_AND_ROW_IDENTITY` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare durable database, object, and row identity rules and how they participate in lineage and cluster safety. Donor baseline is `ENGINE_OBJECT_IDENTITY_MODEL`. ScratchBird current lane is `UUIDV7_DATABASE_OBJECT_AND_ROW_IDENTITY`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare durable database, object, and row identity rules and how they participate in lineage and cluster safety. Donor baseline is `ENGINE_OBJECT_IDENTITY_MODEL`. ScratchBird specified target lane is `UUIDV7_DATABASE_OBJECT_AND_ROW_IDENTITY`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/UUID_IDENTITY_AND_COLLISION_RULES.md`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/LOGICAL_ROW_UUID_CLUSTER_TRACKING_AND_VERSION_LINEAGE_OBSERVABILITY_MODEL.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/UUID_IDENTITY_AND_COLLISION_RULES.md`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/LOGICAL_ROW_UUID_CLUSTER_TRACKING_AND_VERSION_LINEAGE_OBSERVABILITY_MODEL.md` |

### `catalog.recursive_schema_tree_and_schema_root_sandboxing` Recursive schema tree and schema-root sandboxing

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `catalog.recursive_schema_tree_and_schema_root_sandboxing` | `catalog.recursive_schema_tree_and_schema_root_sandboxing` |
| Feature Name | Recursive schema tree and schema-root sandboxing | Recursive schema tree and schema-root sandboxing |
| Citus State | `FLAT_SCHEMA_OR_DATABASE_NAMESPACE_MODEL` | `FLAT_SCHEMA_OR_DATABASE_NAMESPACE_MODEL` |
| ScratchBird State | `RECURSIVE_SCHEMA_TREE_WITH_SANDBOX_ROOTS` | `RECURSIVE_SCHEMA_TREE_WITH_SANDBOX_ROOTS` |
| Outcome | `NOT_COMPARABLE` | `NOT_COMPARABLE` |
| Delta Class | `ARCHITECTURALLY_DIFFERENT` | `ARCHITECTURALLY_DIFFERENT` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare namespace structure, schema-root scoping, and whether sessions can be sandboxed into a recursive schema tree. Donor baseline is `FLAT_SCHEMA_OR_DATABASE_NAMESPACE_MODEL`. ScratchBird current lane is `RECURSIVE_SCHEMA_TREE_WITH_SANDBOX_ROOTS`. | Compare namespace structure, schema-root scoping, and whether sessions can be sandboxed into a recursive schema tree. Donor baseline is `FLAT_SCHEMA_OR_DATABASE_NAMESPACE_MODEL`. ScratchBird specified target lane is `RECURSIVE_SCHEMA_TREE_WITH_SANDBOX_ROOTS`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SCHEMA_TREE_CANONICAL.md`<br>`docs/specifications/19_Security_Model/SECURITY_DEFINER_RLS_AND_EMULATED_SCHEMA_SANDBOX_MODEL.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SCHEMA_TREE_CANONICAL.md`<br>`docs/specifications/19_Security_Model/SECURITY_DEFINER_RLS_AND_EMULATED_SCHEMA_SANDBOX_MODEL.md` |

### `catalog.donor_catalog_overlay_and_system_table_emulation` Donor catalog overlay and system-table emulation

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `catalog.donor_catalog_overlay_and_system_table_emulation` | `catalog.donor_catalog_overlay_and_system_table_emulation` |
| Feature Name | Donor catalog overlay and system-table emulation | Donor catalog overlay and system-table emulation |
| Citus State | `NATIVE_SYSTEM_CATALOG_OR_METADATA_SURFACE` | `NATIVE_SYSTEM_CATALOG_OR_METADATA_SURFACE` |
| ScratchBird State | `FAMILY_ADJACENT_OVERLAY_MAPPING` | `PLANNED_DONOR_CATALOG_OVERLAY_PARITY` |
| Outcome | `PARTIAL_PARITY` | `EMULATABLE_WITHOUT_MATERIAL_LOSS` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `CURRENT_GAP` | `NONE` |
| Evidence | `SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare donor system catalogs or metadata surfaces against ScratchBird's virtual-overlay and emulated-schema model. Donor baseline is `NATIVE_SYSTEM_CATALOG_OR_METADATA_SURFACE`. ScratchBird current lane is `FAMILY_ADJACENT_OVERLAY_MAPPING`. The donor surface is broader than the currently proven ScratchBird lane for this row. | Compare donor system catalogs or metadata surfaces against ScratchBird's virtual-overlay and emulated-schema model. Donor baseline is `NATIVE_SYSTEM_CATALOG_OR_METADATA_SURFACE`. ScratchBird specified target lane is `PLANNED_DONOR_CATALOG_OVERLAY_PARITY`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/README.md`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/README.md`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md` |

### `catalog.metadata_publication_and_schema_epoch_rules` Metadata publication and schema-epoch rules

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `catalog.metadata_publication_and_schema_epoch_rules` | `catalog.metadata_publication_and_schema_epoch_rules` |
| Feature Name | Metadata publication and schema-epoch rules | Metadata publication and schema-epoch rules |
| Citus State | `DONOR_METADATA_PUBLICATION_MODEL` | `DONOR_METADATA_PUBLICATION_MODEL` |
| ScratchBird State | `COMMIT_BOUND_SCHEMA_EPOCH_PUBLICATION` | `COMMIT_BOUND_SCHEMA_EPOCH_PUBLICATION` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare commit-bound metadata publication, invalidation, and schema-epoch boundaries. Donor baseline is `DONOR_METADATA_PUBLICATION_MODEL`. ScratchBird current lane is `COMMIT_BOUND_SCHEMA_EPOCH_PUBLICATION`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare commit-bound metadata publication, invalidation, and schema-epoch boundaries. Donor baseline is `DONOR_METADATA_PUBLICATION_MODEL`. ScratchBird specified target lane is `COMMIT_BOUND_SCHEMA_EPOCH_PUBLICATION`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/README.md`<br>`docs/specifications/37_Statistics_Metadata_and_Schema_DDL/SCHEMA_DDL_STATE_MACHINE.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/README.md`<br>`docs/specifications/37_Statistics_Metadata_and_Schema_DDL/SCHEMA_DDL_STATE_MACHINE.md` |

### `catalog.system_views_and_statistics_surface` System views and statistics surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `catalog.system_views_and_statistics_surface` | `catalog.system_views_and_statistics_surface` |
| Feature Name | System views and statistics surface | System views and statistics surface |
| Citus State | `NATIVE_SYSTEM_VIEW_SURFACE` | `NATIVE_SYSTEM_VIEW_SURFACE` |
| ScratchBird State | `PARTIAL_EMULATED_VIEW_SURFACE` | `PLANNED_DONOR_SYSTEM_VIEW_PARITY` |
| Outcome | `PARTIAL_PARITY` | `EMULATABLE_WITHOUT_MATERIAL_LOSS` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `CURRENT_GAP` | `NONE` |
| Evidence | `SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare monitoring, metadata, and statistics views that users or donor tools expect to query directly. Donor baseline is `NATIVE_SYSTEM_VIEW_SURFACE`. ScratchBird current lane is `PARTIAL_EMULATED_VIEW_SURFACE`. The donor surface is broader than the currently proven ScratchBird lane for this row. | Compare monitoring, metadata, and statistics views that users or donor tools expect to query directly. Donor baseline is `NATIVE_SYSTEM_VIEW_SURFACE`. ScratchBird specified target lane is `PLANNED_DONOR_SYSTEM_VIEW_PARITY`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_STATS_MAPPING.md`<br>`docs/specifications/37_Statistics_Metadata_and_Schema_DDL/README.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_STATS_MAPPING.md`<br>`docs/specifications/37_Statistics_Metadata_and_Schema_DDL/README.md` |

## 7. Types and Value Semantics

### `types.native_scalar_and_temporal_surface` Native scalar and temporal type surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `types.native_scalar_and_temporal_surface` | `types.native_scalar_and_temporal_surface` |
| Feature Name | Native scalar and temporal type surface | Native scalar and temporal type surface |
| Citus State | `NATIVE_TYPE_SURFACE` | `NATIVE_TYPE_SURFACE` |
| ScratchBird State | `BROAD_NATIVE_TYPE_SYSTEM` | `BROAD_NATIVE_TYPE_SYSTEM` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare the breadth of native scalar, numeric, binary, UUID, and temporal type support. Donor baseline is `NATIVE_TYPE_SURFACE`. ScratchBird current lane is `BROAD_NATIVE_TYPE_SYSTEM`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare the breadth of native scalar, numeric, binary, UUID, and temporal type support. Donor baseline is `NATIVE_TYPE_SURFACE`. ScratchBird specified target lane is `BROAD_NATIVE_TYPE_SYSTEM`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/14_Base_Scalar_Types/README.md`<br>`docs/specifications/14_Base_Scalar_Types/EMULATED_SCALAR_TYPE_MATRIX.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/14_Base_Scalar_Types/README.md`<br>`docs/specifications/14_Base_Scalar_Types/EMULATED_SCALAR_TYPE_MATRIX.md` |

### `types.complex_type_and_specialized_value_surface` Complex-type and specialized-value surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `types.complex_type_and_specialized_value_surface` | `types.complex_type_and_specialized_value_surface` |
| Feature Name | Complex-type and specialized-value surface | Complex-type and specialized-value surface |
| Citus State | `COMPLEX_OR_SPECIALIZED_VALUE_SURFACE` | `COMPLEX_OR_SPECIALIZED_VALUE_SURFACE` |
| ScratchBird State | `BROAD_COMPLEX_TYPE_SYSTEM` | `BROAD_COMPLEX_TYPE_SYSTEM` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare arrays, composites, variants, JSON-like, BSON-like, vector-like, and other non-scalar value carriers. Donor baseline is `COMPLEX_OR_SPECIALIZED_VALUE_SURFACE`. ScratchBird current lane is `BROAD_COMPLEX_TYPE_SYSTEM`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare arrays, composites, variants, JSON-like, BSON-like, vector-like, and other non-scalar value carriers. Donor baseline is `COMPLEX_OR_SPECIALIZED_VALUE_SURFACE`. ScratchBird specified target lane is `BROAD_COMPLEX_TYPE_SYSTEM`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/15_Complex_Types/README.md`<br>`docs/specifications/15_Complex_Types/TYPE_IO_AND_ERROR_SEMANTICS.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/15_Complex_Types/README.md`<br>`docs/specifications/15_Complex_Types/TYPE_IO_AND_ERROR_SEMANTICS.md` |

### `types.custom_domains_and_user_defined_type_control_plane` Custom domains and user-defined type control plane

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `types.custom_domains_and_user_defined_type_control_plane` | `types.custom_domains_and_user_defined_type_control_plane` |
| Feature Name | Custom domains and user-defined type control plane | Custom domains and user-defined type control plane |
| Citus State | `DOMAIN_OR_USER_DEFINED_TYPE_SURFACE` | `DOMAIN_OR_USER_DEFINED_TYPE_SURFACE` |
| ScratchBird State | `DOMAIN_CONTROL_PLANE_NATIVE` | `DOMAIN_CONTROL_PLANE_NATIVE` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare whether the platform exposes a first-class control plane for custom datatypes or domain-like user-defined value contracts. Donor baseline is `DOMAIN_OR_USER_DEFINED_TYPE_SURFACE`. ScratchBird current lane is `DOMAIN_CONTROL_PLANE_NATIVE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare whether the platform exposes a first-class control plane for custom datatypes or domain-like user-defined value contracts. Donor baseline is `DOMAIN_OR_USER_DEFINED_TYPE_SURFACE`. ScratchBird specified target lane is `DOMAIN_CONTROL_PLANE_NATIVE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/15_Complex_Types/DOMAIN_DDL_AND_CATALOG.md`<br>`docs/specifications/15_Complex_Types/SYSTEM_DOMAIN_UUID_REGISTRY.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/15_Complex_Types/DOMAIN_DDL_AND_CATALOG.md`<br>`docs/specifications/15_Complex_Types/SYSTEM_DOMAIN_UUID_REGISTRY.md` |

### `types.domain_security_masking_and_encryption_metadata` Domain security, masking, and encryption metadata

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `types.domain_security_masking_and_encryption_metadata` | `types.domain_security_masking_and_encryption_metadata` |
| Feature Name | Domain security, masking, and encryption metadata | Domain security, masking, and encryption metadata |
| Citus State | `TYPE_OR_DOMAIN_SECURITY_SURFACE` | `TYPE_OR_DOMAIN_SECURITY_SURFACE` |
| ScratchBird State | `DOMAIN_MASKING_AND_SECURITY_PAYLOAD` | `DOMAIN_MASKING_AND_SECURITY_PAYLOAD` |
| Outcome | `SUBJECT_B_ONLY` | `SUBJECT_B_ONLY` |
| Delta Class | `SUBJECT_B_SUPERSET` | `SUBJECT_B_SUPERSET` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare whether datatype or domain definitions can carry masking, audit, and encryption metadata as part of the type contract. Donor baseline is `TYPE_OR_DOMAIN_SECURITY_SURFACE`. ScratchBird current lane is `DOMAIN_MASKING_AND_SECURITY_PAYLOAD`. | Compare whether datatype or domain definitions can carry masking, audit, and encryption metadata as part of the type contract. Donor baseline is `TYPE_OR_DOMAIN_SECURITY_SURFACE`. ScratchBird specified target lane is `DOMAIN_MASKING_AND_SECURITY_PAYLOAD`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/19_Security_Model/DOMAIN_SECURITY_PAYLOAD_AND_PERMISSION_MASK_MODEL.md`<br>`docs/specifications/19_Security_Model/DOMAIN_SECURITY_MASKING_ENCRYPTION_AND_AUDIT_MODEL.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/19_Security_Model/DOMAIN_SECURITY_PAYLOAD_AND_PERMISSION_MASK_MODEL.md`<br>`docs/specifications/19_Security_Model/DOMAIN_SECURITY_MASKING_ENCRYPTION_AND_AUDIT_MODEL.md` |

### `types.donor_type_mapping_and_emulation_surface` Donor type-mapping and emulation surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `types.donor_type_mapping_and_emulation_surface` | `types.donor_type_mapping_and_emulation_surface` |
| Feature Name | Donor type-mapping and emulation surface | Donor type-mapping and emulation surface |
| Citus State | `DONOR_TYPE_SURFACE` | `DONOR_TYPE_SURFACE` |
| ScratchBird State | `FAMILY_TYPE_MAPPING_SPECIFIED` | `PLANNED_DONOR_TYPE_MAPPING_PARITY` |
| Outcome | `PARTIAL_PARITY` | `EMULATABLE_WITHOUT_MATERIAL_LOSS` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `CURRENT_GAP` | `NONE` |
| Evidence | `SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare donor-facing type compatibility against ScratchBird's emulated type and mapping surfaces. Donor baseline is `DONOR_TYPE_SURFACE`. ScratchBird current lane is `FAMILY_TYPE_MAPPING_SPECIFIED`. The donor surface is broader than the currently proven ScratchBird lane for this row. | Compare donor-facing type compatibility against ScratchBird's emulated type and mapping surfaces. Donor baseline is `DONOR_TYPE_SURFACE`. ScratchBird specified target lane is `PLANNED_DONOR_TYPE_MAPPING_PARITY`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/15_Complex_Types/EMULATED_COMPLEX_TYPE_MATRIX.md`<br>`docs/specifications/14_Base_Scalar_Types/EMULATED_SCALAR_TYPE_MATRIX.md`<br>`docs/specifications/28_Parser_Implementations/SCHEMA_VISIBILITY_AND_TRANSLATION_MATRIX.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/15_Complex_Types/EMULATED_COMPLEX_TYPE_MATRIX.md`<br>`docs/specifications/14_Base_Scalar_Types/EMULATED_SCALAR_TYPE_MATRIX.md`<br>`docs/specifications/28_Parser_Implementations/SCHEMA_VISIBILITY_AND_TRANSLATION_MATRIX.md` |

## 8. DDL and Schema Lifecycle

### `ddl.object_lifecycle_and_dependency_publication` Object lifecycle and dependency publication

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `ddl.object_lifecycle_and_dependency_publication` | `ddl.object_lifecycle_and_dependency_publication` |
| Feature Name | Object lifecycle and dependency publication | Object lifecycle and dependency publication |
| Citus State | `NATIVE_OBJECT_LIFECYCLE_SURFACE` | `NATIVE_OBJECT_LIFECYCLE_SURFACE` |
| ScratchBird State | `NATIVE_OBJECT_LIFECYCLE_AND_PUBLICATION` | `NATIVE_OBJECT_LIFECYCLE_AND_PUBLICATION` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare create, alter, drop, rename, dependency replacement, and publication boundaries. Donor baseline is `NATIVE_OBJECT_LIFECYCLE_SURFACE`. ScratchBird current lane is `NATIVE_OBJECT_LIFECYCLE_AND_PUBLICATION`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare create, alter, drop, rename, dependency replacement, and publication boundaries. Donor baseline is `NATIVE_OBJECT_LIFECYCLE_SURFACE`. ScratchBird specified target lane is `NATIVE_OBJECT_LIFECYCLE_AND_PUBLICATION`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/21_V3_Dialect_Surface/README.md`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_INVALIDATION_DEPENDENCY_GRAPH_AND_SELF_CHECK.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/21_V3_Dialect_Surface/README.md`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_INVALIDATION_DEPENDENCY_GRAPH_AND_SELF_CHECK.md` |

### `ddl.donor_schema_mapping_and_search_path_behavior` Donor schema mapping and search-path behavior

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `ddl.donor_schema_mapping_and_search_path_behavior` | `ddl.donor_schema_mapping_and_search_path_behavior` |
| Feature Name | Donor schema mapping and search-path behavior | Donor schema mapping and search-path behavior |
| Citus State | `DONOR_SCHEMA_AND_SEARCH_PATH_MODEL` | `DONOR_SCHEMA_AND_SEARCH_PATH_MODEL` |
| ScratchBird State | `FAMILY_SCHEMA_MAPPING` | `PLANNED_DONOR_SCHEMA_OR_NAMESPACE_MAPPING_PARITY` |
| Outcome | `PARTIAL_PARITY` | `EMULATABLE_WITHOUT_MATERIAL_LOSS` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `CURRENT_GAP` | `NONE` |
| Evidence | `SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare donor database, schema, and search-path semantics against ScratchBird's emulated-schema-root model. Donor baseline is `DONOR_SCHEMA_AND_SEARCH_PATH_MODEL`. ScratchBird current lane is `FAMILY_SCHEMA_MAPPING`. The donor surface is broader than the currently proven ScratchBird lane for this row. | Compare donor database, schema, and search-path semantics against ScratchBird's emulated-schema-root model. Donor baseline is `DONOR_SCHEMA_AND_SEARCH_PATH_MODEL`. ScratchBird specified target lane is `PLANNED_DONOR_SCHEMA_OR_NAMESPACE_MAPPING_PARITY`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SCHEMA_HOME_AND_SEARCH_PATH_SEMANTICS.md`<br>`docs/specifications/29_Listener_and_Server_Orchestration/CONNECTION_AND_SESSION_LIFECYCLE.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SCHEMA_HOME_AND_SEARCH_PATH_SEMANTICS.md`<br>`docs/specifications/29_Listener_and_Server_Orchestration/CONNECTION_AND_SESSION_LIFECYCLE.md` |

## 9. DML, Query, and Planner Surface

### `dml.query_and_mutation_surface` Query and mutation surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `dml.query_and_mutation_surface` | `dml.query_and_mutation_surface` |
| Feature Name | Query and mutation surface | Query and mutation surface |
| Citus State | `DONOR_QUERY_AND_MUTATION_SURFACE` | `DONOR_QUERY_AND_MUTATION_SURFACE` |
| ScratchBird State | `NATIVE_SQL_CORE_WITH_EMULATION_BOUNDARIES` | `NATIVE_SQL_CORE_WITH_EMULATION_BOUNDARIES` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare selection, mutation, returning behavior, and the breadth of the primary relational or query surface. Donor baseline is `DONOR_QUERY_AND_MUTATION_SURFACE`. ScratchBird current lane is `NATIVE_SQL_CORE_WITH_EMULATION_BOUNDARIES`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare selection, mutation, returning behavior, and the breadth of the primary relational or query surface. Donor baseline is `DONOR_QUERY_AND_MUTATION_SURFACE`. ScratchBird specified target lane is `NATIVE_SQL_CORE_WITH_EMULATION_BOUNDARIES`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/21_V3_Dialect_Surface/README.md`<br>`docs/specifications/36_Query_Rewrite_and_Planner/README.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/21_V3_Dialect_Surface/README.md`<br>`docs/specifications/36_Query_Rewrite_and_Planner/README.md` |

### `dml.donor_query_semantics_emulation_surface` Donor query-semantics emulation surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `dml.donor_query_semantics_emulation_surface` | `dml.donor_query_semantics_emulation_surface` |
| Feature Name | Donor query-semantics emulation surface | Donor query-semantics emulation surface |
| Citus State | `DONOR_QUERY_SEMANTICS` | `DONOR_QUERY_SEMANTICS` |
| ScratchBird State | `FAMILY_QUERY_MAPPING` | `PLANNED_DONOR_QUERY_EMULATION_PARITY` |
| Outcome | `PARTIAL_PARITY` | `EMULATABLE_WITHOUT_MATERIAL_LOSS` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `CURRENT_GAP` | `NONE` |
| Evidence | `SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare donor-facing SQL or query semantics against the shipped or specified ScratchBird emulation surface. Donor baseline is `DONOR_QUERY_SEMANTICS`. ScratchBird current lane is `FAMILY_QUERY_MAPPING`. The donor surface is broader than the currently proven ScratchBird lane for this row. | Compare donor-facing SQL or query semantics against the shipped or specified ScratchBird emulation surface. Donor baseline is `DONOR_QUERY_SEMANTICS`. ScratchBird specified target lane is `PLANNED_DONOR_QUERY_EMULATION_PARITY`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/28_Parser_Implementations/README.md`<br>`docs/specifications/29_Listener_and_Server_Orchestration/README.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/28_Parser_Implementations/README.md`<br>`docs/specifications/29_Listener_and_Server_Orchestration/README.md` |

### `planner.path_taxonomy_plan_control_and_explain_surface` Planner-path taxonomy, plan control, and explain surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `planner.path_taxonomy_plan_control_and_explain_surface` | `planner.path_taxonomy_plan_control_and_explain_surface` |
| Feature Name | Planner-path taxonomy, plan control, and explain surface | Planner-path taxonomy, plan control, and explain surface |
| Citus State | `DONOR_PLAN_AND_EXPLAIN_SURFACE` | `DONOR_PLAN_AND_EXPLAIN_SURFACE` |
| ScratchBird State | `PARTIAL_CODE_BACKED_PLANNER_TAXONOMY` | `CANONICAL_PLANNER_TAXONOMY` |
| Outcome | `PARTIAL_PARITY` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `CURRENT_GAP` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare path taxonomy, explainability, typed planner metrics, and plan-control visibility. Donor baseline is `DONOR_PLAN_AND_EXPLAIN_SURFACE`. ScratchBird current lane is `PARTIAL_CODE_BACKED_PLANNER_TAXONOMY`. The donor surface is broader than the currently proven ScratchBird lane for this row. | Compare path taxonomy, explainability, typed planner metrics, and plan-control visibility. Donor baseline is `DONOR_PLAN_AND_EXPLAIN_SURFACE`. ScratchBird specified target lane is `CANONICAL_PLANNER_TAXONOMY`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/INDEX_PLANNER_PATH_TAXONOMY_AND_EXACTNESS.md`<br>`docs/specifications/31_Conformance_Performance_and_Reliability_Gates/OPTIMIZER_PARITY_TRACE_AND_PLAN_EXPLANATION_CERTIFICATION_MODEL.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/INDEX_PLANNER_PATH_TAXONOMY_AND_EXACTNESS.md`<br>`docs/specifications/31_Conformance_Performance_and_Reliability_Gates/OPTIMIZER_PARITY_TRACE_AND_PLAN_EXPLANATION_CERTIFICATION_MODEL.md` |

## 10. Indexing and Access Paths

### `indexes.runtime_family_surface` Index runtime-family surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `indexes.runtime_family_surface` | `indexes.runtime_family_surface` |
| Feature Name | Index runtime-family surface | Index runtime-family surface |
| Citus State | `DONOR_INDEX_FAMILY_SURFACE` | `DONOR_INDEX_FAMILY_SURFACE` |
| ScratchBird State | `MULTI_FAMILY_INDEX_RUNTIME` | `MULTI_FAMILY_INDEX_RUNTIME` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare the breadth of admitted index runtime families rather than treating indexing as one generic feature. Donor baseline is `DONOR_INDEX_FAMILY_SURFACE`. ScratchBird current lane is `MULTI_FAMILY_INDEX_RUNTIME`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare the breadth of admitted index runtime families rather than treating indexing as one generic feature. Donor baseline is `DONOR_INDEX_FAMILY_SURFACE`. ScratchBird specified target lane is `MULTI_FAMILY_INDEX_RUNTIME`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/README.md`<br>`docs/specifications/18_Index_Framework/INDEX_RUNTIME_TAXONOMY_AND_ALIAS_LOWERING.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/README.md`<br>`docs/specifications/18_Index_Framework/INDEX_RUNTIME_TAXONOMY_AND_ALIAS_LOWERING.md` |

### `indexes.donor_index_mapping_and_alias_lowering` Donor index mapping and alias lowering

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `indexes.donor_index_mapping_and_alias_lowering` | `indexes.donor_index_mapping_and_alias_lowering` |
| Feature Name | Donor index mapping and alias lowering | Donor index mapping and alias lowering |
| Citus State | `DONOR_INDEX_NAME_AND_BEHAVIOR_SURFACE` | `DONOR_INDEX_NAME_AND_BEHAVIOR_SURFACE` |
| ScratchBird State | `CANONICAL_INDEX_ALIAS_LOWERING` | `CANONICAL_INDEX_ALIAS_LOWERING` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare donor index-name and index-behavior mapping against ScratchBird's canonical index-family lowering model. Donor baseline is `DONOR_INDEX_NAME_AND_BEHAVIOR_SURFACE`. ScratchBird current lane is `CANONICAL_INDEX_ALIAS_LOWERING`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare donor index-name and index-behavior mapping against ScratchBird's canonical index-family lowering model. Donor baseline is `DONOR_INDEX_NAME_AND_BEHAVIOR_SURFACE`. ScratchBird specified target lane is `CANONICAL_INDEX_ALIAS_LOWERING`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/INDEX_RUNTIME_TAXONOMY_AND_ALIAS_LOWERING.md`<br>`docs/specifications/18_Index_Framework/DIALECT_COMPATIBILITY_MATRIX.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/INDEX_RUNTIME_TAXONOMY_AND_ALIAS_LOWERING.md`<br>`docs/specifications/18_Index_Framework/DIALECT_COMPATIBILITY_MATRIX.md` |

### `indexes.specialized_index_families` Specialized index families

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `indexes.specialized_index_families` | `indexes.specialized_index_families` |
| Feature Name | Specialized index families | Specialized index families |
| Citus State | `DONOR_SPECIALIZED_INDEX_SURFACE` | `DONOR_SPECIALIZED_INDEX_SURFACE` |
| ScratchBird State | `BROAD_SPECIALIZED_INDEX_SURFACE` | `BROAD_SPECIALIZED_INDEX_SURFACE` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare text, spatial, vector, graph, summary, and donor-specific specialized index surfaces. Donor baseline is `DONOR_SPECIALIZED_INDEX_SURFACE`. ScratchBird current lane is `BROAD_SPECIALIZED_INDEX_SURFACE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare text, spatial, vector, graph, summary, and donor-specific specialized index surfaces. Donor baseline is `DONOR_SPECIALIZED_INDEX_SURFACE`. ScratchBird specified target lane is `BROAD_SPECIALIZED_INDEX_SURFACE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/README.md`<br>`docs/specifications/18_Index_Framework/GENERALIZED_SEARCH_AND_SPATIAL_PLANNER_SPEC.md`<br>`docs/specifications/18_Index_Framework/VECTOR_ANN_PLANNER_SPEC.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/README.md`<br>`docs/specifications/18_Index_Framework/GENERALIZED_SEARCH_AND_SPATIAL_PLANNER_SPEC.md`<br>`docs/specifications/18_Index_Framework/VECTOR_ANN_PLANNER_SPEC.md` |

### `indexes.mga_safe_publication_reclaim_and_metrics` MGA-safe publication, reclaim, and metrics

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `indexes.mga_safe_publication_reclaim_and_metrics` | `indexes.mga_safe_publication_reclaim_and_metrics` |
| Feature Name | MGA-safe publication, reclaim, and metrics | MGA-safe publication, reclaim, and metrics |
| Citus State | `DONOR_INDEX_PUBLICATION_AND_METRICS_MODEL` | `DONOR_INDEX_PUBLICATION_AND_METRICS_MODEL` |
| ScratchBird State | `MGA_SAFE_INDEX_PUBLICATION_AND_METRICS` | `MGA_SAFE_INDEX_PUBLICATION_AND_METRICS` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare whether index publication, reclaim, and observability remain subordinate to MGA-visible row truth. Donor baseline is `DONOR_INDEX_PUBLICATION_AND_METRICS_MODEL`. ScratchBird current lane is `MGA_SAFE_INDEX_PUBLICATION_AND_METRICS`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare whether index publication, reclaim, and observability remain subordinate to MGA-visible row truth. Donor baseline is `DONOR_INDEX_PUBLICATION_AND_METRICS_MODEL`. ScratchBird specified target lane is `MGA_SAFE_INDEX_PUBLICATION_AND_METRICS`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/INDEX_MGA_PUBLICATION_AND_RECLAIM.md`<br>`docs/specifications/18_Index_Framework/INDEX_FAMILY_NATIVE_METRICS_PACKET_CONTRACT.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/INDEX_MGA_PUBLICATION_AND_RECLAIM.md`<br>`docs/specifications/18_Index_Framework/INDEX_FAMILY_NATIVE_METRICS_PACKET_CONTRACT.md` |

## 11. Callable, Procedural, and Trigger Surface

### `functions.builtin_function_and_operator_surface` Built-in function and operator surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `functions.builtin_function_and_operator_surface` | `functions.builtin_function_and_operator_surface` |
| Feature Name | Built-in function and operator surface | Built-in function and operator surface |
| Citus State | `DONOR_BUILTIN_FUNCTION_SURFACE` | `DONOR_BUILTIN_FUNCTION_SURFACE` |
| ScratchBird State | `BROAD_BUILTIN_FUNCTION_AND_OPERATOR_SURFACE` | `BROAD_BUILTIN_FUNCTION_AND_OPERATOR_SURFACE` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare built-in callable surface, scalar operators, casts, aggregates, and donor-facing function families. Donor baseline is `DONOR_BUILTIN_FUNCTION_SURFACE`. ScratchBird current lane is `BROAD_BUILTIN_FUNCTION_AND_OPERATOR_SURFACE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare built-in callable surface, scalar operators, casts, aggregates, and donor-facing function families. Donor baseline is `DONOR_BUILTIN_FUNCTION_SURFACE`. ScratchBird specified target lane is `BROAD_BUILTIN_FUNCTION_AND_OPERATOR_SURFACE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/17_Functions_and_Procedures/README.md`<br>`docs/specifications/13_Operator_Model_and_Coercion/README.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/17_Functions_and_Procedures/README.md`<br>`docs/specifications/13_Operator_Model_and_Coercion/README.md` |

### `procedural.server_side_code_trigger_and_routine_surface` Server-side code, trigger, and routine surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `procedural.server_side_code_trigger_and_routine_surface` | `procedural.server_side_code_trigger_and_routine_surface` |
| Feature Name | Server-side code, trigger, and routine surface | Server-side code, trigger, and routine surface |
| Citus State | `DONOR_SERVER_SIDE_CODE_SURFACE` | `DONOR_SERVER_SIDE_CODE_SURFACE` |
| ScratchBird State | `NATIVE_ROUTINE_AND_TRIGGER_SURFACE` | `NATIVE_ROUTINE_AND_TRIGGER_SURFACE` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare procedures, triggers, stored routines, and the surrounding execution model. Donor baseline is `DONOR_SERVER_SIDE_CODE_SURFACE`. ScratchBird current lane is `NATIVE_ROUTINE_AND_TRIGGER_SURFACE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare procedures, triggers, stored routines, and the surrounding execution model. Donor baseline is `DONOR_SERVER_SIDE_CODE_SURFACE`. ScratchBird specified target lane is `NATIVE_ROUTINE_AND_TRIGGER_SURFACE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/17_Functions_and_Procedures/README.md`<br>`docs/specifications/21_V3_Dialect_Surface/NATIVE_PSQL_TSQL_LANGUAGE_DEFINITION.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/17_Functions_and_Procedures/README.md`<br>`docs/specifications/21_V3_Dialect_Surface/NATIVE_PSQL_TSQL_LANGUAGE_DEFINITION.md` |

### `procedural.sblr_bytecode_runtime_and_compilation_lane` SBLR bytecode runtime and compilation lane

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `procedural.sblr_bytecode_runtime_and_compilation_lane` | `procedural.sblr_bytecode_runtime_and_compilation_lane` |
| Feature Name | SBLR bytecode runtime and compilation lane | SBLR bytecode runtime and compilation lane |
| Citus State | `NO_SBLR_EQUIVALENT` | `NO_SBLR_EQUIVALENT` |
| ScratchBird State | `SBLR_BYTECODE_RUNTIME_WITH_LLVM` | `SBLR_BYTECODE_RUNTIME_WITH_LLVM` |
| Outcome | `NOT_COMPARABLE` | `NOT_COMPARABLE` |
| Delta Class | `ARCHITECTURALLY_DIFFERENT` | `ARCHITECTURALLY_DIFFERENT` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare donor routine execution against ScratchBird's native SBLR bytecode runtime with LLVM-backed compilation lanes. Donor baseline is `NO_SBLR_EQUIVALENT`. ScratchBird current lane is `SBLR_BYTECODE_RUNTIME_WITH_LLVM`. | Compare donor routine execution against ScratchBird's native SBLR bytecode runtime with LLVM-backed compilation lanes. Donor baseline is `NO_SBLR_EQUIVALENT`. ScratchBird specified target lane is `SBLR_BYTECODE_RUNTIME_WITH_LLVM`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/README.md`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_SBLR_EXECUTION_ARTIFACTS.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/README.md`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_SBLR_EXECUTION_ARTIFACTS.md` |

## 12. Security, Identity, and Policy Surface

### `security.identity_authentication_and_provider_chain` Identity, authentication, and provider chain

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `security.identity_authentication_and_provider_chain` | `security.identity_authentication_and_provider_chain` |
| Feature Name | Identity, authentication, and provider chain | Identity, authentication, and provider chain |
| Citus State | `DONOR_AUTHENTICATION_SURFACE` | `DONOR_AUTHENTICATION_SURFACE` |
| ScratchBird State | `AUTH_PROVIDER_CHAIN_AND_SESSION_IDENTITY` | `AUTH_PROVIDER_CHAIN_AND_SESSION_IDENTITY` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare user, role, authentication, and provider-chain behavior exposed to clients and operators. Donor baseline is `DONOR_AUTHENTICATION_SURFACE`. ScratchBird current lane is `AUTH_PROVIDER_CHAIN_AND_SESSION_IDENTITY`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare user, role, authentication, and provider-chain behavior exposed to clients and operators. Donor baseline is `DONOR_AUTHENTICATION_SURFACE`. ScratchBird specified target lane is `AUTH_PROVIDER_CHAIN_AND_SESSION_IDENTITY`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/19_Security_Model/README.md`<br>`docs/specifications/19_Security_Model/ENGINE_AUTHENTICATION_HARDENING_AND_MANAGER_OPTION_SPEC.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/19_Security_Model/README.md`<br>`docs/specifications/19_Security_Model/ENGINE_AUTHENTICATION_HARDENING_AND_MANAGER_OPTION_SPEC.md` |

### `security.row_column_domain_masking_and_policy_pipeline` Row, column, domain, masking, and policy pipeline

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `security.row_column_domain_masking_and_policy_pipeline` | `security.row_column_domain_masking_and_policy_pipeline` |
| Feature Name | Row, column, domain, masking, and policy pipeline | Row, column, domain, masking, and policy pipeline |
| Citus State | `DONOR_PRIVILEGE_AND_POLICY_SURFACE` | `DONOR_PRIVILEGE_AND_POLICY_SURFACE` |
| ScratchBird State | `ROW_COLUMN_DOMAIN_MASKING_PIPELINE` | `ROW_COLUMN_DOMAIN_MASKING_PIPELINE` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare whether disclosure control spans row policy, column policy, domain security, masking, and audit in one deterministic pipeline. Donor baseline is `DONOR_PRIVILEGE_AND_POLICY_SURFACE`. ScratchBird current lane is `ROW_COLUMN_DOMAIN_MASKING_PIPELINE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare whether disclosure control spans row policy, column policy, domain security, masking, and audit in one deterministic pipeline. Donor baseline is `DONOR_PRIVILEGE_AND_POLICY_SURFACE`. ScratchBird specified target lane is `ROW_COLUMN_DOMAIN_MASKING_PIPELINE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/19_Security_Model/ROW_COLUMN_DOMAIN_SECURITY_MASKING_EVALUATION_ORDER_MODEL.md`<br>`docs/specifications/19_Security_Model/PRIVILEGE_GRAPH_ROW_COLUMN_DOMAIN_SECURITY_AND_MASKING_MODEL.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/19_Security_Model/ROW_COLUMN_DOMAIN_SECURITY_MASKING_EVALUATION_ORDER_MODEL.md`<br>`docs/specifications/19_Security_Model/PRIVILEGE_GRAPH_ROW_COLUMN_DOMAIN_SECURITY_AND_MASKING_MODEL.md` |

### `security.definer_invoker_and_schema_sandbox_security` Definer/invoker and schema-sandbox security

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `security.definer_invoker_and_schema_sandbox_security` | `security.definer_invoker_and_schema_sandbox_security` |
| Feature Name | Definer/invoker and schema-sandbox security | Definer/invoker and schema-sandbox security |
| Citus State | `DONOR_DEFINER_OR_SCHEMA_SECURITY_MODEL` | `DONOR_DEFINER_OR_SCHEMA_SECURITY_MODEL` |
| ScratchBird State | `DEFINER_INVOKER_AND_SCHEMA_SANDBOX_SECURITY` | `DEFINER_INVOKER_AND_SCHEMA_SANDBOX_SECURITY` |
| Outcome | `SUBJECT_B_ONLY` | `SUBJECT_B_ONLY` |
| Delta Class | `SUBJECT_B_SUPERSET` | `SUBJECT_B_SUPERSET` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare definer or invoker behavior, schema sandboxing, and policy resolution at the execution boundary. Donor baseline is `DONOR_DEFINER_OR_SCHEMA_SECURITY_MODEL`. ScratchBird current lane is `DEFINER_INVOKER_AND_SCHEMA_SANDBOX_SECURITY`. | Compare definer or invoker behavior, schema sandboxing, and policy resolution at the execution boundary. Donor baseline is `DONOR_DEFINER_OR_SCHEMA_SECURITY_MODEL`. ScratchBird specified target lane is `DEFINER_INVOKER_AND_SCHEMA_SANDBOX_SECURITY`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/19_Security_Model/SECURITY_DEFINER_RLS_AND_EMULATED_SCHEMA_SANDBOX_MODEL.md`<br>`docs/specifications/19_Security_Model/ROW_COLUMN_DOMAIN_MASKING_AND_SANDBOX_SECURITY_MODEL.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/19_Security_Model/SECURITY_DEFINER_RLS_AND_EMULATED_SCHEMA_SANDBOX_MODEL.md`<br>`docs/specifications/19_Security_Model/ROW_COLUMN_DOMAIN_MASKING_AND_SANDBOX_SECURITY_MODEL.md` |

## 13. Parser, Protocol, and Emulation Front Door

### `parser.engine_parser_separation_and_sblr_lowering` Engine/parser separation and SBLR lowering

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `parser.engine_parser_separation_and_sblr_lowering` | `parser.engine_parser_separation_and_sblr_lowering` |
| Feature Name | Engine/parser separation and SBLR lowering | Engine/parser separation and SBLR lowering |
| Citus State | `INTEGRATED_PARSER_AND_ENGINE_SURFACE` | `INTEGRATED_PARSER_AND_ENGINE_SURFACE` |
| ScratchBird State | `SEPARATE_PARSER_FAMILIES_LOWER_TO_SBLR` | `SEPARATE_PARSER_FAMILIES_LOWER_TO_SBLR` |
| Outcome | `NOT_COMPARABLE` | `NOT_COMPARABLE` |
| Delta Class | `ARCHITECTURALLY_DIFFERENT` | `ARCHITECTURALLY_DIFFERENT` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare integrated donor parser models against ScratchBird's separate parser families and dialect-local lowering to SBLR. Donor baseline is `INTEGRATED_PARSER_AND_ENGINE_SURFACE`. ScratchBird current lane is `SEPARATE_PARSER_FAMILIES_LOWER_TO_SBLR`. | Compare integrated donor parser models against ScratchBird's separate parser families and dialect-local lowering to SBLR. Donor baseline is `INTEGRATED_PARSER_AND_ENGINE_SURFACE`. ScratchBird specified target lane is `SEPARATE_PARSER_FAMILIES_LOWER_TO_SBLR`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/28_Parser_Implementations/README.md`<br>`docs/specifications/29_Listener_and_Server_Orchestration/PARSER_AGENT_EXECUTABLE_COMPOSITION_AND_RUNTIME_STACK_MODEL.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/28_Parser_Implementations/README.md`<br>`docs/specifications/29_Listener_and_Server_Orchestration/PARSER_AGENT_EXECUTABLE_COMPOSITION_AND_RUNTIME_STACK_MODEL.md` |

### `protocol.dedicated_donor_front_door` Dedicated donor front door

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `protocol.dedicated_donor_front_door` | `protocol.dedicated_donor_front_door` |
| Feature Name | Dedicated donor front door | Dedicated donor front door |
| Citus State | `DONOR_NATIVE_FRONT_DOOR` | `DONOR_NATIVE_FRONT_DOOR` |
| ScratchBird State | `FAMILY_ADJACENT_FRONT_DOOR` | `PLANNED_DONOR_FRONT_DOOR_PARITY` |
| Outcome | `PARTIAL_PARITY` | `EMULATABLE_WITHOUT_MATERIAL_LOSS` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `CURRENT_GAP` | `NONE` |
| Evidence | `SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare donor wire protocol or API front doors against the shipped or specified ScratchBird parser-adapter entry points. Donor baseline is `DONOR_NATIVE_FRONT_DOOR`. ScratchBird current lane is `FAMILY_ADJACENT_FRONT_DOOR`. The donor surface is broader than the currently proven ScratchBird lane for this row. | Compare donor wire protocol or API front doors against the shipped or specified ScratchBird parser-adapter entry points. Donor baseline is `DONOR_NATIVE_FRONT_DOOR`. ScratchBird specified target lane is `PLANNED_DONOR_FRONT_DOOR_PARITY`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/28_Parser_Implementations/README.md`<br>`docs/specifications/29_Listener_and_Server_Orchestration/README.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/28_Parser_Implementations/README.md`<br>`docs/specifications/29_Listener_and_Server_Orchestration/README.md` |

### `protocol.prepared_parameter_error_and_result_mapping` Prepared, parameter, error, and result mapping

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `protocol.prepared_parameter_error_and_result_mapping` | `protocol.prepared_parameter_error_and_result_mapping` |
| Feature Name | Prepared, parameter, error, and result mapping | Prepared, parameter, error, and result mapping |
| Citus State | `DONOR_PREPARED_AND_ERROR_SURFACE` | `DONOR_PREPARED_AND_ERROR_SURFACE` |
| ScratchBird State | `FAMILY_PREPARED_AND_ERROR_MAPPING` | `PLANNED_DONOR_RESULT_AND_ERROR_PARITY` |
| Outcome | `PARTIAL_PARITY` | `EMULATABLE_WITHOUT_MATERIAL_LOSS` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `CURRENT_GAP` | `NONE` |
| Evidence | `SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare prepared statements, parameter binding, error tuples, and donor-facing result framing. Donor baseline is `DONOR_PREPARED_AND_ERROR_SURFACE`. ScratchBird current lane is `FAMILY_PREPARED_AND_ERROR_MAPPING`. The donor surface is broader than the currently proven ScratchBird lane for this row. | Compare prepared statements, parameter binding, error tuples, and donor-facing result framing. Donor baseline is `DONOR_PREPARED_AND_ERROR_SURFACE`. ScratchBird specified target lane is `PLANNED_DONOR_RESULT_AND_ERROR_PARITY`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/28_Parser_Implementations/NORMATIVE_WIRE_PROTOCOL_MYSQL_CHECKLIST.md`<br>`docs/specifications/28_Parser_Implementations/NORMATIVE_WIRE_PROTOCOL_POSTGRESQL_CHECKLIST.md`<br>`docs/specifications/28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md`<br>`docs/specifications/28_Parser_Implementations/README.md`<br>`docs/specifications/29_Listener_and_Server_Orchestration/README.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/28_Parser_Implementations/NORMATIVE_WIRE_PROTOCOL_MYSQL_CHECKLIST.md`<br>`docs/specifications/28_Parser_Implementations/NORMATIVE_WIRE_PROTOCOL_POSTGRESQL_CHECKLIST.md`<br>`docs/specifications/28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md`<br>`docs/specifications/28_Parser_Implementations/README.md`<br>`docs/specifications/29_Listener_and_Server_Orchestration/README.md` |

### `protocol.donor_tool_and_upstream_harness_compatibility` Donor-tool and upstream-harness compatibility

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `protocol.donor_tool_and_upstream_harness_compatibility` | `protocol.donor_tool_and_upstream_harness_compatibility` |
| Feature Name | Donor-tool and upstream-harness compatibility | Donor-tool and upstream-harness compatibility |
| Citus State | `DONOR_TOOLS_EXIST` | `DONOR_TOOLS_EXIST` |
| ScratchBird State | `FAMILY_TOOLING_ONLY` | `PLANNED_DONOR_TOOL_AND_HARNESS_PARITY` |
| Outcome | `PARTIAL_PARITY` | `SUBJECT_B_ONLY` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `SUBJECT_B_SUPERSET` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare donor-native client and upstream harness execution against ScratchBird's current bounded proof or specified target. Donor baseline is `DONOR_TOOLS_EXIST`. ScratchBird current lane is `FAMILY_TOOLING_ONLY`. | Compare donor-native client and upstream harness execution against ScratchBird's current bounded proof or specified target. Donor baseline is `DONOR_TOOLS_EXIST`. ScratchBird specified target lane is `PLANNED_DONOR_TOOL_AND_HARNESS_PARITY`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`tests/compatibility/README.md`<br>`tests/compatibility/postgresql/README.md`<br>`tests/compatibility/mysql/README.md`<br>`tests/compatibility/firebird/README.md`<br>`docs/specifications/28_Parser_Implementations/README.md`<br>`docs/specifications/29_Listener_and_Server_Orchestration/README.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`tests/compatibility/README.md`<br>`tests/compatibility/postgresql/README.md`<br>`tests/compatibility/mysql/README.md`<br>`tests/compatibility/firebird/README.md`<br>`docs/specifications/28_Parser_Implementations/README.md`<br>`docs/specifications/29_Listener_and_Server_Orchestration/README.md` |

## 14. Operations, Introspection, and Data Movement

### `operations.runtime_modes_operator_views_and_utilities` Runtime modes, operator views, and utilities

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `operations.runtime_modes_operator_views_and_utilities` | `operations.runtime_modes_operator_views_and_utilities` |
| Feature Name | Runtime modes, operator views, and utilities | Runtime modes, operator views, and utilities |
| Citus State | `DONOR_RUNTIME_AND_OPERATOR_SURFACE` | `DONOR_RUNTIME_AND_OPERATOR_SURFACE` |
| ScratchBird State | `EMBEDDED_AND_LISTENER_RUNTIME_MODES` | `EMBEDDED_AND_LISTENER_RUNTIME_MODES` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare embedded, server, listener, and operator-visible runtime modes and utilities. Donor baseline is `DONOR_RUNTIME_AND_OPERATOR_SURFACE`. ScratchBird current lane is `EMBEDDED_AND_LISTENER_RUNTIME_MODES`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare embedded, server, listener, and operator-visible runtime modes and utilities. Donor baseline is `DONOR_RUNTIME_AND_OPERATOR_SURFACE`. ScratchBird specified target lane is `EMBEDDED_AND_LISTENER_RUNTIME_MODES`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/25_Runtime_Modes/README.md`<br>`docs/specifications/30_Client_Tooling/README.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/25_Runtime_Modes/README.md`<br>`docs/specifications/30_Client_Tooling/README.md` |

### `operations.mga_diagnostics_and_introspection` MGA diagnostics and introspection

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `operations.mga_diagnostics_and_introspection` | `operations.mga_diagnostics_and_introspection` |
| Feature Name | MGA diagnostics and introspection | MGA diagnostics and introspection |
| Citus State | `DONOR_INTROSPECTION_AND_DIAGNOSTICS_SURFACE` | `DONOR_INTROSPECTION_AND_DIAGNOSTICS_SURFACE` |
| ScratchBird State | `MGA_DIAGNOSTICS_AND_OPERATOR_INTROSPECTION` | `MGA_DIAGNOSTICS_AND_OPERATOR_INTROSPECTION` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare introspection surfaces that expose transaction, sweep, lineage, and derivative-lane health. Donor baseline is `DONOR_INTROSPECTION_AND_DIAGNOSTICS_SURFACE`. ScratchBird current lane is `MGA_DIAGNOSTICS_AND_OPERATOR_INTROSPECTION`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare introspection surfaces that expose transaction, sweep, lineage, and derivative-lane health. Donor baseline is `DONOR_INTROSPECTION_AND_DIAGNOSTICS_SURFACE`. ScratchBird specified target lane is `MGA_DIAGNOSTICS_AND_OPERATOR_INTROSPECTION`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/MGA_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/PAGE_WALKER_AND_REPAIR.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/MGA_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/PAGE_WALKER_AND_REPAIR.md` |

### `data_movement.backup_restore_bulk_import_export` Backup, restore, bulk, import, and export

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `data_movement.backup_restore_bulk_import_export` | `data_movement.backup_restore_bulk_import_export` |
| Feature Name | Backup, restore, bulk, import, and export | Backup, restore, bulk, import, and export |
| Citus State | `DONOR_BACKUP_AND_BULK_SURFACE` | `DONOR_BACKUP_AND_BULK_SURFACE` |
| ScratchBird State | `BACKUP_RESTORE_AND_BULK_PATHS` | `BACKUP_RESTORE_AND_BULK_PATHS` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare backup, restore, dump, load, import, export, and bulk data paths. Donor baseline is `DONOR_BACKUP_AND_BULK_SURFACE`. ScratchBird current lane is `BACKUP_RESTORE_AND_BULK_PATHS`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare backup, restore, dump, load, import, export, and bulk data paths. Donor baseline is `DONOR_BACKUP_AND_BULK_SURFACE`. ScratchBird specified target lane is `BACKUP_RESTORE_AND_BULK_PATHS`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/README.md`<br>`docs/specifications/25_Runtime_Modes/README.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/README.md`<br>`docs/specifications/25_Runtime_Modes/README.md` |

### `data_movement.cdc_migration_replay_and_change_capture` CDC, migration, replay, and change capture

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `data_movement.cdc_migration_replay_and_change_capture` | `data_movement.cdc_migration_replay_and_change_capture` |
| Feature Name | CDC, migration, replay, and change capture | CDC, migration, replay, and change capture |
| Citus State | `DONOR_CHANGE_CAPTURE_OR_MIGRATION_SURFACE` | `DONOR_CHANGE_CAPTURE_OR_MIGRATION_SURFACE` |
| ScratchBird State | `CDC_AND_MIGRATION_SUBSTRATE` | `CDC_AND_MIGRATION_SUBSTRATE` |
| Outcome | `PARTIAL_PARITY` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `CURRENT_GAP` | `MAPPING_REQUIRED` |
| Evidence | `SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare change capture, migration, replay, and commit-ordered export surfaces. Donor baseline is `DONOR_CHANGE_CAPTURE_OR_MIGRATION_SURFACE`. ScratchBird current lane is `CDC_AND_MIGRATION_SUBSTRATE`. The donor surface is broader than the currently proven ScratchBird lane for this row. | Compare change capture, migration, replay, and commit-ordered export surfaces. Donor baseline is `DONOR_CHANGE_CAPTURE_OR_MIGRATION_SURFACE`. ScratchBird specified target lane is `CDC_AND_MIGRATION_SUBSTRATE`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/29_Listener_and_Server_Orchestration/PROXY_MIGRATION_FOR_NON_REPLICATING_DONORS_MODEL.md`<br>`docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/README.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/29_Listener_and_Server_Orchestration/PROXY_MIGRATION_FOR_NON_REPLICATING_DONORS_MODEL.md`<br>`docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/README.md` |

## 15. Replication, Distribution, and Availability

### `replication.cluster_identity_fencing_and_routing_epoch` Cluster identity, fencing, and routing epoch

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `replication.cluster_identity_fencing_and_routing_epoch` | `replication.cluster_identity_fencing_and_routing_epoch` |
| Feature Name | Cluster identity, fencing, and routing epoch | Cluster identity, fencing, and routing epoch |
| Citus State | `DONOR_CLUSTER_OR_ROUTING_SURFACE` | `DONOR_CLUSTER_OR_ROUTING_SURFACE` |
| ScratchBird State | `CLUSTER_IDENTITY_FENCING_AND_ROUTING_EPOCH` | `CLUSTER_IDENTITY_FENCING_AND_ROUTING_EPOCH` |
| Outcome | `COMPATIBLE_WITH_MAPPING` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `MAPPING_REQUIRED` | `MAPPING_REQUIRED` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare cluster identity, routing epoch, write fencing, and refusal visibility. Donor baseline is `DONOR_CLUSTER_OR_ROUTING_SURFACE`. ScratchBird current lane is `CLUSTER_IDENTITY_FENCING_AND_ROUTING_EPOCH`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. | Compare cluster identity, routing epoch, write fencing, and refusal visibility. Donor baseline is `DONOR_CLUSTER_OR_ROUTING_SURFACE`. ScratchBird specified target lane is `CLUSTER_IDENTITY_FENCING_AND_ROUTING_EPOCH`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_IDENTITY_SECURITY_QUORUM_AND_WRITE_FENCE_OBSERVABILITY_MODEL.md`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_ROUTING_FENCING_AND_SHARD_COMMIT_LOG_OBSERVABILITY.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_IDENTITY_SECURITY_QUORUM_AND_WRITE_FENCE_OBSERVABILITY_MODEL.md`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_ROUTING_FENCING_AND_SHARD_COMMIT_LOG_OBSERVABILITY.md` |

### `replication.sharding_partitioning_and_topology_control` Sharding, partitioning, and topology control

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `replication.sharding_partitioning_and_topology_control` | `replication.sharding_partitioning_and_topology_control` |
| Feature Name | Sharding, partitioning, and topology control | Sharding, partitioning, and topology control |
| Citus State | `DONOR_TOPOLOGY_CONTROL_SURFACE` | `DONOR_TOPOLOGY_CONTROL_SURFACE` |
| ScratchBird State | `PARTIAL_TOPOLOGY_AND_SHARDING_MODEL` | `CANONICAL_TOPOLOGY_AND_SHARDING_MODEL` |
| Outcome | `PARTIAL_PARITY` | `COMPATIBLE_WITH_MAPPING` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `CURRENT_GAP` | `MAPPING_REQUIRED` |
| Evidence | `SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare sharding, partitioning, placement, and topology-management surfaces. Donor baseline is `DONOR_TOPOLOGY_CONTROL_SURFACE`. ScratchBird current lane is `PARTIAL_TOPOLOGY_AND_SHARDING_MODEL`. The donor surface is broader than the currently proven ScratchBird lane for this row. | Compare sharding, partitioning, placement, and topology-management surfaces. Donor baseline is `DONOR_TOPOLOGY_CONTROL_SURFACE`. ScratchBird specified target lane is `CANONICAL_TOPOLOGY_AND_SHARDING_MODEL`. This row depends on explicit mapping or overlay behavior rather than one-for-one architecture. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SHARDING_CATALOG_SCHEMA.md`<br>`docs/specifications/41_Platform_Interface_and_Lifecycle_Management/ENGINE_PROFILE_ENUM_FREEZE_AND_RUNTIME_CAPABILITY_CONTRACT.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SHARDING_CATALOG_SCHEMA.md`<br>`docs/specifications/41_Platform_Interface_and_Lifecycle_Management/ENGINE_PROFILE_ENUM_FREEZE_AND_RUNTIME_CAPABILITY_CONTRACT.md` |

### `replication.apply_lag_failover_and_observability` Apply lag, failover, and observability

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `replication.apply_lag_failover_and_observability` | `replication.apply_lag_failover_and_observability` |
| Feature Name | Apply lag, failover, and observability | Apply lag, failover, and observability |
| Citus State | `DONOR_REPLICATION_AND_FAILOVER_SURFACE` | `DONOR_REPLICATION_AND_FAILOVER_SURFACE` |
| ScratchBird State | `PARTIAL_APPLY_AND_FAILOVER_OBSERVABILITY` | `CANONICAL_APPLY_AND_FAILOVER_MODEL` |
| Outcome | `PARTIAL_PARITY` | `PARTIAL_PARITY` |
| Delta Class | `NO_DIRECTIONAL_DELTA` | `NO_DIRECTIONAL_DELTA` |
| Blocking Class | `CURRENT_GAP` | `TARGET_GAP` |
| Evidence | `CODE_AND_SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare replication apply, failover, lag, and operator-visible availability diagnostics. Donor baseline is `DONOR_REPLICATION_AND_FAILOVER_SURFACE`. ScratchBird current lane is `PARTIAL_APPLY_AND_FAILOVER_OBSERVABILITY`. The donor surface is broader than the currently proven ScratchBird lane for this row. | Compare replication apply, failover, lag, and operator-visible availability diagnostics. Donor baseline is `DONOR_REPLICATION_AND_FAILOVER_SURFACE`. ScratchBird specified target lane is `CANONICAL_APPLY_AND_FAILOVER_MODEL`. The donor surface remains broader than the currently specified ScratchBird target lane for this row. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_ROUTING_FENCING_AND_SHARD_COMMIT_LOG_OBSERVABILITY.md`<br>`docs/specifications/29_Listener_and_Server_Orchestration/NORMATIVE_LISTENER_ONE_WAY_AND_BIDIRECTIONAL_REPLICATION_CHECKLIST.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/CLUSTER_ROUTING_FENCING_AND_SHARD_COMMIT_LOG_OBSERVABILITY.md`<br>`docs/specifications/29_Listener_and_Server_Orchestration/NORMATIVE_LISTENER_ONE_WAY_AND_BIDIRECTIONAL_REPLICATION_CHECKLIST.md` |

## 16. Specialized and Non-Relational Surface Areas

### `specialized.document_surface` Document surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `specialized.document_surface` | `specialized.document_surface` |
| Feature Name | Document surface | Document surface |
| Citus State | `ABSENT` | `ABSENT` |
| ScratchBird State | `PARTIAL_DOCUMENT_SUBSTRATE` | `PARTIAL_DOCUMENT_SUBSTRATE` |
| Outcome | `SUBJECT_B_ONLY` | `SUBJECT_B_ONLY` |
| Delta Class | `SUBJECT_B_SUPERSET` | `SUBJECT_B_SUPERSET` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare document-native storage, nested-path behavior, and document-query or aggregation surfaces. Donor baseline is `ABSENT`. ScratchBird current lane is `PARTIAL_DOCUMENT_SUBSTRATE`. | Compare document-native storage, nested-path behavior, and document-query or aggregation surfaces. Donor baseline is `ABSENT`. ScratchBird specified target lane is `PARTIAL_DOCUMENT_SUBSTRATE`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_ANALYSIS_MONGODB.md`<br>`docs/specifications/15_Complex_Types/EMULATED_COMPLEX_TYPE_MATRIX.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_ANALYSIS_MONGODB.md`<br>`docs/specifications/15_Complex_Types/EMULATED_COMPLEX_TYPE_MATRIX.md` |

### `specialized.graph_surface` Graph surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `specialized.graph_surface` | `specialized.graph_surface` |
| Feature Name | Graph surface | Graph surface |
| Citus State | `ABSENT` | `ABSENT` |
| ScratchBird State | `PARTIAL_GRAPH_SUBSTRATE` | `PARTIAL_GRAPH_SUBSTRATE` |
| Outcome | `SUBJECT_B_ONLY` | `SUBJECT_B_ONLY` |
| Delta Class | `SUBJECT_B_SUPERSET` | `SUBJECT_B_SUPERSET` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare graph-native modeling, traversal, path, and graph-query surfaces. Donor baseline is `ABSENT`. ScratchBird current lane is `PARTIAL_GRAPH_SUBSTRATE`. | Compare graph-native modeling, traversal, path, and graph-query surfaces. Donor baseline is `ABSENT`. ScratchBird specified target lane is `PARTIAL_GRAPH_SUBSTRATE`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/NEO4J_TEXT_SPEC.md`<br>`docs/specifications/18_Index_Framework/NEO4J_RANGE_SPEC.md`<br>`docs/specifications/18_Index_Framework/NEO4J_VECTOR_SPEC.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/NEO4J_TEXT_SPEC.md`<br>`docs/specifications/18_Index_Framework/NEO4J_RANGE_SPEC.md`<br>`docs/specifications/18_Index_Framework/NEO4J_VECTOR_SPEC.md` |

### `specialized.vector_ann_surface` Vector and ANN surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `specialized.vector_ann_surface` | `specialized.vector_ann_surface` |
| Feature Name | Vector and ANN surface | Vector and ANN surface |
| Citus State | `ABSENT` | `ABSENT` |
| ScratchBird State | `PARTIAL_VECTOR_AND_ANN_SUBSTRATE` | `PARTIAL_VECTOR_AND_ANN_SUBSTRATE` |
| Outcome | `SUBJECT_B_ONLY` | `SUBJECT_B_ONLY` |
| Delta Class | `SUBJECT_B_SUPERSET` | `SUBJECT_B_SUPERSET` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare vector storage, ANN indexing, and similarity-search surfaces. Donor baseline is `ABSENT`. ScratchBird current lane is `PARTIAL_VECTOR_AND_ANN_SUBSTRATE`. | Compare vector storage, ANN indexing, and similarity-search surfaces. Donor baseline is `ABSENT`. ScratchBird specified target lane is `PARTIAL_VECTOR_AND_ANN_SUBSTRATE`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/VECTOR_ANN_PLANNER_SPEC.md`<br>`docs/specifications/18_Index_Framework/IVF_VARIANTS_SPEC.md`<br>`docs/specifications/18_Index_Framework/HNSW_SPEC.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/VECTOR_ANN_PLANNER_SPEC.md`<br>`docs/specifications/18_Index_Framework/IVF_VARIANTS_SPEC.md`<br>`docs/specifications/18_Index_Framework/HNSW_SPEC.md` |

### `specialized.key_value_and_data_structure_surface` Key-value and data-structure surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `specialized.key_value_and_data_structure_surface` | `specialized.key_value_and_data_structure_surface` |
| Feature Name | Key-value and data-structure surface | Key-value and data-structure surface |
| Citus State | `ABSENT` | `ABSENT` |
| ScratchBird State | `PARTIAL_KEY_VALUE_SUBSTRATE` | `PARTIAL_KEY_VALUE_SUBSTRATE` |
| Outcome | `SUBJECT_B_ONLY` | `SUBJECT_B_ONLY` |
| Delta Class | `SUBJECT_B_SUPERSET` | `SUBJECT_B_SUPERSET` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare key-value commands, collection-like structures, and non-relational atomic mutation surfaces. Donor baseline is `ABSENT`. ScratchBird current lane is `PARTIAL_KEY_VALUE_SUBSTRATE`. | Compare key-value commands, collection-like structures, and non-relational atomic mutation surfaces. Donor baseline is `ABSENT`. ScratchBird specified target lane is `PARTIAL_KEY_VALUE_SUBSTRATE`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/REDIS_DATA_STRUCTURES_SPEC.md`<br>`docs/specifications/28_Parser_Implementations/NORMATIVE_WIRE_PROTOCOL_REDIS_RESP_CHECKLIST.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/18_Index_Framework/REDIS_DATA_STRUCTURES_SPEC.md`<br>`docs/specifications/28_Parser_Implementations/NORMATIVE_WIRE_PROTOCOL_REDIS_RESP_CHECKLIST.md` |

### `specialized.time_series_surface` Time-series surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `specialized.time_series_surface` | `specialized.time_series_surface` |
| Feature Name | Time-series surface | Time-series surface |
| Citus State | `ABSENT` | `ABSENT` |
| ScratchBird State | `PARTIAL_TIME_SERIES_SUBSTRATE` | `PARTIAL_TIME_SERIES_SUBSTRATE` |
| Outcome | `SUBJECT_B_ONLY` | `SUBJECT_B_ONLY` |
| Delta Class | `SUBJECT_B_SUPERSET` | `SUBJECT_B_SUPERSET` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `SPEC_BACKED` | `SPEC_BACKED` |
| Notes or Mapping | Compare time-series writes, retention, windows, and downsampling-visible behavior. Donor baseline is `ABSENT`. ScratchBird current lane is `PARTIAL_TIME_SERIES_SUBSTRATE`. | Compare time-series writes, retention, windows, and downsampling-visible behavior. Donor baseline is `ABSENT`. ScratchBird specified target lane is `PARTIAL_TIME_SERIES_SUBSTRATE`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/36_Query_Rewrite_and_Planner/README.md`<br>`docs/specifications/15_Complex_Types/README.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/36_Query_Rewrite_and_Planner/README.md`<br>`docs/specifications/15_Complex_Types/README.md` |

### `specialized.history_temporal_and_lineage_query_surface` History, temporal, and lineage-query surface

| Row Heading | Current State | Target State |
| --- | --- | --- |
| Assessment Lane | `CURRENT_STATE` | `SPECIFIED_TARGET_STATE` |
| Feature Key | `specialized.history_temporal_and_lineage_query_surface` | `specialized.history_temporal_and_lineage_query_surface` |
| Feature Name | History, temporal, and lineage-query surface | History, temporal, and lineage-query surface |
| Citus State | `ABSENT` | `ABSENT` |
| ScratchBird State | `RETAINED_LINEAGE_AND_TEMPORAL_SUBSTRATE` | `RETAINED_LINEAGE_AND_TEMPORAL_SUBSTRATE` |
| Outcome | `SUBJECT_B_ONLY` | `SUBJECT_B_ONLY` |
| Delta Class | `SUBJECT_B_SUPERSET` | `SUBJECT_B_SUPERSET` |
| Blocking Class | `NONE` | `NONE` |
| Evidence | `CODE_AND_SPEC_BACKED` | `CODE_AND_SPEC_BACKED` |
| Notes or Mapping | Compare temporal queryability, retained history, lineage inspection, and immutable-history style behavior. Donor baseline is `ABSENT`. ScratchBird current lane is `RETAINED_LINEAGE_AND_TEMPORAL_SUBSTRATE`. | Compare temporal queryability, retained history, lineage inspection, and immutable-history style behavior. Donor baseline is `ABSENT`. ScratchBird specified target lane is `RETAINED_LINEAGE_AND_TEMPORAL_SUBSTRATE`. |
| Source References | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/LOGICAL_ROW_UUID_CLUSTER_TRACKING_AND_VERSION_LINEAGE_OBSERVABILITY_MODEL.md`<br>`docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md` | `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`<br>`docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`<br>`docs/specifications/20_Diagnostics_Audit_and_Observability/LOGICAL_ROW_UUID_CLUSTER_TRACKING_AND_VERSION_LINEAGE_OBSERVABILITY_MODEL.md`<br>`docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md` |

## 17. Limitations, Exclusions, and Non-Comparable Areas

- This report keeps environment-level ScratchBird rows visible even when the donor has no equivalent surface.
- Direct donor-tool and upstream harness rows are intentionally conservative. Current bounded evidence exists for direct donor lanes only where cited in the row references.
- `SPECIFIED_TARGET_STATE` counts only behavior already defined in authoritative ScratchBird specifications. Desirable but unspec'ed work is not scored.

## 18. Optional Mapping or Emulation Strategy

- Primary follow-on hard-gap set for this donor: `replication.apply_lag_failover_and_observability`.
- Mapping-intensive rows for this donor: `architecture.storage_engine_toast_and_oversized_value_model, architecture.cluster_ready_identity_and_retained_replication_model, catalog.uuid_identity_and_row_lineage_keys, catalog.metadata_publication_and_schema_epoch_rules, types.native_scalar_and_temporal_surface, types.complex_type_and_specialized_value_surface, types.custom_domains_and_user_defined_type_control_plane, ddl.object_lifecycle_and_dependency_publication, dml.query_and_mutation_surface, planner.path_taxonomy_plan_control_and_explain_surface, indexes.runtime_family_surface, indexes.donor_index_mapping_and_alias_lowering`.
- For direct donor families, prioritize catalog-overlay, schema-root, type-mapping, and front-door rows before widening ecosystem tooling claims.
- For non-direct donor families, treat environment-boundary rows as design inputs rather than as evidence of existing donor parity.

## 19. Final Assessment

### CURRENT_STATE

Verdict: `NOT_CURRENTLY_FEASIBLE`. Current hard gaps are driven by `catalog.donor_catalog_overlay_and_system_table_emulation, catalog.system_views_and_statistics_surface, types.donor_type_mapping_and_emulation_surface, ddl.donor_schema_mapping_and_search_path_behavior, dml.donor_query_semantics_emulation_surface, planner.path_taxonomy_plan_control_and_explain_surface, protocol.dedicated_donor_front_door, protocol.prepared_parameter_error_and_result_mapping, data_movement.cdc_migration_replay_and_change_capture, replication.sharding_partitioning_and_topology_control, replication.apply_lag_failover_and_observability`. Mapping-required rows are `architecture.storage_engine_toast_and_oversized_value_model, architecture.cluster_ready_identity_and_retained_replication_model, catalog.uuid_identity_and_row_lineage_keys, catalog.metadata_publication_and_schema_epoch_rules, types.native_scalar_and_temporal_surface, types.complex_type_and_specialized_value_surface, types.custom_domains_and_user_defined_type_control_plane, ddl.object_lifecycle_and_dependency_publication, dml.query_and_mutation_surface, indexes.runtime_family_surface, indexes.donor_index_mapping_and_alias_lowering, indexes.specialized_index_families`.

### SPECIFIED_TARGET_STATE

Verdict: `PARTIAL_WITH_GAPS`. Target-state hard gaps are driven by `replication.apply_lag_failover_and_observability`. Mapping-required rows are `architecture.storage_engine_toast_and_oversized_value_model, architecture.cluster_ready_identity_and_retained_replication_model, catalog.uuid_identity_and_row_lineage_keys, catalog.metadata_publication_and_schema_epoch_rules, types.native_scalar_and_temporal_surface, types.complex_type_and_specialized_value_surface, types.custom_domains_and_user_defined_type_control_plane, ddl.object_lifecycle_and_dependency_publication, dml.query_and_mutation_surface, planner.path_taxonomy_plan_control_and_explain_surface, indexes.runtime_family_surface, indexes.donor_index_mapping_and_alias_lowering`.

## 20. Evidence and Sources

- donor control matrix: `docs/specifications/Reference_Documentation_specification.md`
- donor formal reference pack: `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/README.md`, `docs/reference/formal_reference_set/21_40_beta2_design_and_method_set/39_citus/SOURCE.txt`
- comparison schema authority: `docs/specifications/31_Conformance_Performance_and_Reliability_Gates/CROSS_ENGINE_FEATURE_COMPARISON_REPORT_SCHEMA_AND_PAIRWISE_COMPARABILITY_MODEL.md`
- ScratchBird canonical anchors: `docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/UUID_IDENTITY_AND_COLLISION_RULES.md`; `docs/specifications/11_TOAST_and_LOB_Storage/README.md`; `docs/specifications/15_Complex_Types/README.md`; `docs/specifications/18_Index_Framework/README.md`; `docs/specifications/19_Security_Model/README.md`; `docs/specifications/20_Diagnostics_Audit_and_Observability/MGA_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md`; `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/README.md`; `docs/specifications/28_Parser_Implementations/README.md`; `docs/specifications/29_Listener_and_Server_Orchestration/README.md`; `docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md`
