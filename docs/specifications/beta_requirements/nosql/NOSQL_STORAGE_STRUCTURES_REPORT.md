# NoSQL Storage Structures Gap Report (Beta)

**Status:** Draft (Beta planning)

## Purpose

This report enumerates common NoSQL data models, the storage structures they
require, and how those structures map to ScratchBird's current specifications.
It highlights gaps that would need Beta-level work to support each model.

This is a storage and catalog report. Query language design (Cypher, Gremlin,
Mongo-style APIs, etc) is intentionally out of scope.

## Existing ScratchBird Building Blocks (Spec-Level)

The following specifications already cover core structures that are useful for
NoSQL models:

- JSON/JSONB types (Alpha stores JSONB as text)
  - `ScratchBird/docs/specifications/types/README.md`
  - `ScratchBird/docs/specifications/types/03_TYPES_AND_DOMAINS.md`
  - `ScratchBird/docs/specifications/types/DATA_TYPE_PERSISTENCE_AND_CASTS.md`
- JSON_TABLE and XMLTABLE for document shredding
  - `ScratchBird/docs/specifications/dml/DML_XML_JSON_TABLES.md`
- Vector data type and ANN indexes (HNSW, IVF)
  - `ScratchBird/docs/specifications/indexes/INDEX_ARCHITECTURE.md`
  - `ScratchBird/docs/specifications/indexes/IVFIndex.md`
- Columnar storage and analytics indexes
  - `ScratchBird/docs/specifications/indexes/COLUMNSTORE_SPEC.md`
  - `ScratchBird/docs/specifications/indexes/ZoneMapsIndex.md`
- LSM tree and LSM TTL for write-heavy and time-series workloads
  - `ScratchBird/docs/specifications/indexes/LSM_TREE_SPEC.md`
  - `ScratchBird/docs/specifications/indexes/LSMTimeSeriesIndex.md`
- Full-text indexes and text search primitives
  - `ScratchBird/docs/specifications/indexes/InvertedIndex.md`
  - `ScratchBird/docs/specifications/indexes/FSTIndex.md`
  - `ScratchBird/docs/specifications/indexes/SuffixIndex.md`
- Spatial types and indexes (R-Tree, Z-Order, Geohash/S2, Quadtree/Octree)
  - `ScratchBird/docs/specifications/types/MULTI_GEOMETRY_TYPES_SPEC.md`
  - `ScratchBird/docs/specifications/indexes/README.md`

## Summary Matrix (Storage Structures)

| NoSQL Model | Typical Storage Structures | ScratchBird Coverage | Beta Additions Needed |
|-------------|----------------------------|----------------------|-----------------------|
| Document | JSON/BSON docs, path indexes, doc-level TTL | JSON/JSONB + JSON_TABLE + GIN/fulltext | JSON path indexes, doc collection catalog, doc-level TTL metadata |
| Key-Value | LSM or hash table, value log, TTL | LSM, HASH index, TOAST | KV buckets, TTL per key, value-log tuning |
| Wide-column | Partition key + clustering, SSTables, tombstones | LSM, columnstore | Column-family metadata, per-cell TTL, compaction policies |
| Graph | Vertex/edge store, adjacency lists, traversal indexes | None explicit | Graph catalogs, adjacency structures, traversal engine |
| Vector | Vector type + ANN indexes | VECTOR + HNSW/IVF specs | Vector collection metadata, filter+ANN pipelines |
| Columnar | Column segments, compression, zone maps | COLUMNSTORE + ZoneMaps | Columnar table class + vectorized exec hooks |
| Time-series | Time partitioning, TTL, downsampling | LSM TTL + partitioning | Retention policies, downsampling, time-bucket catalogs |
| Search | Inverted index segments, analyzers, scoring | Inverted/FST/Suffix indexes | Analyzer registry, doc values, ranking configs |

## Model-by-Model Detail

### 1) Document Databases (MongoDB, Couchbase)

**Typical storage structures**
- Collection catalog and collection-level options (shard key, TTL, validators)
- Document storage as JSON/BSON with a stable document ID
- Secondary indexes on field paths (including nested arrays)
- TTL index that expires documents
- Optional change stream log (append-only history)

**ScratchBird coverage**
- JSON/JSONB types and JSON_TABLE (spec level)
- GIN/fulltext indexes for token and array search (spec level)

**Beta gaps and additions**
- JSON path index definition and storage layout (path -> postings list)
- Collection catalog entries (collection-level options and validators)
- Document TTL metadata and expiry sweep integration
- Change stream or audit-friendly append log (optional, Beta)

### 2) Key-Value Stores (Redis, RocksDB, DynamoDB)

