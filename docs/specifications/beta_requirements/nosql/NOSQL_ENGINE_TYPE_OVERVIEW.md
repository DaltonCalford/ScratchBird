# NoSQL Engine Types: Detailed Overview and Competitive Landscape

**Status:** Draft (Beta)

## 1. Purpose

Provide a verbose, detailed description of each major NoSQL engine type,
including what problems it solves, which products implement the model, and
how the type competes against the others. This is a strategic reference for
deciding which NoSQL paradigms ScratchBird should emulate or integrate.

## 2. Scope

Engine types covered:

- Document stores
- Key-value stores
- Wide-column / column-family stores
- Graph databases (property graph)
- RDF triple stores (SPARQL)
- Search engines (full-text + ranking)
- Time-series databases (TSDB)
- Vector databases
- Columnar analytics stores
- Multi-model databases (hybrid engines)

Each section includes:
- **What it is**
- **Problems solved / strengths**
- **Limitations**
- **Representative products** (open-source and leading proprietary)
- **Competitive dynamics** vs other engine types

## 3. Document Stores

### What it is
Document databases store data as JSON/BSON-like documents with flexible or
schema-light structure. A document is a nested map of fields and values.

### Problems solved / strengths
- Rapid development for evolving schemas (no strict table definition).
- Natural modeling of nested structures and arrays.
- Application-friendly: JSON is a native exchange format.
- Indexing on nested fields and array elements.

### Limitations
- Join-heavy workloads are less natural (often avoided or limited).
- Schema flexibility can hide data quality issues if not validated.
- Cross-document transactions vary by product.

### Representative products
**Open source:** MongoDB (community server), CouchDB, Couchbase (community),
ArangoDB (multi-model), RavenDB (source-available).

**Proprietary/managed:** MongoDB Atlas, AWS DocumentDB, Azure Cosmos DB.

### Competitive dynamics
- Strong vs relational for schema-flexible domains.
- Weaker than graph DBs for multi-hop relationships.
- Often overlaps with search engines for text search, but relies on external
  search systems for advanced relevance ranking.

### ScratchBird positioning
- **Near-term fit:** JSON/JSONB + JSON_TABLE covers document storage; JSON path
  indexing and missing/null semantics are Beta gaps.
- **Differentiator:** MGA-based MVCC with UUID objects provides stronger
  consistency controls than typical document stores.

## 4. Key-Value Stores

### What it is
A simple map from key to value. Values are usually opaque blobs or small
structured objects. The model emphasizes speed and scalability.

### Problems solved / strengths
- Extremely fast reads/writes with simple access patterns.
- Low overhead and high throughput.
- Ideal for caching, session storage, and counters.

### Limitations
- Limited query capability beyond key lookup.
- Secondary indexing is often restricted or external.

### Representative products
**Open source:** Redis, KeyDB, LevelDB/RocksDB (embedded), Memcached.

**Proprietary/managed:** DynamoDB (Amazon), Aerospike, Azure Cache.

### Competitive dynamics
- Faster and simpler than document or wide-column systems when access is
  key-based.
- Loses when rich queries or joins are required.

### ScratchBird positioning
- **Near-term fit:** LSM + TOAST + KV buckets (spec) can emulate KV workloads.
- **Differentiator:** Integrated SQL/PSQL tooling for KV metadata and auditing.

## 5. Wide-Column / Column-Family Stores

### What it is
A model where data is organized by partition key and clustering columns.
Each row can hold many sparse columns in column families. Optimized for
large-scale, distributed storage.

### Problems solved / strengths
- High write throughput for time-ordered or partitioned data.
- Horizontal scalability with shard/partition key.
- Efficient range scans within partition.

### Limitations
- Query patterns must follow partition/clustering design.
- Joins are typically unsupported.
- Tombstone management and compaction overhead.

### Representative products
**Open source:** Apache Cassandra, ScyllaDB, Apache HBase.

**Proprietary/managed:** Google Bigtable, Amazon DynamoDB (hybrid model).

### Competitive dynamics
- Outperforms document stores at large-scale write-heavy workloads with
  predictable key access.
- Less flexible than document or relational for ad-hoc queries.

### ScratchBird positioning
- **Near-term fit:** LSM tree + planned column-family catalog and TTL.
- **Differentiator:** Unified catalog + security with SQL-accessible metadata.

## 6. Graph Databases (Property Graph)

### What it is
Nodes and relationships with properties; optimized for traversals and
pattern matching.

### Problems solved / strengths
- Multi-hop queries and relationship-heavy domains (social, fraud, network).
- Fast traversals without expensive joins.
- Natural graph pattern semantics (Cypher, Gremlin).

### Limitations
- Storage and indexing can be complex at scale.
- Analytics over large graphs can be expensive without specialized engines.

### Representative products
**Open source:** Neo4j (community), JanusGraph, Apache TinkerPop,
ArangoDB (multi-model).

**Proprietary/managed:** Neo4j Enterprise, TigerGraph, AWS Neptune.

### Competitive dynamics
- Dominates for relationship-centric queries.
- Outperformed by wide-column for write-heavy time-series workloads.
- Often combined with search engines for text search.

### ScratchBird positioning
- **Near-term fit:** Requires graph catalogs + traversal engine (Beta work).
- **Differentiator:** Co-located SQL + graph in one engine, shared security.

## 7. RDF Triple Stores (SPARQL)

### What it is
Stores data as RDF triples (subject, predicate, object). Optimized for
semantic web and knowledge graphs, queried via SPARQL.

### Problems solved / strengths
- Semantic reasoning, ontology-driven queries.
- Flexible schema via RDF vocabularies.
- Graph pattern matching with standardized W3C syntax.

