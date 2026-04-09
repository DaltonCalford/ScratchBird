# Cross-Engine Feature Comparison Report Schema and Pairwise Comparability Model

Status: current_authority

## Purpose

This document defines the canonical structure, vocabulary, and comparison rules
for pairwise database-platform comparison reports.

It standardizes the legacy comparison-report family created on November 23,
2025 under `docs/audit/comparisons/November_2025/` so future reports can
compare:

- ScratchBird against another database product
- another database product against ScratchBird
- peer database products against one another

The canonical report family is intended to support:

- feature-to-feature emulation analysis
- follow-on ScratchBird specification drafting
- gap reports and work decomposition
- comparison of current behavior against spec-defined planned work

## Scope

This specification governs narrative feature comparison reports rendered as
Markdown, PDF, HTML, or equivalent report views.

It applies to reports that answer questions such as:

- whether product `A` can emulate or replace product `B` for a defined scope
- where two products are functionally equivalent despite architectural
  differences
- where one product exposes a broader or narrower surface than another
- which gaps are current blockers, which are target-state gaps, and which are
  deliberate non-goals

It does not govern:

- single-product project status reports
- raw benchmark result rows
- protocol-only throughput or latency reports
- row-level result diff classification for replay

## ScratchBird Interpretation Rule

When ScratchBird is one subject, the report shall treat it as a database
environment, not as a monolithic SQL engine.

At minimum, the report shall keep the following boundaries explicit:

- ScratchBird engine executes SBLR and internal procedures; it is not the SQL
  parser authority
- parser families are separate front-door components that translate donor SQL
  or protocol requests into SBLR and translate replies back to donor-facing
  formats
- emulated catalog and system-table surfaces may be exposed through virtual
  overlays rooted in ScratchBird canonical catalogs
- cluster, UUID, lineage, and derivative-lane behavior are part of the
  compared platform surface when they materially affect parity or gap analysis

It is non-conforming to flatten ScratchBird into "generic SQL engine parity"
and omit its environment-level surfaces when the report is intended to drive
new specifications or gap reports.

## Canonical Rule

Every cross-engine feature comparison report shall be modeled as five logical
components:

1. a normalized report header
2. a category scorecard
3. a per-feature comparison matrix organized by canonical taxonomy families
4. an evidence and provenance ledger
5. an optional intent-specific verdict layer

The per-feature matrix is the authoritative factual layer.

Executive summaries, conclusions, feasibility claims, and roadmap text are
derivative views over that matrix and shall not contradict it.

For `EMULATION_FEASIBILITY` reports used to drive specifications or gap
reports, the matrix shall be full-surface and feature-to-feature. Family-only
summaries are not sufficient.

## Comparison Subject Model

Every report shall identify two explicit subjects:

- `subject_a`
- `subject_b`

Every report shall also declare one `comparison_intent`:

- `PAIRWISE_NEUTRAL`
- `EMULATION_FEASIBILITY`
- `REPLACEMENT_FEASIBILITY`
- `DELTA_REFRESH`

Rules:

1. If the intent is `PAIRWISE_NEUTRAL`, subject order shall be lexical by
   canonical engine key.
2. If the intent is asymmetric, `subject_a` is the reference surface and
   `subject_b` is the implementation candidate or evaluator.
3. Even when the intent is asymmetric, the per-feature rows shall record both
   subject states explicitly.
4. When ScratchBird is one subject, the report shall additionally record
   `scratchbird_role` as one of:
   - `REFERENCE_SURFACE`
   - `IMPLEMENTATION_CANDIDATE`
   - `PEER_PLATFORM`

## Required Header Fields

Each report shall declare, at minimum:

- `report_id`
- `report_date`
- `report_family_version`
- `comparison_intent`
- `assessment_lanes`
- `scope_profile`
- `subject_a.engine_key`
- `subject_a.display_name`
- `subject_a.version_or_release`
- `subject_a.edition_or_variant`
- `subject_a.deployment_mode`
- `subject_b.engine_key`
- `subject_b.display_name`
- `subject_b.version_or_release`
- `subject_b.edition_or_variant`
- `subject_b.deployment_mode`
- `evidence_cutoff_date`
- `generator_or_author`
- `excluded_domains`
- `source_pack`

## Scope Profile Contract

The `scope_profile` shall state the comparison boundary explicitly enough that
the report can be reused honestly in later pairwise analysis.

It shall include:

- deployment mode such as `embedded`, `server`, `wire_protocol`, `api`, or
  `mixed`
- whether network functionality is in scope
- whether the report evaluates `CURRENT_STATE`, `SPECIFIED_TARGET_STATE`, or
  both
