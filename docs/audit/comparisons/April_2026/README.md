# April 2026 Cross-Engine Comparison Report Set

This directory contains the April 2026 comparison batch generated from the canonical section 31 report schema and the donor list in `docs/specifications/Reference_Documentation_specification.md`.

The current batch is environment-aware. Each report compares a donor product against ScratchBird's engine core, parser boundary, catalog overlays, donor front doors, and cluster or lineage surface instead of reducing the comparison to one generic SQL-engine matrix.

## Included Database and Database-Platform Donors

| Rank | Donor | Phase | Role | Current Verdict | Target Verdict | Feature Rows | Report |
| ---: | --- | --- | --- | --- | --- | ---: | --- |
| 1 | FirebirdSQL | Beta 1 | Core MGA semantic donor | `PARTIAL_WITH_GAPS` | `FEASIBLE_WITH_MAPPING` | 48 | [firebirdsql_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](firebirdsql_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 2 | PostgreSQL | Beta 1 + Beta 2 | Core relational maturity donor | `PARTIAL_WITH_GAPS` | `PARTIAL_WITH_GAPS` | 48 | [postgresql_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](postgresql_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 7 | SQLite | Beta 1 | Pager / invariant donor | `PARTIAL_WITH_GAPS` | `FEASIBLE_WITH_MAPPING` | 48 | [sqlite_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](sqlite_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 9 | Dolt | Beta 1 | Git/database donor | `PARTIAL_WITH_GAPS` | `PARTIAL_WITH_GAPS` | 48 | [dolt_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](dolt_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 10 | DuckDB | Beta 1 | Analytical execution donor | `PARTIAL_WITH_GAPS` | `FEASIBLE_WITH_MAPPING` | 48 | [duckdb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](duckdb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 11 | FoundationDB | Beta 1 + Beta 2 | Transactional discipline and simulation donor | `PARTIAL_WITH_GAPS` | `PARTIAL_WITH_GAPS` | 48 | [foundationdb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](foundationdb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 12 | MySQL | Beta 1 + Beta 2 | Protocol / compatibility donor | `PARTIAL_WITH_GAPS` | `PARTIAL_WITH_GAPS` | 48 | [mysql_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](mysql_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 13 | MariaDB | Beta 1 + Beta 2 | Alternate MySQL-family donor | `NOT_CURRENTLY_FEASIBLE` | `PARTIAL_WITH_GAPS` | 48 | [mariadb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](mariadb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 19 | Milvus | Beta 1 + Beta 2 | Vector / ANN donor | `NOT_CURRENTLY_FEASIBLE` | `PARTIAL_WITH_GAPS` | 48 | [milvus_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](milvus_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 20 | ClickHouse | Beta 1 + Beta 2 | Columnar analytical donor | `PARTIAL_WITH_GAPS` | `PARTIAL_WITH_GAPS` | 48 | [clickhouse_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](clickhouse_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 21 | etcd | Beta 2 | Control-plane donor | `PARTIAL_WITH_GAPS` | `PARTIAL_WITH_GAPS` | 48 | [etcd_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](etcd_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 22 | TiKV | Beta 2 | Data-plane placement donor | `PARTIAL_WITH_GAPS` | `PARTIAL_WITH_GAPS` | 48 | [tikv_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](tikv_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 27 | immudb | Beta 1 | Audit / security donor | `PARTIAL_WITH_GAPS` | `PARTIAL_WITH_GAPS` | 48 | [immudb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](immudb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 28 | XTDB | Beta 1 | Temporal donor | `PARTIAL_WITH_GAPS` | `PARTIAL_WITH_GAPS` | 48 | [xtdb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](xtdb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 30 | Redis | Beta 1 + Beta 2 | Protocol and low-latency donor | `NOT_CURRENTLY_FEASIBLE` | `PARTIAL_WITH_GAPS` | 48 | [redis_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](redis_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 31 | MongoDB | Beta 1 + Beta 2 | Document donor | `NOT_CURRENTLY_FEASIBLE` | `PARTIAL_WITH_GAPS` | 48 | [mongodb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](mongodb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 32 | OpenSearch | Beta 1 + Beta 2 | Search / segment donor | `PARTIAL_WITH_GAPS` | `PARTIAL_WITH_GAPS` | 48 | [opensearch_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](opensearch_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 33 | Cassandra | Beta 1 + Beta 2 | Distributed storage foil | `NOT_CURRENTLY_FEASIBLE` | `PARTIAL_WITH_GAPS` | 48 | [cassandra_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](cassandra_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 34 | InfluxDB | Beta 1 + Beta 2 | Time-series donor | `PARTIAL_WITH_GAPS` | `PARTIAL_WITH_GAPS` | 48 | [influxdb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](influxdb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 35 | Neo4j | Beta 1 + Beta 2 | Graph donor | `NOT_CURRENTLY_FEASIBLE` | `PARTIAL_WITH_GAPS` | 48 | [neo4j_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](neo4j_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 36 | TiDB | Beta 2 | Distributed SQL donor | `NOT_CURRENTLY_FEASIBLE` | `PARTIAL_WITH_GAPS` | 48 | [tidb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](tidb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 37 | CockroachDB | Beta 2 | Distributed SQL donor | `NOT_CURRENTLY_FEASIBLE` | `PARTIAL_WITH_GAPS` | 48 | [cockroachdb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](cockroachdb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 38 | YugabyteDB | Beta 2 | Distributed SQL donor | `NOT_CURRENTLY_FEASIBLE` | `PARTIAL_WITH_GAPS` | 48 | [yugabytedb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](yugabytedb_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 39 | Citus | Beta 2 | Distributed Postgres donor | `NOT_CURRENTLY_FEASIBLE` | `PARTIAL_WITH_GAPS` | 48 | [citus_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](citus_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |
| 40 | Apache Ignite | Beta 2 | Data-grid donor | `PARTIAL_WITH_GAPS` | `PARTIAL_WITH_GAPS` | 48 | [apache_ignite_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md](apache_ignite_vs_scratchbird_cross_engine_feature_comparison_2026-04-01.md) |

## Excluded Non-Database Donors

| Donor | Reason Excluded From This Per-Database Report Batch |
| --- | --- |
| SQLancer | Testing tool, not a database product. |
| sqllogictest | Result-oracle corpus, not a database product. |
| SQLsmith | Fuzzing tool, not a database product. |
| Apache Calcite | Planner and federation framework, not a database product. |
| Debezium | CDC and migration platform, not a database product. |
| WiredTiger | Storage-engine library, not a standalone database product. |
| Substrait | Interop specification, not a database product. |
| Vitess | Sharding and control-plane layer rather than a standalone database product. |
| RocksDB | Storage-engine library, not a standalone database product. |
| LMDB | Embedded storage library, not a full database product in this batch. |
| Jepsen | Fault-testing methodology and tooling, not a database product. |
| TLA+ | Formal method, not a database product. |
| Elle | Anomaly detector, not a database product. |
| Maelstrom | Fault-simulation harness, not a database product. |
| Arrow Flight SQL | Transport protocol, not a database product. |
| Raft paper | Paper, not a database product. |
| Spanner paper | Paper, not a database product. |
| Calvin paper | Paper, not a database product. |
| Calcite paper | Paper, not a database product. |
| Differential Query Execution | Paper/topic donor, not a database product. |
| SQLancer / TLP paper | Paper, not a database product. |

## Artifacts

- manifest: [comparison_report_manifest.csv](comparison_report_manifest.csv)
- report schema authority: `docs/specifications/31_Conformance_Performance_and_Reliability_Gates/CROSS_ENGINE_FEATURE_COMPARISON_REPORT_SCHEMA_AND_PAIRWISE_COMPARABILITY_MODEL.md`
- donor control source: `docs/specifications/Reference_Documentation_specification.md`