### Limitations
- Often slower than property graph engines for traversal-heavy workloads.
- More complex modeling for application developers.

### Representative products
**Open source:** Apache Jena/Fuseki, Blazegraph, RDF4J.

**Proprietary/managed:** Stardog, Ontotext GraphDB, Virtuoso.

### Competitive dynamics
- Best for semantic reasoning and ontology alignment.
- Less competitive for operational graph workloads where Cypher/Gremlin
  are more developer-friendly.

### ScratchBird positioning
- **Near-term fit:** Requires RDF storage and SPARQL algebra (Beta work).
- **Differentiator:** Potential hybrid RDF + SQL for mixed workloads.

## 8. Search Engines (Full-Text + Ranking)

### What it is
Text indexing and relevance ranking engines built on inverted indexes and
scoring models (e.g., BM25).

### Problems solved / strengths
- Fast full-text search across large corpora.
- Relevance ranking, fuzziness, autocomplete.
- Aggregations and faceted navigation.

### Limitations
- Not transactional in the same way as OLTP engines.
- Complex indexing pipelines and schema mapping.

### Representative products
**Open source:** Elasticsearch, OpenSearch, Apache Solr, Meilisearch, Typesense.

**Proprietary/managed:** Algolia, Elastic Cloud, AWS OpenSearch Service.

### Competitive dynamics
- Dominates for text relevance; relational DBs lag without extensions.
- Often paired with document stores or relational DBs.

### ScratchBird positioning
- **Near-term fit:** Inverted index spec exists; DSL parser and scoring are
  Beta gaps.
- **Differentiator:** Combine full-text search with transactional MGA storage.

## 9. Time-Series Databases (TSDB)

### What it is
Optimized for time-ordered data points with timestamps, tags, and numeric
fields. Provides efficient compression and range queries.

### Problems solved / strengths
- High-ingest metrics and telemetry data.
- Efficient time-window aggregations.
- Retention policies and downsampling.

### Limitations
- Limited support for general-purpose relational features.
- Joins and multi-dimensional analytics can be constrained.

### Representative products
**Open source:** InfluxDB, Prometheus, VictoriaMetrics, QuestDB.

**Proprietary/managed:** Datadog, New Relic, Splunk Observability.

### Competitive dynamics
- Best for metrics and telemetry.
- Less suitable for general document or graph use cases.

### ScratchBird positioning
- **Near-term fit:** LSM TTL spec + time bucket indexing needed.
- **Differentiator:** Unified SQL/PSQL analytics and strong transaction model.

## 10. Vector Databases

### What it is
Databases specialized for nearest-neighbor search on high-dimensional vectors
with optional metadata filters.

### Problems solved / strengths
- Semantic search and AI retrieval (embeddings).
- Approximate nearest neighbor (ANN) at scale.

### Limitations
- Not designed for complex transactional workloads.
- Requires careful index tuning and data lifecycle management.

### Representative products
**Open source:** Milvus, Weaviate, Qdrant, Chroma.

**Proprietary/managed:** Pinecone, Azure AI Search (vector), AWS Kendra.

### Competitive dynamics
- Superior for embedding similarity queries.
- Often combined with relational/document DBs for metadata.

### ScratchBird positioning
- **Near-term fit:** VECTOR type + ANN index specs exist; collection metadata
  and search pipeline are Beta gaps.
- **Differentiator:** Co-located vector search with SQL/PSQL and MGA storage.

## 11. Columnar Analytics Stores

### What it is
Column-oriented storage optimized for OLAP queries, compression, and
vectorized execution.

### Problems solved / strengths
- High-performance aggregations over large datasets.
- Compression and scan efficiency.
- Ideal for analytics and reporting.

### Limitations
- Less optimized for high-write OLTP workloads.
- Complex indexing and merge logic for mixed workloads.

### Representative products
**Open source:** ClickHouse, Apache Druid, DuckDB.

**Proprietary/managed:** Amazon Redshift, Snowflake, BigQuery.

### Competitive dynamics
- Outperforms row stores for analytics workloads.
- Not suited for transactional/OLTP use cases.

### ScratchBird positioning
- **Near-term fit:** Columnstore spec exists; vectorized exec is Beta work.
- **Differentiator:** Unified OLTP + OLAP within a single engine boundary.

## 12. Multi-Model Databases

### What it is
Engines that support multiple NoSQL models in a single system (document,
key-value, graph, or search).

### Problems solved / strengths
- Unified platform with fewer moving parts.
- Cross-model queries in a single engine.

### Limitations
- Complexity can lead to compromises in performance.
- Risk of weaker best-in-class features in any one model.

### Representative products
**Open source:** ArangoDB, OrientDB.

**Proprietary/managed:** Azure Cosmos DB, MarkLogic.

### Competitive dynamics
- Best when operational simplicity matters more than specialization.
- Specialized engines usually outperform in their niche.

### ScratchBird positioning
- **Near-term fit:** Multi-model via parser and catalog layering (Beta).
- **Differentiator:** Emulated dialects with unified security + MGA.

## 13. Competitive Summary (High-Level)

- **Document vs Wide-Column**: Document is flexible; wide-column wins at
  scale for predictable partitioned access.
- **Graph vs Document**: Graph dominates complex relationship traversal;
  document is simpler for nested entities.
- **Search vs Document**: Search engines dominate relevance ranking; document
  DBs handle CRUD and schema evolution.
- **Time-series vs Wide-Column**: TSDBs add retention and compression;
  wide-column offers general partitioned storage.
- **Vector vs Search**: Vector excels at semantic similarity; search excels
  at lexical matching and scoring.
- **Columnar vs Row/Document**: Columnar excels in analytics; row/document
  excels in transactional workloads.