- whether ScratchBird environment-level surfaces are included
- which major capability families are excluded

Rules:

1. If the report intent is `EMULATION_FEASIBILITY` and the artifact is
   intended to drive new specifications or gap reports, the default scope
   shall be the full canonical comparison surface defined in this file.
2. If any canonical feature family is excluded, the exclusion shall be named in
   `scope_profile` and repeated in the limitations section.

## Assessment Lane Contract

Every report shall declare one or both assessment lanes:

- `CURRENT_STATE`
- `SPECIFIED_TARGET_STATE`

Rules:

1. `CURRENT_STATE` covers behavior that exists now within the scoped product
   surface.
2. `SPECIFIED_TARGET_STATE` covers planned or in-progress behavior that is
   already defined in an authoritative specification set and is therefore
   intentionally comparable.
3. A report may include both lanes, but it shall keep them separate in tables,
   scorecards, and final verdicts.
4. Planned work defined in authoritative specifications is valid comparison
   material for the `SPECIFIED_TARGET_STATE` lane and shall not be mislabeled
   as current-state parity.

## Required Report Section Order

Canonical reports shall preserve this section order:

1. purpose and scope
2. executive summary
3. subject identity and scope assumptions
4. overall category scorecard
5. architecture, storage, and transaction core
6. catalog, metadata, and object identity
7. types and value semantics
8. DDL and schema lifecycle
9. DML, query, and planner surface
10. indexing and access paths
11. callable, procedural, and trigger surface
12. security, identity, and policy surface
13. parser, protocol, and emulation front door
14. operations, introspection, and data movement
15. replication, distribution, and availability
16. specialized and non-relational surface areas
17. limitations, exclusions, and non-comparable areas
18. optional mapping or emulation strategy
19. final assessment
20. evidence and sources

Rules:

1. A section may be marked `OUT_OF_SCOPE`, but shall not be silently omitted.
2. Additional sections may be inserted only if they are clearly labeled as
   extensions.
3. Any conclusion about feasibility or replacement shall appear after the
   factual comparison sections, not before them.

## Canonical Taxonomy Families

Every feature row shall belong to one top-level `family_key`.

The minimum supported top-level family set is:

- `architecture`
- `catalog`
- `types`
- `ddl`
- `dml`
- `planner`
- `indexes`
- `functions`
- `procedural`
- `security`
- `parser`
- `protocol`
- `operations`
- `data_movement`
- `replication`
- `specialized`

Feature keys shall be engine-neutral, lowercase, and dot-delimited so rows
from one report can be joined against rows from another report without
renaming.

## Canonical Full-Surface Matrix

`EMULATION_FEASIBILITY` reports intended for specification or gap-report use
shall include at least the following feature keys:

| Section | Feature key |
| --- | --- |
| architecture | `architecture.product_model_and_execution_boundary` |
| architecture | `architecture.storage_engine_toast_and_oversized_value_model` |
| architecture | `architecture.transaction_context_lifecycle` |
| architecture | `architecture.non_destructive_version_lineage` |
| architecture | `architecture.recovery_truth_and_derivative_lanes` |
| architecture | `architecture.cluster_ready_identity_and_retained_replication_model` |
| catalog | `catalog.uuid_identity_and_row_lineage_keys` |
| catalog | `catalog.recursive_schema_tree_and_schema_root_sandboxing` |
| catalog | `catalog.donor_catalog_overlay_and_system_table_emulation` |
| catalog | `catalog.metadata_publication_and_schema_epoch_rules` |
| catalog | `catalog.system_views_and_statistics_surface` |
| types | `types.native_scalar_and_temporal_surface` |
| types | `types.complex_type_and_specialized_value_surface` |
| types | `types.custom_domains_and_user_defined_type_control_plane` |
| types | `types.domain_security_masking_and_encryption_metadata` |
| types | `types.donor_type_mapping_and_emulation_surface` |
| ddl | `ddl.object_lifecycle_and_dependency_publication` |
| ddl | `ddl.donor_schema_mapping_and_search_path_behavior` |
| dml | `dml.query_and_mutation_surface` |
| dml | `dml.donor_query_semantics_emulation_surface` |
| planner | `planner.path_taxonomy_plan_control_and_explain_surface` |
| indexes | `indexes.runtime_family_surface` |
| indexes | `indexes.donor_index_mapping_and_alias_lowering` |
| indexes | `indexes.specialized_index_families` |
| indexes | `indexes.mga_safe_publication_reclaim_and_metrics` |
| functions | `functions.builtin_function_and_operator_surface` |
| procedural | `procedural.server_side_code_trigger_and_routine_surface` |
| procedural | `procedural.sblr_bytecode_runtime_and_compilation_lane` |
| security | `security.identity_authentication_and_provider_chain` |
| security | `security.row_column_domain_masking_and_policy_pipeline` |
| security | `security.definer_invoker_and_schema_sandbox_security` |
| parser | `parser.engine_parser_separation_and_sblr_lowering` |
| protocol | `protocol.dedicated_donor_front_door` |
| protocol | `protocol.prepared_parameter_error_and_result_mapping` |
| protocol | `protocol.donor_tool_and_upstream_harness_compatibility` |
| operations | `operations.runtime_modes_operator_views_and_utilities` |
| operations | `operations.mga_diagnostics_and_introspection` |
| data_movement | `data_movement.backup_restore_bulk_import_export` |
| data_movement | `data_movement.cdc_migration_replay_and_change_capture` |
| replication | `replication.cluster_identity_fencing_and_routing_epoch` |
| replication | `replication.sharding_partitioning_and_topology_control` |
| replication | `replication.apply_lag_failover_and_observability` |
| specialized | `specialized.document_surface` |
| specialized | `specialized.graph_surface` |
| specialized | `specialized.vector_ann_surface` |
| specialized | `specialized.key_value_and_data_structure_surface` |
| specialized | `specialized.time_series_surface` |
| specialized | `specialized.history_temporal_and_lineage_query_surface` |

