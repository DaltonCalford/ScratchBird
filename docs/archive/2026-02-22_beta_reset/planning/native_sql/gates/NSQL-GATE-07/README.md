# NSQL Gate-07 Execution Tracker
Last modified: 2026-02-19

## Purpose

This directory tracks execution closure for all Gate-06 mandatory-open rows.

Source baseline:

- `docs/planning/native_sql/gates/NSQL-GATE-06/NATIVE_CAPABILITY_MATRIX.csv`

## Artifacts

- `docs/planning/native_sql/gates/NSQL-GATE-07/EXECUTION_TRACKER.tsv`
- `docs/planning/native_sql/gates/NSQL-GATE-07/EXECUTION_TRACKER_OWNER_LOAD.tsv`
- `docs/planning/native_sql/gates/NSQL-GATE-07/EXECUTION_TRACKER_SPRINT_LOAD.tsv`
- `docs/planning/native_sql/gates/NSQL-GATE-07/EXECUTION_TRACKER_SUMMARY.env`

## Generation

Command:

```bash
tools/compliance/native_sql_gate07_execution_tracker.sh
```

## Owner Assignment Policy

Rows are assigned by `engine` to owner pods:

- `MySQL -> SQL-Compat-MySQL`
- `PostgreSQL -> SQL-Compat-PostgreSQL`
- `FirebirdSQL -> SQL-Compat-Firebird`
- `MariaDB -> SQL-Compat-MariaDB`
- `Cassandra -> Polyglot-CQL`
- `MongoDB -> Polyglot-Mongo`
- `Neo4j -> Polyglot-Cypher`
- `Redis -> Polyglot-Redis`
- `Milvus -> Polyglot-Milvus`
- `ClickHouse -> Analytics-ClickHouse`
- `InfluxDB -> TimeSeries-InfluxDB`
- `OpenSearch -> Search-OpenSearch`
- `DuckDB -> Analytics-DuckDB`

## Sprint Assignment Policy

Rows are assigned from `priority` and engine rank:

- `P0`: top-3 engines (`MySQL`, `PostgreSQL`, `FirebirdSQL`) -> `Sprint-3`; all other engines -> `Sprint-4`
- `P1`: top-3 engines -> `Sprint-4`; ranks 4-9 -> `Sprint-5`; ranks 10+ -> `Sprint-6`
- `P2`: all engines -> `Sprint-6`

## Current Snapshot

- total rows: `141`
- `P0=52`, `P1=69`, `P2=20`
- `Sprint-3=44`, `Sprint-4=35`, `Sprint-5=26`, `Sprint-6=36`
