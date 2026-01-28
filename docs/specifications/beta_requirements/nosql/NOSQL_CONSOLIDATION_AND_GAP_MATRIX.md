# NoSQL Consolidation and ScratchBird Gap Matrix

**Status:** Draft (Beta)

## 1. Purpose

Normalize terminology and feature expectations across the NoSQL language specs
and compare them against current ScratchBird capabilities. The goal is to
identify what can be supported via existing infrastructure versus what must be
built.

## 2. Normalized Terminology

| Normalized Term | Document (MQL/Mango/N1QL/AQL) | Wide-Column (CQL/HBase) | Graph (Cypher/Gremlin/SPARQL) | Search (DSL/Lucene) | Time-Series (InfluxQL/Flux/PromQL) | Vector (Milvus/Weaviate) |
|----------------|--------------------------------|--------------------------|-------------------------------|---------------------|-------------------------------------|--------------------------|
| Collection | collection | table | graph/vertex set | index | measurement | class/collection |
| Document | document | row | vertex/edge | document | point | object |
| Field/Property | field | column | property | field | field/tag | property |
| Filter | selector/WHERE | WHERE | WHERE/FILTER | query/filter | WHERE/label match | where |
| Projection | fields/RETURN | SELECT | RETURN/values | _source fields | SELECT | field list |
| Aggregation | group | GROUP BY | group/aggregate | aggs | aggregateWindow/rate | group/aggregate |
| Join/Traversal | join/UNNEST | N/A | traversal | N/A | N/A | nearVector |
| Pagination | limit/skip | LIMIT | SKIP/LIMIT | from/size | LIMIT | limit/offset |

## 3. ScratchBird Infrastructure Inventory (Relevant)

### 3.1 Storage and Catalog
- MGA storage engine with catalog-backed SQL objects.
- JSON/JSONB types (JSONB stored as text in Alpha).
- VECTOR type and ANN index specs (HNSW, IVF).
- LSM tree + LSM TTL spec for time-series workloads.
- Columnstore + zone maps specs.
- Full-text inverted index spec.
- Tablespaces and catalog-based metadata storage.

### 3.2 Query and Execution
- Native SQL + PSQL + SBLR bytecode.
- JSON_TABLE for document shredding.
- Parser remapping layer for emulated dialects.
- UDR system for external integration.

### 3.3 Monitoring and Security
- sys.* monitoring views (spec-driven).
- Permissions and dependency tracking are UUID-based.

## 4. Capability Mapping by Dialect

| Dialect | Core Requirements | ScratchBird Mapping | Gaps / New Work | Priority |
|---------|-------------------|---------------------|-----------------|----------|
| MongoDB MQL | JSON filter, update ops, aggregation pipeline | JSON/JSONB + parser layer | Aggregation pipeline operators, array semantics, missing vs null | P0 |
| CouchDB Mango | Selector, index hints, pagination | JSON/JSONB + JSON path index (Beta) | Mango selector semantics, index planner | P1 |
| Couchbase N1QL | SQL++ with UNNEST, USE KEYS, META() | SQL parser + JSON_TABLE | Missing vs null semantics, META fields, UNNEST/NEST support | P1 |
| ArangoDB AQL | Pipeline, graph traversal | SQL + graph storage (planned) | Graph catalogs, traversal engine | P2 |
| Redis RESP | RESP framing, command set | Network listener + KV storage | RESP protocol adapter, KV buckets, TTL | P0 |
| Cassandra CQL | Partition key, TTL, clustering | LSM + catalog schema | Column-family metadata, per-cell TTL, compaction policies | P1 |
| HBase Shell | Admin + scan filters | Storage + admin DDL | Filter language, HBase-style scans | P2 |
| Cypher | Pattern matching, graph update | Graph catalog + traversal engine | Graph storage and execution | P2 |
| Gremlin | Traversal steps | Graph catalog + traversal engine | Traverser engine, step semantics | P2 |
| SPARQL | RDF triples, graph patterns | Graph catalog + triple store | RDF storage, SPARQL algebra | P2 |
| Elasticsearch DSL | Full-text query + aggs | Inverted index + analytics | DSL parser, scoring models, doc values | P1 |
| Lucene query | Query syntax | Inverted index | Query parser, analyzer registry | P1 |
| InfluxQL | Time-series SELECT + GROUP BY time | LSM TTL + SQL | Measurement/time semantics, tag indexing | P0 |
| Flux | Pipe-based functions | UDR or parser layer | Flux parser, function runtime | P2 |
| PromQL | Metric selectors, rate, vector ops | Metrics engine + TS storage | PromQL parser, vector matching | P0 |
| Milvus | Vector search + filters | VECTOR + ANN indexes | Search pipeline, collection metadata | P1 |
| Weaviate | GraphQL + vector search | Vector + GraphQL layer | GraphQL parser, hybrid ranking | P2 |

## 5. Consolidated Gap Categories

### 5.1 Core Engine Gaps (Beta)
- Graph storage layout and traversal engine.
- JSON path index (spec exists, not implemented).
- Column-family metadata + per-cell TTL.
- Vector collection metadata and filtered ANN pipeline.
- Time-series measurement/tag model and retention policies.

### 5.2 Parser/Language Gaps (Beta)
- MongoDB pipeline operators and update modifiers.
- N1QL SQL++ extensions (UNNEST/NEST, USE KEYS).
- AQL pipeline semantics.
- Cypher/Gremlin/SPARQL full parser stacks.
- Elasticsearch DSL and Lucene parsers.
- Flux and PromQL parsers.
- GraphQL subset for Weaviate.

### 5.3 Execution and Semantics Gaps
- Missing vs null semantics in document models.
- Array matching semantics (elemMatch, ANY/EVERY).
- Scoring/ranking for full-text and vector search.
- Consistent pagination semantics across APIs.

## 6. What Can Be Handled by Existing Infrastructure

- **JSON storage and basic filtering** via JSON/JSONB and SQL predicates.
- **Vector similarity search** via HNSW/IVF once collection metadata exists.
- **Time-series retention** via LSM TTL spec (needs measurement/tag semantics).
- **Full-text search** via inverted index (needs parser + analyzers).
- **Table-backed emulation** for many document and wide-column workloads.

## 7. What Must Be Built

- Graph catalogs, adjacency storage, and traversal engine.
- JSON path index implementation and planner wiring.
- Column-family modeling and compaction policies.
- Dedicated query parsers and semantic layers for non-SQL languages.
- Scoring and ranking engines for search DSLs.

## 8. Recommendations (Beta Sequencing)

1. **Document model foundations**: JSON path index + missing/null semantics.
2. **Vector + search**: vector collection metadata + search pipeline.
3. **Time-series**: measurement/tag model + PromQL/InfluxQL parser surface.
4. **Graph**: catalog + storage + traversal engine (Cypher/Gremlin/SPARQL).
