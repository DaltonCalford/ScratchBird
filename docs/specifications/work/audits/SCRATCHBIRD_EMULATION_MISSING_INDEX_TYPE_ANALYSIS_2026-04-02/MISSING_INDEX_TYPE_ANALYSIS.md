# Missing Index Type Analysis

## Purpose

Determine which raw "missing index type" rows are real ScratchBird engine gaps
for full donor-engine emulation, which are only dialect or catalog mapping
gaps, and which are not persisted index families at all.

## Source Basis

This analysis uses only local source already present in the ScratchBird tree:

- the raw packet in
  `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/MISSING_DONOR_INDEX_SURFACES.csv`
- ScratchBird family admission and runtime code in:
  - `include/scratchbird/parser/ast_v3.h`
  - `src/parser/parser_v3.cpp`
  - `src/core/index_factory.cpp`
  - `src/optimizer/index_family_lowering.cpp`
- donor-source anchors for the rows being classified, including:
  - MongoDB `index_names.h`, `index_names.cpp`,
    `shard_role/shard_catalog/index_catalog_impl.cpp`,
    and `index_key_validate.cpp`
  - ClickHouse `MergeTreeIndices.cpp` and `MergeTreeIndexHypothesis.cpp`
  - Milvus `client/index/common.go` and
    `internal/util/indexparamcheck/index_type.go`
  - CockroachDB `pkg/sql/sem/idxtype/idxtype.proto`,
    `pkg/sql/sem/idxtype/idxtype.go`, and `pkg/sql/sem/tree/create.go`
  - Dolt `go/libraries/doltcore/schema/index.go`
  - Vitess `go/vt/sqlparser/sql.y` and `go/vt/vtgate/vindexes/vschema.go`
  - immudb `embedded/sql/sql_grammar.y`
  - XTDB `information_schema.clj` and `trie_catalog.clj`
  - YugabyteDB `src/postgres/src/include/catalog/pg_am.dat`
  - Apache Ignite `H2IndexType.java`
  - TiDB `pkg/parser/parser.y`
  - MariaDB `sql/sql_yacc.yy`

## Summary

The raw packet contains 70 rows. After source-backed review:

- 34 rows are `EXISTING_SB_SURFACE`
  - 29 unique engine/surface pairs
- 16 rows are `NON_FAMILY_PROPERTY_OR_MODIFIER`
  - they should not drive physical index-family work
- 14 rows are `NEW_DONOR_SURFACE_OVER_EXISTING_RUNTIME`
  - 13 unique engine/surface pairs
- 6 rows are `NEW_RUNTIME_OR_MAJOR_VARIANT`
  - 6 unique engine/surface pairs

The practical result is that the raw packet materially overstates new engine
work. Most rows are either:

- already present in ScratchBird as admitted named families
- donor parser, routing, or property surfaces rather than persisted families
- donor-visible names that can sit over existing ScratchBird runtimes once
  family-specific validators and metadata contracts are added

## Classification Rules

### `EXISTING_SB_SURFACE`

Use this class when donor source proves the row is a real index surface and
ScratchBird source already proves the named family or a donor-shaped named
family exists in `ast_v3.h`, `parser_v3.cpp`, and `index_factory.cpp`.

These are mapping or packet-quality gaps, not new physical-family gaps.

### `NON_FAMILY_PROPERTY_OR_MODIFIER`

Use this class when donor source proves the row is:

- an index property
- a DDL modifier
- a routing abstraction
- a planner hint
- a table access method
- a scan or generic access-path label

These rows belong in dialect, routing, or planner compatibility work, not the
physical-family backlog.

### `NEW_DONOR_SURFACE_OVER_EXISTING_RUNTIME`

Use this class when donor source proves the surface is real, ScratchBird
already has a plausible runtime substrate, but current ScratchBird source does
not yet expose the donor-visible family name, validator, or persisted metadata
contract needed for 1:1 emulation.

These do not require inventing a new base physical family, but they do require
new named-family or alias authority.

### `NEW_RUNTIME_OR_MAJOR_VARIANT`

Use this class when donor source proves the surface is real and current
ScratchBird source does not prove a conforming lowering onto an existing
runtime.

These are real Beta 2 design items.

## Packet Defects

### False Positive: MongoDB `2DSPHERE_BUCKET`

The raw packet marks MongoDB `2DSPHERE_BUCKET` as missing, but ScratchBird
already exposes `MONGODB_2DSPHERE_BUCKET` in `include/scratchbird/parser/ast_v3.h`,
admits it in `src/parser/parser_v3.cpp`, and binds it to `RTREE` runtime class
in `src/core/index_factory.cpp`. This is a packet defect, not a missing family.

### Under-Reported Surfaces

The current raw packet also misses donor surfaces that exist in donor source:

- Apache Ignite `SPATIAL`
  - present in `H2IndexType.java`
- TiDB `COLUMNAR`
  - present in `IndexKeyTypeOpt` in `pkg/parser/parser.y`

Those omissions matter because they show the current packet is not yet a safe
planning truth source by itself.

### Non-Family Contamination

The raw packet includes many rows that are not persisted families:

- Dolt `UNIQUE`, `SPATIAL`, `FULLTEXT`, `VECTOR`
  - exposed as index properties in `schema/index.go`
- Vitess `VINDEX`
  - routing construct in `vtgate/vindexes/vschema.go`
- CockroachDB `FORWARD` and `HASH_SHARDED`
  - generic index type and sharding modifier in `idxtype.*` and `create.go`
- immudb `SECONDARY_INDEX`, `UNIQUE_INDEX`, `USE_INDEX_HINT`
  - grammar surfaces in `sql_grammar.y`
- XTDB `HEAP`
  - `pg_am` table access method entry in `information_schema.clj`