Rules:

1. All rows above shall appear in every full-surface emulation report.
2. A row may be scored as `OUT_OF_SCOPE`, `SUBJECT_A_ONLY`, or
   `NOT_COMPARABLE`, but it shall remain visible.
3. Additional feature rows are allowed when a donor exposes important behavior
   beyond the minimum matrix.
4. The canonical matrix may be extended, but rows shall not be renamed or
   repurposed incompatibly.

## Per-Feature Row Contract

Each comparison row shall carry the following logical fields:

- `assessment_lane`
- `feature_key`
- `feature_name`
- `family_key`
- `subject_a_state`
- `subject_b_state`
- `comparison_outcome`
- `delta_class`
- `blocking_class`
- `evidence_grade`
- `notes_or_mapping`
- `source_refs`

The storage format is implementation-defined. The row may appear in a rendered
table, embedded structured data, or a machine-readable export, but the logical
fields above are mandatory.

## Rendered Narrow-Matrix Contract

Markdown renderings shall default to a narrow three-column matrix per feature:

- `Row Heading`
- `Current State`
- `Target State`

The row labels in that rendered table shall be:

- `Assessment Lane`
- `Feature Key`
- `Feature Name`
- `<Subject A Display Name> State`
- `<Subject B Display Name> State`
- `Outcome`
- `Delta Class`
- `Blocking Class`
- `Evidence`
- `Notes or Mapping`
- `Source References`

Rules:

1. The rendered table shall use the real subject display names, not
   placeholders such as `Subject A`.
2. If one subject is ScratchBird, the rendered row label should use
   `ScratchBird State` for that side.
3. The two value columns shall represent `CURRENT_STATE` and
   `SPECIFIED_TARGET_STATE` respectively when both lanes are present.
4. Wide matrix renderings that place all logical fields in columns are allowed
   only as machine-oriented exports, not as the canonical reader-facing
   Markdown report view.

## Subject State Contract

`subject_a_state` and `subject_b_state` shall use stable uppercase tokens that
fit the compared feature surface.

Global fixed vocabulary is intentionally not required because environment-level
rows may need states such as:

- `DATABASE_ENGINE`
- `DATABASE_ENVIRONMENT`
- `SHIPPED_FRONT_DOOR`
- `SPECIFIED_FRONT_DOOR`
- `VIRTUAL_OVERLAY_CATALOG`
- `MGA_PRIMARY_DERIVATIVE_SECONDARY`
- `ALWAYS_IN_TRANSACTION`
- `PARTIAL`
- `ABSENT`
- `OUT_OF_SCOPE`

Rules:

1. State tokens shall be stable within a report family version.
2. The notes field shall make clear what the chosen state token means for the
   row when the token is not obvious from the feature name.
3. `OUT_OF_SCOPE` rows remain visible in the report and are excluded from
   parity counts.

## Comparison Outcome Vocabulary

`comparison_outcome` shall use one of:

- `IDENTICAL`
- `FUNCTIONALLY_EQUIVALENT`
- `COMPATIBLE_WITH_MAPPING`
- `EMULATABLE_WITHOUT_MATERIAL_LOSS`
- `PARTIAL_PARITY`
- `SUBJECT_A_ONLY`
- `SUBJECT_B_ONLY`
- `NOT_COMPARABLE`

