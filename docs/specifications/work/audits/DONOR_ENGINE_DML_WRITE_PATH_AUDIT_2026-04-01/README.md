# Donor Engine DML Write-Path Audit

This audit set captures how each locally staged donor engine optimizes insert, update, delete, and index-maintenance work for speed, efficiency, and reliability. The goal is not to copy any donor literally. The goal is to extract the strongest write-path ideas and turn them into a concrete ScratchBird concept path that can meet or exceed the donors on DML performance while preserving ScratchBird's own cluster-native, non-destructive transaction model.

## Scope

Reviewed donor engines:

- PostgreSQL
- FirebirdSQL
- MySQL
- MariaDB
- MongoDB
- Cassandra
- ClickHouse
- DuckDB
- OpenSearch
- Neo4j
- Redis
- Milvus
- InfluxDB

## Method

- Research source: local donor engine clones under the ScratchBird reference corpus.
- Emphasis: actual write-path and index-maintenance code paths, plus local architecture docs when the donor repo provides them.
- Questions asked of every donor:
  - How are inserts made cheap?
  - How are updates and deletes kept correct without making the foreground path too expensive?
  - How is index maintenance reduced, deferred, compressed, batched, or published safely?
  - What is the donor's publication barrier between "work in progress" and "durable/queryable"?
  - Which parts fit ScratchBird's architecture and which parts do not?

## File Set

- `DML_WRITE_PATH_ENGINE_COMPARISON_MATRIX.csv`
- `SCRATCHBIRD_DML_BEST_WAY_SYNTHESIS.md`
- `POSTGRESQL_DML_WRITE_PATH_AUDIT.md`
- `FIREBIRD_DML_WRITE_PATH_AUDIT.md`
- `MYSQL_DML_WRITE_PATH_AUDIT.md`
- `MARIADB_DML_WRITE_PATH_AUDIT.md`
- `MONGODB_DML_WRITE_PATH_AUDIT.md`
- `CASSANDRA_DML_WRITE_PATH_AUDIT.md`
- `CLICKHOUSE_DML_WRITE_PATH_AUDIT.md`
- `DUCKDB_DML_WRITE_PATH_AUDIT.md`
- `OPENSEARCH_DML_WRITE_PATH_AUDIT.md`
- `NEO4J_DML_WRITE_PATH_AUDIT.md`
- `REDIS_DML_WRITE_PATH_AUDIT.md`
- `MILVUS_DML_WRITE_PATH_AUDIT.md`
- `INFLUXDB_DML_WRITE_PATH_AUDIT.md`

## Reading Order

1. Read `DML_WRITE_PATH_ENGINE_COMPARISON_MATRIX.csv` for the donor map.
2. Read `SCRATCHBIRD_DML_BEST_WAY_SYNTHESIS.md` for the combined concept path.
3. Read the per-engine audit files when a specific donor subsystem needs to be borrowed or challenged.

## Main Conclusion

There is no single donor that should be copied wholesale.

- Firebird is the strongest donor for exact MGA-owned visibility and index truth.
- PostgreSQL is the strongest donor for update-time index churn suppression and AM-specific maintenance rules.
- MySQL is the strongest donor for secondary-index deferral on cold pages.
- MongoDB is the strongest donor for online shadow-build, drain, and publish workflows.
- Cassandra, ClickHouse, Milvus, and InfluxDB are the strongest donors for immutable-generation publish and background consolidation.
- DuckDB is the strongest donor for checkpoint-time delta merge.
- OpenSearch is the strongest donor for tiny durable checkpoint metadata around a heavier write buffer.
- Neo4j is the strongest donor for batched index-update application.
- Redis is the strongest donor for adaptive in-memory encodings and lazy reclamation.

ScratchBird should combine those ideas around its own existing strengths:

- non-destructive row/version lineage
- always-in-transaction semantics
- sweep/archive pipeline
- cluster-native UUID identity
- parser/emulation separation
- multi-family index framework

That combined model is laid out in `SCRATCHBIRD_DML_BEST_WAY_SYNTHESIS.md`.
