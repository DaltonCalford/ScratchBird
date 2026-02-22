# Canonical Methods
Last modified: 2026-02-21

Back links:
- [Index Methods README](README.md)
- [Data Types README](../README.md)

Series navigation:
- Previous: [Parser-Accepted Methods](parser-accepted-methods.md)

## Canonical Index Type Set (58)
Catalog canonical validation recognizes these method names:

`BTREE`, `HASH`, `GIN`, `GIST`, `SPGIST`, `BRIN`, `FULLTEXT`, `SPATIAL`, `BITMAP`, `COLUMNSTORE`, `LSM`, `HNSW`, `IVF`, `ART`, `BLOOM`, `VECTOR_FLAT`, `VECTOR_BIN_FLAT`, `IVF_FLAT`, `BIN_IVF_FLAT`, `IVF_PQ`, `IVF_SQ8`, `IVF_SQ8_HYBRID`, `RHNSW_PQ`, `RHNSW_SQ`, `ANNOY`, `NSG`, `DISKANN`, `SCANN`, `GPU_CAGRA`, `MINHASH_LSH`, `SPARSE_INVERTED`, `SPARSE_WAND`, `TRIE`, `INVERTED`, `STL_SORT`, `NGRAM`, `MONGODB_2D`, `MONGODB_2DSPHERE`, `MONGODB_2DSPHERE_BUCKET`, `MONGODB_GEO_HAYSTACK`, `MONGODB_WILDCARD`, `MONGODB_ENCRYPTED_RANGE`, `NEO4J_LOOKUP`, `NEO4J_TEXT`, `NEO4J_RANGE`, `NEO4J_POINT`, `NEO4J_VECTOR`, `CASSANDRA_SASI`, `CASSANDRA_SAI`, `REDIS_STRING`, `REDIS_HASH`, `REDIS_LIST`, `REDIS_SET`, `REDIS_ZSET`, `REDIS_STREAM`, `REDIS_BITMAP`, `REDIS_HLL`, `REDIS_GEO`.

## Alias Normalization Notes
Executor/catalog normalization supports alias spellings for some ingest paths:

- `B-TREE` -> `BTREE`
- `VECTOR` -> `HNSW`
- `ZONE_MAP` -> `ZONEMAP`
- `R-TREE` -> `RTREE`
- `SP-GIST` -> `SPGIST`
- `LSMTREE` -> `LSM`
- `LSM-TREE` -> `LSM`
- `2D` -> `MONGODB_2D`
- `2DSPHERE` -> `MONGODB_2DSPHERE`
- `GEOHAYSTACK` -> `MONGODB_GEO_HAYSTACK`

## Native v3 Parser Rule
Native parser v3 syntax should use canonical method tokens documented in:
- [Parser-Accepted Methods](parser-accepted-methods.md)
- [DDL INDEX CREATE](../../ddl/data-storage/index/create.md)

Not every catalog/executor alias is accepted by native v3 parser text input.
