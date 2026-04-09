Status: canonical_beta2_immediate_implementation

# ClickHouse Vector Similarity Specification

## Purpose

Define the `CLICKHOUSE_VECTOR_SIMILARITY` family required for donor-compatible
emulation of ClickHouse `vector_similarity(...)` indexes.

## Donor Basis

The donor shape is grounded in:

- `MergeTreeIndices.cpp` registration of `vector_similarity`
- `MergeTreeIndexVectorSimilarity.h`
- `MergeTreeIndexVectorSimilarity.cpp`

Those donor files prove:

- only `hnsw` method is admitted
- distance functions include `L2Distance` and `cosineDistance`
- optional quantization and HNSW build parameters are persisted
- supported quantizations include at least:
  - `f64`
  - `f32`
  - `f16`
  - `bf16`
  - `i8`
  - `b1`
- donor runtime exposes graph statistics such as connectivity, node count,
  edge count, and memory usage

## Canonical Identity

- admitted named family:
  - `CLICKHOUSE_VECTOR_SIMILARITY`
- donor engines supported:
  - `ClickHouse`
- physical family:
  - `VECTOR_SIMILARITY`
- planner family:
  - `ANN_APPROX`
- metrics type:
  - `ANN`
- lifecycle model:
  - durable graph image plus resident search state

## Required DDL Surface

Canonical donor-compatible forms:

```sql
CREATE INDEX idx_name ON t (vec)
USING CLICKHOUSE_VECTOR_SIMILARITY('hnsw', 'L2Distance', 768);

CREATE INDEX idx_name ON t (vec)
USING CLICKHOUSE_VECTOR_SIMILARITY('hnsw', 'cosineDistance', 768, 'bf16', 32, 128);
```

Required argument rules:

- exactly 3 or 6 arguments
- argument 1:
  - method string
  - only `hnsw`
- argument 2:
  - distance string
  - only donor-supported distance names
- argument 3:
  - positive dimension count
- optional argument 4:
  - supported quantization string
- optional arguments 5 and 6:
  - positive HNSW connectivity and expansion-add integers

Binary quantization rules:

- only valid with cosine distance
- dimension must be a multiple of 8

## Runtime Model

`CLICKHOUSE_VECTOR_SIMILARITY` is a distinct family, not a cosmetic alias of
generic `HNSW`.

Required persisted fields:

- `method_name = hnsw`
- `distance_metric`
- `vector_dimension`
- `scalar_quantization`
- `hnsw_connectivity`
- `hnsw_expansion_add`
- `expansion_search_default`
- `resolved_scalar_kind`
- `resolved_metric_kind`

The resident runtime may reuse ScratchBird ANN infrastructure, but the named
family, donor argument names, and persisted graph-parameter contract remain
independently primary.

## Metrics Contract

The native payload must include:

- `named_family = "CLICKHOUSE_VECTOR_SIMILARITY"`
- `vector_dimension`
- `distance_metric`
- `scalar_quantization`
- `graph_connectivity`
- `graph_expansion_add`
- `graph_expansion_search_default`
- `resident_bytes`
- `node_count`
- `edge_count`
- `max_level`
- `bytes_per_vector`
- `avg_candidates_scanned`
- `recall_estimate_at_k`
- `rerank_fraction`
- `cold_load_penalty`

## Optimizer Rules

1. This family is a primary ANN candidate for donor vector-search predicates.
2. Ranking must compare it against:
   - generic ANN siblings
   - exact vector scan fallback
3. Donor-visible argument choices must affect costing:
   - quantization lowers memory cost but may reduce recall
   - higher connectivity and expansion-add raise build cost and memory but may
     improve recall
4. `EXPLAIN` must display the donor-visible method and distance names.

## Required Pseudocode

```cpp
VectorSimilaritySpec spec = parse_clickhouse_vector_args(args);
validate_vector_similarity(spec);
PersistedGraphMeta meta = {
    .method_name = spec.method_name,
    .distance_metric = spec.distance_metric,
    .vector_dimension = spec.dimension,
    .scalar_quantization = spec.quantization,
    .hnsw_connectivity = spec.connectivity,
    .hnsw_expansion_add = spec.expansion_add
};
```

## Refusal Rules

Create must fail if:

- the method is not `hnsw`
- the distance metric is unsupported
- dimension is zero
- quantization is unsupported
- binary quantization violates donor constraints
- key arity is not exactly one vector expression

## First-Class Rule

`CLICKHOUSE_VECTOR_SIMILARITY` is a first-class ANN family. It may share ANN
substrates, but it must publish its own packet, plan label, and calibration.
