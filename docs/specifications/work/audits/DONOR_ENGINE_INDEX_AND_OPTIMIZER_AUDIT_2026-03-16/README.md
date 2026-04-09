# Donor Engine Index and Optimizer Audit

This audit set captures how each donor engine currently plans queries and uses indexes so ScratchBird can make direct, apples-to-apples comparisons against:

- current implementation
- active optimizer/index specifications
- future parity-plus workplans

## Scope

The reviewed engines are:

- PostgreSQL
- FirebirdSQL
- MySQL
- MariaDB
- DuckDB
- ClickHouse
- Cassandra
- MongoDB
- OpenSearch
- Redis
- Neo4j
- Milvus
- InfluxDB

## Structure

- `ENGINE_COMPARISON_MATRIX.csv`
- `POSTGRESQL_AUDIT.md`
- `FIREBIRD_AUDIT.md`
- `MYSQL_AUDIT.md`
- `MARIADB_AUDIT.md`
- `DUCKDB_AUDIT.md`
- `CLICKHOUSE_AUDIT.md`
- `CASSANDRA_AUDIT.md`
- `MONGODB_AUDIT.md`
- `OPENSEARCH_AUDIT.md`
- `REDIS_AUDIT.md`
- `NEO4J_AUDIT.md`
- `MILVUS_AUDIT.md`
- `INFLUXDB_AUDIT.md`

## Reading Order

1. Read `ENGINE_COMPARISON_MATRIX.csv` for the donor map.
2. Read the per-engine audit for the exact process flow.
3. Use the “ScratchBird comparison hooks” section in each file to drive later comparison and implementation planning.

## Review Notes

- This is an implementation audit, not a marketecture survey.
- The emphasis is on actual planner and index execution flow, not marketing labels.
- Redis is audited as core Redis, not RediSearch. If ScratchBird wants RediSearch-grade comparison later, that needs its own donor pass.
- MariaDB is split conceptually between core optimizer behavior and storage-engine/plugin variability.
- ClickHouse, Cassandra, Milvus, OpenSearch, Redis, and InfluxDB are not direct MVCC SQL-planner donors in the PostgreSQL sense; they are still important donors for specific index families, pruning models, search execution, and publication/rebuild discipline.