Rules:

1. `IDENTICAL` is reserved for materially matching externally visible behavior
   within scope.
2. `FUNCTIONALLY_EQUIVALENT` is valid when internals differ but application-
   visible behavior is equivalent for the scoped surface.
3. `COMPATIBLE_WITH_MAPPING` requires a concrete mapping rule or explicit
   environment-boundary note in `notes_or_mapping`.
4. `EMULATABLE_WITHOUT_MATERIAL_LOSS` may be used in `CURRENT_STATE` only when
   the required emulation layer exists now, and may be used in
   `SPECIFIED_TARGET_STATE` when that layer is authoritatively specified.
5. `PARTIAL_PARITY` requires the missing sub-surface to be named explicitly.
6. `SUBJECT_A_ONLY` and `SUBJECT_B_ONLY` describe directional presence, not
   quality judgment.

## Delta and Blocking Vocabulary

`delta_class` shall use one of:

- `NO_DIRECTIONAL_DELTA`
- `ARCHITECTURALLY_DIFFERENT`
- `SUBJECT_A_SUPERSET`
- `SUBJECT_B_SUPERSET`
- `SCOPE_EXCLUDED`

`blocking_class` shall use one of:

- `NONE`
- `MAPPING_REQUIRED`
- `CURRENT_GAP`
- `TARGET_GAP`
- `NON_GOAL`
- `UNKNOWN`

Directional advantage and blocking status are separate dimensions.

Rules:

1. `MAPPING_REQUIRED` marks translation, overlay, or donor-surface adaptation
   work and shall not be counted as a hard blocker by itself.
2. `CURRENT_GAP`, `TARGET_GAP`, and `UNKNOWN` are blocker classes that may
   drive infeasibility verdicts.
3. `NON_GOAL` excludes the row from feasibility-blocker rollups even though it
   remains visible in the matrix.

## Evidence Grade Contract

Each feature row shall declare one `evidence_grade`:

- `RUNTIME_HARNESS_VERIFIED`
- `RUNTIME_VERIFIED`
- `IMPLEMENTATION_BACKED`
- `CODE_AND_SPEC_BACKED`
- `SPEC_BACKED`
- `REFERENCE_PACK_BACKED`
- `VENDOR_DOC_BACKED`
- `SECONDARY_DOC_ONLY`
- `PLANNED_ONLY`

Rules:

1. ScratchBird current-state claims shall rely on canonical specs, live
   implementation evidence, or executed tests.
2. Donor-tool compatibility and upstream harness claims shall use
   `RUNTIME_HARNESS_VERIFIED`, `RUNTIME_VERIFIED`, or a weaker explicit grade;
   they shall not be silently promoted from checklist prose.
3. `SPEC_BACKED` rows may support `SPECIFIED_TARGET_STATE` comparison even when
   implementation is incomplete.
4. `PLANNED_ONLY` rows shall not contribute to current-state or target-state
   full-parity counts.

## Scorecard Contract

Every report shall include a category scorecard that summarizes, per lane:

- in-scope feature count
- count of `IDENTICAL`
- count of `FUNCTIONALLY_EQUIVALENT`
- count of `COMPATIBLE_WITH_MAPPING`
- count of `EMULATABLE_WITHOUT_MATERIAL_LOSS`
- count of `PARTIAL_PARITY`
- count of directional-only rows
- blocker count
- roadmap-only row count

Rules:

1. The denominator shall exclude `OUT_OF_SCOPE` rows.
2. If both assessment lanes are present, the scorecard shall emit separate
   `CURRENT_STATE` and `SPECIFIED_TARGET_STATE` summaries.
3. Roadmap-only rows shall be counted separately from scored parity in both
   lanes.
4. `blocker count` shall count only rows whose `blocking_class` is
   `CURRENT_GAP`, `TARGET_GAP`, or `UNKNOWN`.
5. Rows marked `MAPPING_REQUIRED` remain visible through the
   `COMPATIBLE_WITH_MAPPING` outcome count and may be summarized separately,
   but they shall not inflate the blocker count.

## Final Assessment Contract

For `EMULATION_FEASIBILITY` or `REPLACEMENT_FEASIBILITY`, the report shall
publish one verdict per lane:

- `NO_BLOCKERS`
- `FEASIBLE_WITH_MAPPING`
- `PARTIAL_WITH_GAPS`
- `NOT_CURRENTLY_FEASIBLE`

Rules:

