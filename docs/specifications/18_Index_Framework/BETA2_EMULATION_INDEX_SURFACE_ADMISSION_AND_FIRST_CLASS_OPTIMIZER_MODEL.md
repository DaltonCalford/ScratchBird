Status: canonical_beta2_immediate_implementation

# Beta 2 Emulation Index Surface Admission and First-Class Optimizer Model

## Purpose

Define the Beta 2 index-family expansion required for full donor-engine
emulation and require every newly admitted surface to publish optimizer-grade
metrics so it participates as a first-class index family instead of a parser
or catalog-only compatibility label.

## Scope

This file is authoritative for:

- new named `CatalogManager::IndexType` and V3 AST admissions
- donor-visible index surfaces that lower onto existing ScratchBird runtimes
- true new runtime or major-variant index families
- required persisted metadata fields and family modes
- planner-family binding and required metrics packets
- `EXPLAIN`, `SHOW`, catalog, and wire-visible family identity
- create-time refusal and open-time validation rules

## Source Basis

This file is grounded in current ScratchBird code and the donor-source audit:

- ScratchBird:
  - `include/scratchbird/parser/ast_v3.h`
  - `src/parser/parser_v3.cpp`
  - `src/core/index_factory.cpp`
  - `src/optimizer/index_family_lowering.cpp`
  - `include/scratchbird/optimizer/statistics.h`
  - `src/optimizer/statistics_manager.cpp`
- donor engines:
  - ClickHouse `MergeTreeIndices.cpp`, `MergeTreeIndexSet.*`,
    `MergeTreeIndexBloomFilterText.*`, `MergeTreeIndexText.*`,
    `MergeTreeIndexHypothesis.cpp`, `MergeTreeIndexVectorSimilarity.*`
  - Milvus `client/index/common.go`
  - YugabyteDB `src/include/access/ybgin.h`,
    `src/backend/access/ybgin/ybginwrite.c`,
    `src/backend/access/ybgin/ybginget.c`
  - MongoDB `src/mongo/db/index_names.h`
  - MySQL `sql/sql_yacc.yy`
  - MariaDB `sql/sql_yacc.yy`
  - TiDB `pkg/parser/parser.y`
  - Vitess `go/vt/sqlparser/sql.y`
  - Apache Ignite
    `modules/indexing/.../H2IndexType.java`
  - CockroachDB `pkg/sql/sem/idxtype/idxtype.proto`

## Non-Negotiable Rules

1. Every newly admitted donor surface must become a real named-family identity
   in parser, catalog, planner, metrics, and observability paths.
2. Shared runtime lowering is allowed, but shared runtime lowering never
   authorizes silent family collapse.
3. No Beta 2 family may be accepted for DDL unless:
   - its family-mode metadata is persisted
   - its create-time validator is implemented
   - its metrics packet contract is implemented
   - its `EXPLAIN` rendering is donor-compatible
4. Every new family must publish a planner-visible metrics packet on the same
   path used by existing families. "Supported but no packet" is non-conforming.
5. Omitted packet rows from the audit are not authority. The donor-source files
   listed above are the controlling truth.

## Required Enum Additions

The following named families shall be added to both:

- `parser::ast_v3::IndexType`
- `CatalogManager::IndexType`

Required additions:

- `SPATIAL`
- `VECTOR`
- `COLUMNAR`
- `MONGODB_COLUMN`
- `YBGIN`
- `MILVUS_AUTOINDEX`
- `MILVUS_IVF_RABITQ`
- `MILVUS_IVF_HNSW`
- `MILVUS_GPU_IVF_FLAT`
- `MILVUS_GPU_IVF_PQ`
- `MILVUS_GPU_BRUTE_FORCE`
- `CLICKHOUSE_SET`
- `CLICKHOUSE_TOKENBF_V1`
- `CLICKHOUSE_SPARSE_GRAMS`
- `CLICKHOUSE_TEXT`
- `CLICKHOUSE_HYPOTHESIS`
- `CLICKHOUSE_VECTOR_SIMILARITY`

The enum names above are canonical and shall be used in:

- parser lookup tables
- `CREATE INDEX ... USING`
- catalog `index.index_type`
- plan payloads
- `SHOW INDEX`
- donor overlay catalogs and access-method views

## Family Admission Matrix

### `SPATIAL`