- Apache Ignite `SCAN`
  - access-path label in `H2IndexType.java`
- Milvus `VECINDEX` and `HYBRID`
  - generic class or composite marker in `index_type.go`

These should not be treated as requests for new ScratchBird persisted families.

## Existing ScratchBird Surfaces

The largest class is mapping-only work. Examples:

- MongoDB `2DSPHERE_BUCKET`
- Neo4j `FULLTEXT`
- Milvus `MINHASH_LSH`, `SPARSE_INVERTED_INDEX`, `SPARSE_WAND`, `TRIE`,
  `STL_SORT`, `INVERTED`, `BITMAP`, `NGRAM`, `RTREE`
- Vitess `HASH`, `FULLTEXT`
- XTDB `BTREE`, `HASH`, `TRIE`
- TiDB `BTREE`, `HASH`, `FULLTEXT`
- CockroachDB `INVERTED`
- YugabyteDB `BTREE`, `HASH`, `GIST`, `GIN`, `SPGIST`, `BRIN`, `LSM`
- Apache Ignite `BTREE`, `HASH`

Required work for this class:

1. add dialect or donor-family admission
2. add plan and `SHOW` rendering rules
3. add catalog or access-method mapping
4. fix the packet generator so these do not appear as family gaps again

## Excluded From Physical-Family Backlog

The following unique donor surfaces are not new persisted families:

- Apache Ignite `SCAN`
- Citus `INDEX`
- Citus `PARTITIONED_INDEX`
- CockroachDB `FORWARD`
- CockroachDB `HASH_SHARDED`
- Dolt `FULLTEXT`
- Dolt `SPATIAL`
- Dolt `UNIQUE`
- Dolt `VECTOR`
- Milvus `HYBRID`
- Milvus `VECINDEX`
- Vitess `VINDEX`
- XTDB `HEAP`
- immudb `SECONDARY_INDEX`
- immudb `UNIQUE_INDEX`
- immudb `USE_INDEX_HINT`

These should be handled in parser, routing, or planner compatibility work only.

## New Donor Surfaces Over Existing ScratchBird Runtimes

These are the real named-surface backlog items that do not appear to require
new base runtimes:

- MariaDB `VECTOR`
- TiDB `SPATIAL`
- TiDB `VECTOR`
- Vitess `SPATIAL`
- CockroachDB `VECTOR`
- YugabyteDB `YBGIN`
- MongoDB `COLUMN`
- Milvus `IVF_RABITQ`
- Milvus `IVF_HNSW`
- Milvus `AUTOINDEX`
- Milvus `GPU_IVF_FLAT`
- Milvus `GPU_IVF_PQ`
- Milvus `GPU_BRUTE_FORCE`

### Why These Fit Shared Runtimes

- vector donors:
  ScratchBird already has a large ANN and vector family surface in
  `ast_v3.h` and `index_factory.cpp`, including `VECTOR_FLAT`, `IVF_*`,
  `HNSW`, `DISKANN`, `SCANN`, `GPU_CAGRA`, and related variants.
- spatial donors:
  ScratchBird already has `RTREE` plus donor-shaped MongoDB geo families and
  `index_family_lowering.cpp` already groups those spatial surfaces.
- MongoDB `COLUMN`:
  donor source still recognizes catalog `columnstore` while rejecting new
  creation, and ScratchBird already has `COLUMNSTORE`.
- Yugabyte `YBGIN`:
  donor source names a distinct access method, but ScratchBird already has
  `GIN`; the gap is donor-visible access-method identity and distributed rules.

### Required Work Pattern

For this class, ScratchBird needs:

1. donor-visible named-family admission in parser and catalog
2. create-time validator and metadata rules for the donor name
3. persisted alias-origin or family-mode metadata
4. plan rendering and error-surface parity
5. packet and matrix generator updates so these map to shared runtime truth

## True Runtime or Major-Variant Gaps

The only class that currently looks like genuine new index-family work is the
ClickHouse MergeTree secondary index set:

- `SET`
- `TOKENBF_V1`
- `SPARSE_GRAMS`
- `TEXT`
- `HYPOTHESIS`
- `VECTOR_SIMILARITY`

Why these stay in the real backlog:

- ClickHouse registers them as real secondary index creators and validators in
  `MergeTreeIndices.cpp`
- current ScratchBird source does not prove a conforming lowering for any of
  them
- `HYPOTHESIS` is especially distinct because
  `MergeTreeIndexHypothesis::createIndexCondition` throws `"Not supported"` in
  `MergeTreeIndexHypothesis.cpp`, which makes it a persisted but non-standard
  search surface rather than a normal searchable family

These six items should be treated as the actual Beta 2 physical-family or major
runtime-variant design backlog coming out of the current packet.

## Recommended Work Order

1. Fix the raw packet and matrix generator.
   - remove non-family rows from the physical-family backlog
   - recognize already-admitted ScratchBird surfaces like
     `MONGODB_2DSPHERE_BUCKET`
   - add omitted donor surfaces such as Apache Ignite `SPATIAL`
2. Close the mapping-only backlog.
   - add donor parser, catalog, and plan mappings for the 29 unique
     `EXISTING_SB_SURFACE` items
3. Specify donor-visible shared-runtime families.
   - `VECTOR`
   - `SPATIAL`
   - `COLUMN`
   - `YBGIN`
   - Milvus-specific ANN labels and policies
4. Design the true ClickHouse gaps as Beta 2 index-family work.
5. Re-run the packet generator after each class closes so the remaining backlog
   stays trustworthy.

## Output Artifact

The row-level classification for every raw packet row is in
`MISSING_INDEX_TYPE_DECISION_MATRIX.csv`.