1. Current-state and target-state verdicts shall not be merged into one
   unlabeled sentence.
2. Every verdict shall derive from row-level blocking classes and shall name
   the hard-blocking feature keys or families explicitly.
3. If the report is intended to drive specification or gap-report work, the
   final assessment shall call out the blocker feature keys that seed follow-on
   work.
4. `MAPPING_REQUIRED` rows may be called out separately as integration or
   overlay work, but they shall not be presented as hard feasibility blockers
   unless paired with `CURRENT_GAP`, `TARGET_GAP`, or `UNKNOWN`.

## Planned Emulation Closure Rule

When the donor is in the report's planned emulation set, parser-owned emulation
rows are subordinate detail rows, not independent hard-blocker seeds.

This rule applies in particular to rows such as:

- `catalog.donor_catalog_overlay_and_system_table_emulation`
- `catalog.system_views_and_statistics_surface`
- `types.donor_type_mapping_and_emulation_surface`
- `ddl.donor_schema_mapping_and_search_path_behavior`
- `dml.donor_query_semantics_emulation_surface`
- `protocol.dedicated_donor_front_door`
- `protocol.prepared_parameter_error_and_result_mapping`

Rules:

1. If the report already records current direct-family support or
   `SPECIFIED_TARGET_STATE` planned family support for that donor, the
   subordinate rows above shall remain visible but shall not each be promoted
   into separate hard feasibility blockers merely because the family proof is
   bounded or mapping-heavy.
2. In `CURRENT_STATE`, bounded shipped proof may still justify
   `COMPATIBLE_WITH_MAPPING` or `PARTIAL_PARITY`, but the resulting
   `blocking_class` shall default to `MAPPING_REQUIRED` rather than compound
   `CURRENT_GAP` unless the row exposes a non-parser-owned missing surface.
3. In `SPECIFIED_TARGET_STATE`, planned family support for the donor may score
   the subordinate rows as mapping work or as
   `EMULATABLE_WITHOUT_MATERIAL_LOSS`; those rows shall not each seed separate
   `TARGET_GAP` blockers unless the row exposes a non-parser-owned missing
   surface.
4. Donor-tool and upstream-harness closure remains an independent evidence row
   in `CURRENT_STATE` unless a stronger current artifact proves donor-tool
   execution.
5. For specification-matrix reports whose explicit purpose is to define planned
   support for a donor family, `SPECIFIED_TARGET_STATE` may score donor-tool
   compatibility as target-state emulatable when the family-owned front door,
   translation, and response-mapping contract is itself authoritatively
   specified.
6. Full donor-tool regression execution using the donor's own tools is
   stronger current-state evidence than parser checklists and should dominate
   current feasibility interpretation for the exercised donor surface.

## Evidence and Provenance Rules

Every report shall close with an evidence ledger that separates:

- donor reference packs and vendor documentation
- ScratchBird canonical specifications
- live implementation or test evidence
- bounded donor-tool or upstream harness evidence
- secondary commentary where used

Rules:

1. Rendered reports shall use stable repo-relative or public source
   identifiers, not workstation-specific absolute paths.
2. Every section summary shall be traceable to one or more `source_refs`
   entries used by the rows in that section.
3. Spec-backed planned work may appear in the `SPECIFIED_TARGET_STATE` lane and
   is comparable within that lane.
4. Future roadmap text that is not spec-backed shall be placed in the optional
   mapping or emulation strategy section and shall be labeled non-scoring.

## Naming and Rendering Rules

Rendered titles shall use:

- `<Subject A> vs <Subject B>: Cross-Engine Feature Comparison`

Suggested artifact stem:

- `<subject-a>_vs_<subject-b>_cross_engine_feature_comparison_<yyyy-mm-dd>`

Markdown renderings shall prefer the narrow per-feature matrix defined above so
reports remain readable in-source and in line-wrapped text viewers.

## Non-Guarantees

This file does not:

- define benchmark metric schemas
- define row-by-row result equivalence for replay
- guarantee that every possible donor has dedicated runtime proof
- authorize planned future work to count as current compatibility without
  explicit lane labeling
- replace dedicated protocol, migration, or certification gate specifications

## Cross-Section References

- `31_Conformance_Performance_and_Reliability_Gates/CANONICAL_EQUIVALENCE_MODEL_AND_DIFF_CLASSIFICATION.md`
- `31_Conformance_Performance_and_Reliability_Gates/EVIDENCE_ARTIFACTS_AND_REPLAY_REQUIREMENTS.md`
- `28_Parser_Implementations/README.md`
- `29_Listener_and_Server_Orchestration/README.md`