- donor engines:
  - `MySQL`
  - `MariaDB`
  - `TiDB`
  - `Vitess`
  - `Apache Ignite`
- class:
  - donor-visible alias surface over existing ScratchBird spatial runtime
- canonical lowering:
  - `physical_family=RTREE`
  - `planner_family=SPATIAL`
  - `metrics_type=GENERALIZED_SPATIAL`
  - `family_mode=GENERIC_SPATIAL_ALIAS`
- required persisted fields:
  - `coordinate_model`
  - `geometry_kind_mask`
  - `srid_policy`
  - `distance_support`
  - `exact_recheck_required`
- create-time rule:
  - create is accepted only for geometry or geography key expressions that
    lower to one supported RTREE opclass
- plan rendering rule:
  - donor-facing plans show `SPATIAL`
  - ScratchBird native plans may show `SPATIAL (RTREE)`

### `VECTOR`

- donor engines:
  - `MariaDB`
  - `TiDB`
  - `CockroachDB`
- class:
  - donor-visible alias surface over existing ANN runtimes
- canonical lowering:
  - `physical_family` resolves to one of:
    - `VECTOR_FLAT`
    - `HNSW`
    - `IVF`
  - `planner_family` resolves to:
    - `ANN_EXACT` for flat exact mode
    - `ANN_APPROX` for HNSW or IVF modes
  - `metrics_type=ANN`
  - `family_mode=GENERIC_VECTOR_ALIAS`
- required persisted fields:
  - `vector_dimension`
  - `distance_metric`
  - `resolved_runtime_family`
  - `resolved_runtime_mode`
  - `exact_or_approximate`
  - `accelerator_policy`
- create-time rule:
  - the user-visible `VECTOR` family is a donor contract; the resolved runtime
    family is persisted and validated at open time

### `COLUMNAR`

- donor engines:
  - `TiDB`
- class:
  - donor-visible alias surface over existing ScratchBird `COLUMNSTORE`
- canonical lowering:
  - `physical_family=COLUMNSTORE`
  - `planner_family=COLUMNAR`
  - `metrics_type=SUMMARY_CANDIDATE`
  - `family_mode=TIDB_COLUMNAR_ALIAS`
- required persisted fields:
  - `projection_layout`
  - `column_grouping`
  - `late_materialization_enabled`
  - `delta_lane_mode`
- create-time rule:
  - `COLUMNAR` is admitted only when the target key list and projection list can
    be mapped to the existing columnstore key and segment model

### `MONGODB_COLUMN`

- donor engines:
  - `MongoDB`
- class:
  - donor-visible catalog and DDL surface over `COLUMNSTORE`
- canonical lowering:
  - `physical_family=COLUMNSTORE`
  - `planner_family=COLUMNAR`
  - `metrics_type=SUMMARY_CANDIDATE`
  - `family_mode=MONGODB_COLUMNSTORE_ALIAS`
- required persisted fields:
  - `path_projection_mode`
  - `column_path_map`
  - `visibility_filter_mode`
  - `plan_label=columnstore`
- create-time rule:
  - the donor-visible `COLUMN` name is authoritative for overlay catalogs even
    when the implementation runtime is `COLUMNSTORE`

### `YBGIN`

- donor engines:
  - `YugabyteDB`
- class:
  - donor-visible major variant over the existing ScratchBird GIN substrate
- canonical lowering:
  - `physical_family=GIN`
  - `planner_family=GENERALIZED`
  - `metrics_type=GENERALIZED_SPATIAL`
  - `family_mode=YBGIN_ACCESS_METHOD`
- required persisted fields:
  - `yb_fast_update_enabled=false`
  - `yb_scan_capability_mask`
  - `yb_recheck_policy`
  - `yb_backfill_mode`
  - `yb_opclass_uuid`
- create-time rule:
  - `YBGIN` must refuse any mode that would require unsupported donor behavior
    such as hidden fast-update semantics that are not implemented
- optimizer rule:
  - `YBGIN` is first-class and must publish a native packet even while sharing
    the `GIN` runtime substrate

### `MILVUS_AUTOINDEX`

- donor engines:
  - `Milvus`
- class:
  - donor-visible ANN family with runtime chosen by policy
- canonical lowering:
  - `physical_family` resolves to one of:
    - `HNSW`
    - `IVF`
    - `SCANN`
    - `DISKANN`
  - `planner_family=ANN_APPROX`
  - `metrics_type=ANN`
  - `family_mode=MILVUS_AUTOINDEX`
