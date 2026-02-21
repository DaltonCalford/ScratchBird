# Parser-Accepted Index Methods
Last modified: 2026-02-21

Back links:
- [Index Methods README](README.md)
- [Data Types README](../README.md)

Next in series:
- [Canonical Methods](canonical-methods-and-aliases.md)

Native v3 parser accepts the following canonical method tokens (59 total):
- Core: `BTREE`, `HASH`, `HNSW`, `FULLTEXT`, `GIN`, `GIST`, `BRIN`, `RTREE`, `SPGIST`, `BITMAP`, `COLUMNSTORE`, `LSM`, `IVF`, `ZONEMAP`, `ART`, `BLOOM`
- Vector/ANN: `VECTOR_FLAT`, `VECTOR_BIN_FLAT`, `IVF_FLAT`, `BIN_IVF_FLAT`, `IVF_PQ`, `IVF_SQ8`, `IVF_SQ8_HYBRID`, `RHNSW_PQ`, `RHNSW_SQ`, `ANNOY`, `NSG`, `DISKANN`, `SCANN`, `GPU_CAGRA`
- Sparse/text/search variants: `MINHASH_LSH`, `SPARSE_INVERTED`, `SPARSE_WAND`, `TRIE`, `INVERTED`, `STL_SORT`, `NGRAM`
- Mongo variants: `MONGODB_2D`, `MONGODB_2DSPHERE`, `MONGODB_2DSPHERE_BUCKET`, `MONGODB_GEO_HAYSTACK`, `MONGODB_WILDCARD`, `MONGODB_ENCRYPTED_RANGE`
- Neo4j variants: `NEO4J_LOOKUP`, `NEO4J_TEXT`, `NEO4J_RANGE`, `NEO4J_POINT`, `NEO4J_VECTOR`
- Cassandra variants: `CASSANDRA_SASI`, `CASSANDRA_SAI`
- Redis variants: `REDIS_STRING`, `REDIS_HASH`, `REDIS_LIST`, `REDIS_SET`, `REDIS_ZSET`, `REDIS_STREAM`, `REDIS_BITMAP`, `REDIS_HLL`, `REDIS_GEO`