**Typical storage structures**
- LSM tree or hash table with memtable + SSTables
- Value log for large values (optional)
- TTL metadata (per-key expiration)
- Bucket or namespace catalog for multi-tenant KV use

**ScratchBird coverage**
- LSM tree spec and hash indexes
- TOAST/LOB storage for large values

**Beta gaps and additions**
- KV bucket catalog (name -> key/value layout)
- TTL per key with fast expiry index
- Optional value-log layout for large values (if needed)

### 3) Wide-Column / Column-Family (Cassandra, HBase)

**Typical storage structures**
- Partition key + clustering columns
- Memtable + SSTables with tombstones
- Bloom filters + compaction strategies
- Per-cell TTL and table-level default TTL

**ScratchBird coverage**
- LSM tree spec
- Bloom filter index spec
- Columnstore spec (analytics, not the same as column-family)

**Beta gaps and additions**
- Column-family metadata (partition key, clustering order, clustering indexes)
- Per-cell TTL and tombstone visibility rules
- Compaction strategy definitions (size-tiered, leveled)
- Read repair and anti-entropy hooks (cluster-aware, Beta)

### 4) Graph Databases (Neo4j, JanusGraph)

**Typical storage structures**
- Vertex table and edge table with stable IDs
- Adjacency lists (out/in edges) for traversal speed
- Property storage for vertices and edges
- Secondary indexes on labels and properties
- Traversal cache or CSR-like adjacency blocks for large graphs

**ScratchBird coverage**
- No dedicated graph storage structures specified
- Graph can be emulated via SQL tables, but without adjacency-optimized storage

**Beta gaps and additions**
- Graph catalog objects (graphs, vertex labels, edge labels)
- Adjacency storage layout (edge lists or CSR blocks)
- Traversal index support (out-edges by label, in-edges by label)
- Graph query language parser (Cypher/Gremlin) and traversal executor

### 5) Vector Databases (Milvus, Pinecone, FAISS)

**Typical storage structures**
- Vector column type with dimension metadata
- ANN index structures (HNSW, IVF, PQ)
- Filtered ANN (metadata filters + ANN search)
- Collection-level settings (distance metric, quantization)

**ScratchBird coverage**
- VECTOR type and HNSW/IVF index specs
- Vector search operators in index specs

**Beta gaps and additions**
- Vector collection catalog (dimension, metric, ANN parameters)
- Filter+ANN execution pipeline (apply metadata filters before/after ANN)
- Vector index maintenance rules for bulk load and rebuild

### 6) Columnar Analytics (ClickHouse, Redshift, DuckDB)

**Typical storage structures**
- Columnar segments with compression codecs
- Zone maps for data skipping
- Vectorized execution and batch operators

**ScratchBird coverage**
- Columnstore spec and zone maps index spec

**Beta gaps and additions**
- Columnar table storage class (not only index)
- Vectorized execution hooks and operator batches
- Columnar compaction and merge policies

### 7) Time-Series Databases (InfluxDB, Prometheus)

**Typical storage structures**
- Time partitioning (range partition by time)
- TTL or retention policies with automated expiration
- Compression and downsampling/rollup tables

**ScratchBird coverage**
- LSM TTL spec (time-series optimized)
- BRIN/zone-map style pruning

**Beta gaps and additions**
- Retention policy catalog (per table or per partition)
- Downsampling jobs and rollup table definitions
- Time-bucket indexing rules for fast range scans

### 8) Search and Text Engines (Elasticsearch, Lucene)

**Typical storage structures**
- Inverted index segments and postings lists
- Analyzer and tokenization registry (per field)
- Term statistics and scoring models (BM25)
- Doc values / column store for aggregations

**ScratchBird coverage**
- Inverted index spec and related text indexes

**Beta gaps and additions**
- Analyzer registry catalog (per field + per language)
- Field-level scoring configurations
- Doc-values style storage for search+aggregation workloads

## Cross-Cutting Gaps to Plan for Beta

These show up across multiple NoSQL models and should be planned as shared
building blocks:

- **Metadata catalogs** for NoSQL collections/graphs/buckets
- **TTL and retention policy system** (document, key, and time series)
- **Compaction policy framework** (especially for LSM-based models)
- **Bulk load and rebuild workflows** (vector, columnar, time-series)
- **Change streams / CDC** for document and key-value use cases (optional)
- **Query language adapters** for Cypher/Gremlin/Mongo-style APIs

## Recommended Next Documents (Beta)

1. NoSQL catalog model (collections, graphs, buckets, column families)
2. JSON path indexing specification and catalog wiring
3. Graph storage layout and adjacency indexing spec
4. Retention policy and TTL system spec (shared)
5. Vector collection metadata and filtered-ANN execution spec
6. Columnar table storage class + vectorized execution hooks