- required persisted fields:
  - `resolved_runtime_family`
  - `selection_policy_version`
  - `selection_reason`
  - `candidate_runtime_set`

### `MILVUS_IVF_RABITQ`

- donor engines:
  - `Milvus`
- class:
  - donor-visible IVF variant over the existing IVF substrate
- canonical lowering:
  - `physical_family=IVF`
  - `planner_family=ANN_APPROX`
  - `metrics_type=ANN`
  - `family_mode=MILVUS_RABITQ`
- required persisted fields:
  - `quantizer_mode=RABITQ`
  - `nlist`
  - `nprobe_default`
  - `compression_profile`

### `MILVUS_IVF_HNSW`

- donor engines:
  - `Milvus`
- class:
  - donor-visible hybrid ANN family over the IVF substrate
- canonical lowering:
  - `physical_family=IVF`
  - `planner_family=ANN_APPROX`
  - `metrics_type=ANN`
  - `family_mode=MILVUS_IVF_HNSW`
- required persisted fields:
  - `coarse_quantizer_mode=HNSW`
  - `nlist`
  - `nprobe_default`
  - `graph_connectivity`
  - `graph_expansion_add`

### `MILVUS_GPU_IVF_FLAT`

- donor engines:
  - `Milvus`
- class:
  - donor-visible GPU IVF variant over the IVF substrate
- canonical lowering:
  - `physical_family=IVF`
  - `planner_family=ANN_APPROX`
  - `metrics_type=ANN`
  - `family_mode=MILVUS_GPU_IVF_FLAT`
- required persisted fields:
  - `accelerator_policy=gpu_required_or_preferred`
  - `gpu_search_layout=flat`
  - `nlist`
  - `nprobe_default`

### `MILVUS_GPU_IVF_PQ`

- donor engines:
  - `Milvus`
- class:
  - donor-visible GPU IVF-PQ variant over the IVF substrate
- canonical lowering:
  - `physical_family=IVF`
  - `planner_family=ANN_APPROX`
  - `metrics_type=ANN`
  - `family_mode=MILVUS_GPU_IVF_PQ`
- required persisted fields:
  - `accelerator_policy=gpu_required_or_preferred`
  - `gpu_search_layout=pq`
  - `pq_subquantizers`
  - `pq_bits_per_code`
  - `nlist`
  - `nprobe_default`

### `MILVUS_GPU_BRUTE_FORCE`

- donor engines:
  - `Milvus`
- class:
  - donor-visible exact GPU variant over flat vector runtime
- canonical lowering:
  - `physical_family=VECTOR_FLAT`
  - `planner_family=ANN_EXACT`
  - `metrics_type=ANN`
  - `family_mode=MILVUS_GPU_BRUTE_FORCE`
- required persisted fields:
  - `accelerator_policy=gpu_required_or_preferred`
  - `exact_scan_mode=gpu_bruteforce`
  - `distance_metric`
  - `vector_dimension`

### ClickHouse runtime families

The following named families are true Beta 2 runtime or major-variant work and
are specified in family documents in this section:

- `CLICKHOUSE_SET`
- `CLICKHOUSE_TOKENBF_V1`
- `CLICKHOUSE_SPARSE_GRAMS`
- `CLICKHOUSE_TEXT`
- `CLICKHOUSE_HYPOTHESIS`
- `CLICKHOUSE_VECTOR_SIMILARITY`

## Parser, Catalog, and Runtime Flow

### Create-time flow

1. V3 parser resolves the donor-visible name to one admitted `IndexType`.
2. The family-specific validator resolves:
   - key-type admissibility
   - option admissibility
   - donor compatibility mode
   - required resolved runtime family
3. `CatalogManager::createIndex(...)` persists:
   - `index_type`
   - `physical_family`
   - `planner_family`
   - `family_mode`
   - `metrics_type`
   - resolved runtime options payload
4. `IndexFactory` creates the family through the admitted name, not by bypassing
   to the shared runtime directly.
5. Initial `index_stats.family_metrics_payload` is published before the index
   enters `QUERYABLE`.

### Open-time flow

1. Open reads the persisted named family.
2. Open validates the persisted resolved-runtime fields against the family
   validator.
3. Open refuses the index if:
   - family-mode fields are absent
   - resolved runtime differs from persisted metadata
   - donor-required option domains are violated
