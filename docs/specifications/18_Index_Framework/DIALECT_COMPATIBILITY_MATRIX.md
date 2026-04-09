# Dialect Compatibility Matrix

Status: current_authority

## Purpose

Enumerate index features required to emulate Firebird 5, PostgreSQL 18, MySQL 8, Cassandra, Milvus, MongoDB, Neo4j, and Redis, while also recording the current ScratchBird runtime family mapping for shipped index surfaces.

## ScratchBird current runtime family mapping

The following mapping is authoritative for current code-backed implementation:

- ordered family via `BTREE_SPEC.md`:
  - `BTREE`, `STL_SORT`, `ART`, `MONGODB_GEO_HAYSTACK`, `NEO4J_RANGE`, `NEO4J_POINT`, `REDIS_LIST`, `REDIS_ZSET`, `REDIS_STREAM`
- hash family via `HASH_SPEC.md`:
  - `HASH`, `REDIS_STRING`, `REDIS_HASH`, `REDIS_SET`, `REDIS_HLL`
- spatial family via `SPATIAL_SPEC.md`:
  - `RTREE`, `MONGODB_2D`, `MONGODB_2DSPHERE`, `MONGODB_2DSPHERE_BUCKET`, `REDIS_GEO`
- bitmap family via `BITMAP_SPEC.md`:
  - `BITMAP`, `NEO4J_LOOKUP`, `REDIS_BITMAP`
- generalized inverted family via `GIN_SPEC.md`:
  - `GIN`
- generalized search-tree family via `GIST_SPEC.md`:
  - `GIST`
- partitioned search-tree family via `SPGIST_SPEC.md`:
  - `SPGIST`
- summary family via `BRIN_SPEC.md`:
  - `BRIN`, `ZONEMAP`, `BLOOM`
- LSM family via `LSM_TREE_SPEC.md`:
  - `LSM`
- inverted/text family via `FULLTEXT_SPEC.md`:
  - `FULLTEXT`, `INVERTED`, `MONGODB_WILDCARD`, `MONGODB_ENCRYPTED_RANGE`, `NEO4J_TEXT`, `CASSANDRA_SASI`, `CASSANDRA_SAI`, `TRIE`, `NGRAM`, `SPARSE_INVERTED`, `SPARSE_WAND`, `MINHASH_LSH`
- shared vector family via `HNSW_SPEC.md`:
  - `HNSW`, `NEO4J_VECTOR`, `IVF`, `VECTOR_FLAT`, `VECTOR_BIN_FLAT`, `IVF_FLAT`, `BIN_IVF_FLAT`, `IVF_PQ`, `IVF_SQ8`, `IVF_SQ8_HYBRID`, `RHNSW_PQ`, `RHNSW_SQ`, `ANNOY`, `NSG`, `DISKANN`, `SCANN`, `GPU_CAGRA`
- projection family via `COLUMNSTORE_SPEC.md`:
  - `COLUMNSTORE`

All mapped families inherit the MGA-first rules from `INDEX_ARCHITECTURE.md`, `INDEX_MGA_PUBLICATION_AND_RECLAIM.md`, and `INDEX_METRICS_AND_COSTING.md`.

## Firebird 5

- Index types: B-tree only.
- Direction: ascending or descending.
- Computed indexes: supported via `COMPUTED BY`.
- Partial indexes: supported via `WHERE` with optimizer restrictions.
- Partial index usability rules:
  - query WHERE includes exactly the same boolean expression as the index predicate
  - or the predicate is an OR-list and one OR clause matches the query WHERE
  - or the predicate is `IS NOT NULL` and the query predicate is known to exclude NULLs
- Unique indexes: allow multiple NULLs.
- Locking and visibility follow MGA semantics, not WAL or reader-lock semantics.

## PostgreSQL 18

- Index types: B-tree, Hash, GiST, SP-GiST, GIN, BRIN.
- Unique indexes: only B-tree; `NULLS [NOT] DISTINCT` supported.
- Hash indexes: equality-only comparisons.
- Unique hash indexes are rejected in Alpha.
- BRIN: summaries per block range with autosummarize per `BRIN_SPEC.md` defaults.
- SP-GiST: `allTheSame` inner tuples required for degenerate splits.

## MySQL 8

- B-tree default index type for InnoDB and MyISAM.
- MEMORY engine supports HASH and BTREE.
- Prefix lengths are required for some `BLOB` and `TEXT` cases.
- FULLTEXT applies only to the documented text-capable column families and shared charset/collation sets.
- SPATIAL uses bounding-region semantics and exact recheck.

## Cassandra (SAI and SASI)

- Index types: SASI and SAI map into the current inverted/text family unless and until a distinct runtime is promoted.
- Planner use requires the typed inverted-family metrics packet and MGA visibility rejection accounting.

## Milvus

- Vector surfaces map into the current shared vector-family runtime.
- Exposed options must be limited to what the current runtime can honor safely and measurably.

## MongoDB

- `2d`, `2dsphere`, and `2dsphere_bucket` map into the current spatial family.
- wildcard and encrypted range map into the current inverted/text family.
- geoHaystack currently maps into the ordered family and must not overclaim true MongoDB-native geoHaystack semantics.

## Neo4j

- lookup maps into bitmap family
- range maps into ordered family
- point maps into ordered-family current runtime surface unless a distinct spatial/point runtime is promoted
- text maps into inverted/text family
- vector maps into shared vector family

## Redis

- GEO maps into spatial family
- list, zset, and stream currently map into ordered family
- string, hash, set, and HLL map into hash family
- bitmap maps into bitmap family

## Notes

- Dialect-specific restrictions are enforced in the parser layer.
- Current family routing is code-backed implementation authority and takes precedence over older target-state narratives.
- No family is planner-visible without the typed metrics packet required by section `18` umbrella metrics canon.
