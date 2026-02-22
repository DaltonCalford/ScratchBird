# Parser-Accepted Index Methods
Last modified: 2026-02-21

Back links:
- [Index Methods README](README.md)
- [Data Types README](../README.md)

Next in series:
- [Canonical Methods](canonical-methods-and-aliases.md)

Native v3 parser accepts these index method tokens (59 total):

- Core: `BTREE`, `HASH`, `FULLTEXT`, `GIN`, `GIST`, `BRIN`, `RTREE`, `SPGIST`, `BITMAP`, `COLUMNSTORE`, `LSM`, `ZONEMAP`
- Vector/ANN: `HNSW`, `IVF`, `VECTOR_FLAT`, `VECTOR_BIN_FLAT`, `IVF_FLAT`, `BIN_IVF_FLAT`, `IVF_PQ`, `IVF_SQ8`, `IVF_SQ8_HYBRID`, `RHNSW_PQ`, `RHNSW_SQ`, `ANNOY`, `NSG`, `DISKANN`, `SCANN`, `GPU_CAGRA`
- Sparse/text/search: `ART`, `BLOOM`, `MINHASH_LSH`, `SPARSE_INVERTED`, `SPARSE_WAND`, `TRIE`, `INVERTED`, `STL_SORT`, `NGRAM`
- Mongo variants: `MONGODB_2D`, `MONGODB_2DSPHERE`, `MONGODB_2DSPHERE_BUCKET`, `MONGODB_GEO_HAYSTACK`, `MONGODB_WILDCARD`, `MONGODB_ENCRYPTED_RANGE`
- Neo4j variants: `NEO4J_LOOKUP`, `NEO4J_TEXT`, `NEO4J_RANGE`, `NEO4J_POINT`, `NEO4J_VECTOR`
- Cassandra variants: `CASSANDRA_SASI`, `CASSANDRA_SAI`
- Redis variants: `REDIS_STRING`, `REDIS_HASH`, `REDIS_LIST`, `REDIS_SET`, `REDIS_ZSET`, `REDIS_STREAM`, `REDIS_BITMAP`, `REDIS_HLL`, `REDIS_GEO`

Operational notes:
- Native v3 parser does not accept alias spellings like `VECTOR`, `SP-GIST`, or `ZONE_MAP` in SQL text.
- `INVERTED` and `STL_SORT` are fully mapped in native v3 runtime (`CREATE INDEX`) and should be documented/used as native methods.
- Full `CREATE INDEX` behavior and `WITH (...)` option semantics are documented in [DDL INDEX CREATE](../../ddl/data-storage/index/create.md).