4. Open publishes the named family into the in-memory stats packet cache.

### Optimizer flow

1. Candidate enumeration uses the admitted named family.
2. Lowering resolves the planner family and exactness class.
3. Statistics loads the named-family packet.
4. Ranking uses the shared metrics envelope plus family-native payload.
5. `EXPLAIN` and plan payloads retain the named family and the resolved runtime
   family side by side.

## First-Class Metrics Rule

Every family admitted by this file must publish:

- `shared_metrics_envelope`
- `family_metrics_type`
- `family_metrics.named_family`
- `family_metrics.resolved_runtime_family`
- `family_metrics.family_mode`
- `family_metrics.native_runtime_metrics`

Required donor-visible variation fields:

- `SPATIAL`:
  - `geometry_kind_mask`
  - `srid_policy`
  - `nearest_supported`
  - `exact_recheck_ratio`
- `VECTOR` and Milvus vector families:
  - `resolved_runtime_family`
  - `distance_metric`
  - `vector_dimension`
  - `accelerator_policy`
  - `candidate_budget_default`
  - `recall_estimate_at_k`
- `COLUMNAR` and `MONGODB_COLUMN`:
  - `projection_layout_count`
  - `late_materialization_gain_est`
  - `delta_fraction`
  - `path_projection_cardinality`
- `YBGIN`:
  - `opclass_family`
  - `recheck_ratio_est`
  - `posting_density_est`
  - `fast_update_supported=false`
  - `backfill_progress_fraction`
- `CLICKHOUSE_*` families:
  - family-specific metrics from their family specs

No family from this file may be treated as hint-only, advisory-only, or
secondary if it can legally serve the query shape.

## Plan Rendering Rules

### Required labels

- `SPATIAL` renders as `SPATIAL`
- `VECTOR` renders as `VECTOR`
- `COLUMNAR` renders as `COLUMNAR`
- `MONGODB_COLUMN` renders as `COLUMN`
- `YBGIN` renders as `YBGIN`
- Milvus families render as their exact donor names
- ClickHouse families render as their exact donor names with lowercase aliases
  only in donor overlay modes where required

### Required supplemental plan fields

Every plan node using one of these families must expose:

- `named_family`
- `resolved_runtime_family`
- `family_mode`
- `queryability_state`
- `metrics_confidence_class`
- donor-visible options relevant to the family

## Required Pseudocode

```cpp
IndexAdmission resolve_emulation_family(const CreateIndexStmt& stmt) {
    IndexAdmission out;
    out.named_family = parse_index_type(stmt.using_name);
    out.validator = validator_for(out.named_family);
    out.options = out.validator->validate_and_normalize(stmt);
    out.physical_family = out.validator->resolve_runtime_family(out.options);
    out.planner_family = planner_family_for(out.named_family, out.options);
    out.metrics_type = metrics_type_for(out.named_family, out.options);
    out.family_mode = out.validator->family_mode(out.options);
    return out;
}
```

```cpp
IndexFamilyMetricsPacket publish_emulation_metrics(const IndexInfo& index) {
    IndexFamilyMetricsPacket packet = build_shared_packet(index);
    packet.alias_surface = true;
    packet.named_family = index.index_type_name;
    packet.runtime_family = index.physical_family;
    packet.family_mode = index.family_mode;
    packet.family_metrics_type = index.metrics_type;
    packet.family_metrics = collect_named_family_metrics(index);
    return packet;
}
```

## Required Test Lanes

The Section 18 gate must add proof for:

- V3 parser admission of every new family in this file
- persisted `index_type` plus `physical_family` plus `family_mode` round-trip
- create-time refusal for invalid donor option bundles
- named-family packet publication for every new family
- `EXPLAIN` rendering that preserves donor-visible family names
- candidate enumeration proving new families are not silently ignored

## Cross-Section References

- `INDEX_RUNTIME_TAXONOMY_AND_ALIAS_LOWERING.md`
- `INDEX_CATALOG_AND_METADATA.md`
- `INDEX_METRICS_AND_COSTING.md`
- `INDEX_FAMILY_NATIVE_METRICS_PACKET_CONTRACT.md`
- `COLUMNSTORE_SPEC.md`
- `GIN_SPEC.md`
- `SPATIAL_SPEC.md`
- `VECTOR_ANN_PLANNER_SPEC.md`
