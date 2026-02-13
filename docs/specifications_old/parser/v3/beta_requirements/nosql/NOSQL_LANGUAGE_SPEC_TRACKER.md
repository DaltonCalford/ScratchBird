# NoSQL Language Spec Tracker (Beta)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Status:** Active

This tracker records specification status for NoSQL language dialects.

**Reject policy (mandatory):** All NoSQL dialects are optional extensions.
If a dialect is disabled, the parser MUST reject statements with
`ERR_FEATURE_DISABLED` and MUST NOT attempt partial parsing.

| Language | Path | Status | Batch | Notes |
|----------|------|--------|-------|-------|
| MongoDB MQL | languages/mongodb_mql/SPECIFICATION.md | Authoritative | Batch 1 | Core query/update + aggregation pipeline |
| CouchDB Mango | languages/couchdb_mango/SPECIFICATION.md | Authoritative | Batch 1 | Selector + index + pagination |
| Couchbase N1QL | languages/couchbase_n1ql/SPECIFICATION.md | Authoritative | Batch 2 | SQL++ with UNNEST/USE KEYS |
| ArangoDB AQL | languages/arangodb_aql/SPECIFICATION.md | Authoritative | Batch 2 | Graph + document mix |
| Redis RESP | languages/redis_resp/SPECIFICATION.md | Authoritative | Batch 3 | Command grammar + RESP framing |
| Cassandra CQL | languages/cassandra_cql/SPECIFICATION.md | Authoritative | Batch 3 | CQL DDL/DML + TTL |
| HBase Shell | languages/hbase_shell/SPECIFICATION.md | Authoritative | Batch 3 | Shell commands + filters |
| Cypher/openCypher | languages/cypher/SPECIFICATION.md | Authoritative | Batch 4 | Pattern matching + updates |
| Gremlin | languages/gremlin/SPECIFICATION.md | Authoritative | Batch 4 | Traversal steps |
| SPARQL | languages/sparql/SPECIFICATION.md | Authoritative | Batch 4 | RDF graph queries |
| Elasticsearch DSL | languages/elasticsearch_dsl/SPECIFICATION.md | Authoritative | Batch 5 | Query DSL + aggregations |
| Lucene Query | languages/lucene_query_syntax/SPECIFICATION.md | Authoritative | Batch 5 | Term/phrase/range syntax |
| InfluxQL | languages/influxql/SPECIFICATION.md | Authoritative | Batch 6 | Time-series SQL |
| Flux | languages/flux/SPECIFICATION.md | Authoritative | Batch 6 | Pipe-based language |
| PromQL | languages/promql/SPECIFICATION.md | Authoritative | Batch 6 | Metrics queries |
| Milvus Query | languages/milvus_query/SPECIFICATION.md | Authoritative | Batch 7 | Vector search SQL |
| Weaviate GraphQL | languages/weaviate_graphql/SPECIFICATION.md | Authoritative | Batch 7 | GraphQL-based vector search |
