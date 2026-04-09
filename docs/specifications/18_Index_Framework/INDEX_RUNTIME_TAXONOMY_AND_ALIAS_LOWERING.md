# Index Runtime Taxonomy and Alias Lowering

## Purpose
Define the authoritative mapping between exposed index names, real ScratchBird
runtime families, planner families, and create-time rejection rules.

## Scope
- runtime-family identity
- compatibility-label lowering
- query-shape lowering for alias families
- catalog metadata required for open-time validation
- planner-family binding

## Hard Invariants
1. A user-visible index name is not a storage identity unless ScratchBird has a
   real runtime, metadata packet, and lifecycle contract for it.
2. Donor-engine names may remain parseable aliases, but they must lower into a
   canonical ScratchBird physical family or be rejected at create time.
3. Open-time validation must use persisted family metadata, never implicit
   in-memory defaults.
4. Planner enumeration must consume canonical planner families, not raw parser
   labels.
5. Every admitted named `CatalogManager::IndexType` remains an independently
   primary catalog and planner identity even when it routes through a shared
   physical family.

## Canonical Taxonomy Layers

### Layer 1: Physical runtime family
The on-disk and maintenance identity:

- `BTREE`
- `HASH`
- `LSM`
- `BRIN`
- `BITMAP`
- `COLUMNSTORE`
- `SET`
- `GIST`
- `SPGIST`
- `RTREE`
- `GIN`
- `TOKEN_BLOOM_TEXT`
- `CLICKHOUSE_TEXT`
- `HYPOTHESIS_SUMMARY`
- `GIN_TEXT`
- `INVERTED_TEXT`
- `VECTOR_FLAT`
- `HNSW`
- `IVF`
- `VECTOR_SIMILARITY`

### Layer 2: Planner family
The optimizer-facing behavior class:

- `ORDERED_EXACT`
- `HASH_EXACT`
- `WRITE_OPTIMIZED_EXACT`
- `SUMMARY`
- `FILTER_ONLY`
- `COMPRESSED_CANDIDATE`
- `COLUMNAR`
- `GENERALIZED`
- `SPATIAL`
- `INVERTED_BOOLEAN`
- `INVERTED_RANKED`
- `ANN_EXACT`
- `ANN_APPROX`

### Layer 3: Dialect or compatibility alias
Parser-facing names that lower to a physical family plus option bundle:

- `ART`
- `STL_SORT`
- `BLOOM`
- `FULLTEXT`
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
- `INVERTED`
- `TRIE`
- `NGRAM`
- `SPARSE_INVERTED`
- `SPARSE_WAND`
- `RHNSW_PQ`
- `RHNSW_SQ`
- `IVF_FLAT`
- `IVF_PQ`
- `IVF_SQ8`
- `DISKANN`
- `SCANN`
- `ANNOY`
- `NSG`
- donor-shaped MongoDB, Neo4j, Cassandra, and Redis names

## Named-family authority rule

Physical families, planner families, and admitted named catalog families are
distinct layers.

The governing rule is:
- the physical family may be a shared implementation substrate
- the planner family may be a shared costing or exactness substrate
- the admitted named family remains independently primary for DDL, catalog
  identity, metrics publication, candidate enumeration, and operator
  observability

Shared lowering is therefore normalization, not demotion.

## Canonical Runtime Matrix

| Physical family | Planner family | Lifecycle model | Ordered output | Recheck default |
| --- | --- | --- | --- | --- |
| `BTREE` | `ORDERED_EXACT` | mutable in place | yes | visibility only |
| `HASH` | `HASH_EXACT` | mutable in place | no | key and visibility |
| `LSM` | `WRITE_OPTIMIZED_EXACT` | immutable generation | conditional | visibility only |
| `BRIN` | `SUMMARY` | summary generation | no | residual and visibility |
| `BITMAP` | `COMPRESSED_CANDIDATE` | mutable or sealed candidate store | no | key, residual, and visibility |
| `COLUMNSTORE` | `COLUMNAR` | immutable generation | no | visibility unless exact covering is proven |
| `SET` | `SUMMARY` | mutable summary per granule | no | key, residual, and visibility |
| `GIST` | `GENERALIZED` | mutable in place | conditional | opclass-defined |
| `SPGIST` | `GENERALIZED` | mutable in place | conditional | opclass-defined |
| `RTREE` | `SPATIAL` | mutable in place | conditional nearest only | visibility and optional predicate recheck |
| `GIN` | `GENERALIZED` | mutable plus pending state | no | predicate and visibility |
| `TOKEN_BLOOM_TEXT` | `FILTER_ONLY` | mutable summary per granule | no | predicate and visibility |
| `CLICKHOUSE_TEXT` | `INVERTED_RANKED` | immutable segment generation | no | phrase, score, and visibility |
| `HYPOTHESIS_SUMMARY` | `SUMMARY` | mutable summary per granule | no | predicate and visibility |
| `GIN_TEXT` | `INVERTED_BOOLEAN` | mutable plus pending state | no | predicate and visibility |
| `INVERTED_TEXT` | `INVERTED_RANKED` | immutable segment generation | no | score or phrase dependent |
| `VECTOR_FLAT` | `ANN_EXACT` | immutable generation or exact attached structure | order by distance only | visibility only |
| `HNSW` | `ANN_APPROX` | graph plus rebuild generation | approximate order | candidate rerank and visibility |
| `IVF` | `ANN_APPROX` | trained partition generation | approximate order | candidate rerank and visibility |
| `VECTOR_SIMILARITY` | `ANN_APPROX` | graph plus donor-parameter image | approximate order | candidate rerank and visibility |

## Alias Lowering Rules

### Ordered and exact aliases
- `ART` lowers to `BTREE` unless and until ScratchBird ships a distinct ART
  runtime contract with proven prefix-order semantics.
- `STL_SORT` lowers to `BTREE` only for exact ordered semantics; it may not
  imply a separate long-term storage identity.

### Summary and filter aliases
- `ZONEMAP` lowers to `BRIN`.
- `BLOOM` lowers to planner family `FILTER_ONLY`.
- `BITMAP_INDEX_SCAN` is not a stored-family name. It is a planner path that
  must remain distinct from physical family `BITMAP`.

### Text aliases
- `FULLTEXT` is a query-shape alias:
  - boolean containment, token membership, and `@@`-style predicate search
    lower to `GIN_TEXT`
  - ranked retrieval, analyzer-weighted search, wildcard text expansion, and
    top-`K` text retrieval lower to `INVERTED_TEXT`
- `INVERTED`, `TRIE`, `NGRAM`, `SPARSE_INVERTED`, and `SPARSE_WAND` lower to
  `GIN_TEXT` or `INVERTED_TEXT` only when their analyzer, scoring, and payload
  policy can be expressed by one of those physical families. Otherwise create
  must reject them.

### Spatial and generalized aliases
- generic donor `SPATIAL` lowers to `RTREE` only when the key encoding,
  coordinate model, and searchable operator class are all persisted and
  validated.
- MongoDB and Redis geo names lower to `RTREE` only when the key encoding and
  predicate set remain within the declared `RTREE` scope.
- `GIST` and `SPGIST` never infer predicate support from the family name alone;
  every searchable operator must be provided by a bound opclass.
- `YBGIN` lowers to `GIN` only when the donor-visible access-method identity,
  backfill mode, and unsupported fast-update boundary are persisted as explicit
  family-mode metadata.

### Vector aliases
- generic donor `VECTOR` lowers to `VECTOR_FLAT`, `HNSW`, or `IVF` only when
  the resolved runtime family and exact-versus-approximate mode are persisted.
- `RHNSW_PQ` and `RHNSW_SQ` lower to `HNSW` only when compressed-payload mode,
  rerank semantics, and open-time validation are persisted.
- `IVF_FLAT`, `IVF_PQ`, and `IVF_SQ8` lower to `IVF` only when training
  artifacts, payload mode, and quantizer metadata are all persisted.
- `MILVUS_AUTOINDEX` lowers to one resolved ANN family only when the selection
  result and its policy version are persisted.
- `MILVUS_IVF_RABITQ`, `MILVUS_IVF_HNSW`, `MILVUS_GPU_IVF_FLAT`,
  `MILVUS_GPU_IVF_PQ`, and `MILVUS_GPU_BRUTE_FORCE` are admitted named families
  over existing ANN substrates and must keep their independent metrics and
  `EXPLAIN` identities.
- `SCANN`, `DISKANN`, `ANNOY`, `NSG`, and `GPU_CAGRA` are Beta 1 admitted named
  families. They may share ANN or vector runtime substrates, but create-time
  activation is required once family-specific options, fallback policy, and
  persisted canonical family fields can be validated.
- Parser-only acceptance without admissible runtime, metadata, costing, and
  governance closure is non-conforming.

### Summary, columnar, and text-filter aliases
- generic donor `COLUMNAR` lowers to `COLUMNSTORE` only when the projection
  layout, delta-lane behavior, and late-materialization policy are persisted.
- `MONGODB_COLUMN` lowers to `COLUMNSTORE` only when the donor-visible
  `columnstore` plan and catalog identity remain independently primary.
- `CLICKHOUSE_SET` maps to physical family `SET`; it is not a `BRIN` or
  `BITMAP` alias.
- `CLICKHOUSE_TOKENBF_V1` and `CLICKHOUSE_SPARSE_GRAMS` map to physical family
  `TOKEN_BLOOM_TEXT`; they are filter-only text families, not ranked text.
- `CLICKHOUSE_TEXT` maps to physical family `CLICKHOUSE_TEXT`; it is not a
  cosmetic alias of `FULLTEXT`.
- `CLICKHOUSE_HYPOTHESIS` maps to physical family `HYPOTHESIS_SUMMARY`; its
  supported predicate class must be persisted.
- `CLICKHOUSE_VECTOR_SIMILARITY` maps to physical family
  `VECTOR_SIMILARITY`; it may share ANN code, but it remains a distinct named
  family with donor parameter identity.

## Catalog Metadata Contract
Every index catalog row must persist:

- `physical_family`
- `planner_family`
- `family_mode`
- `format_version`
- `alias_origin`, if created through an alias surface
- `family_options_version`
- `lifecycle_model`
- `metrics_type`
- `metrics_version`
- `opclass_id` and `opclass_version` when applicable
- `queryability_state`

This persisted set is Beta 1 required authority for every admitted named
family, including families that share one physical backend or one planner
family substrate.

Additional required family metadata:

- `BTREE`, `HASH`, `LSM`, `BRIN`, `BITMAP`, `COLUMNSTORE`:
  family-specific storage options and rebuild state
- `SET`, `HYPOTHESIS_SUMMARY`:
  granule summary mode, overflow behavior, and supported predicate class
- `GIST`, `SPGIST`, `RTREE`:
  strategy-map version, support-function version, geometry or partition format
- `TOKEN_BLOOM_TEXT`, `CLICKHOUSE_TEXT`:
  tokenizer identifiers, Bloom or posting parameters, scoring or filter mode,
  and merge behavior
- `GIN`, `GIN_TEXT`, `INVERTED_TEXT`:
  analyzer identifiers, scoring model, payload options
- `VECTOR_FLAT`, `HNSW`, `IVF`, `VECTOR_SIMILARITY`:
  metric, vector dimension, payload mode, build knobs, search defaults, and
  training artifact identifiers when applicable

## Create-Time Rules
1. Parser label is accepted only if it lowers to one physical family.
2. Lowering must also resolve the planner family and lifecycle model.
3. If lowering requires metadata that ScratchBird cannot persist or validate,
   create must fail.
4. If a family requires an opclass and no valid default exists, the user must
   provide one explicitly.
5. Donor-visible alias families must persist their resolved runtime family and
   family-mode metadata before the row may publish as active.

## Open-Time Rules
1. Open must validate `physical_family`, `format_version`, `family_mode`, and
   all required persisted options.
2. If an alias originally created the index, open must ignore the alias label
   and trust only persisted canonical metadata.
3. Any runtime or metadata mismatch forces `FAILED` or rebuild-required state;
   silent fallback is forbidden.

## Queryability States
Every family must expose:

- `BUILDING`
- `VALIDATING`
- `QUERYABLE`
- `STALE`
- `MERGING`
- `RETIRING`
- `FAILED`

Only `QUERYABLE` and explicitly allowed `STALE` states may participate in new
planner enumeration.

## Cross-Section References
- `INDEX_ARCHITECTURE.md`
- `BETA2_EMULATION_INDEX_SURFACE_ADMISSION_AND_FIRST_CLASS_OPTIMIZER_MODEL.md`
- `INDEX_CATALOG_AND_METADATA.md`
- `INDEX_MGA_PUBLICATION_AND_RECLAIM.md`
- `INDEX_PLANNER_PATH_TAXONOMY_AND_EXACTNESS.md`
- `../21_V3_Dialect_Surface/README.md`
