# UDR Connector Factory and Remote Engine Degraded Mode Model

## Purpose

Define the canonical connector-factory and capability-advertisement model for remote engines and UDR-backed data access.

## Connector Factory Contract

The connector factory maps:

- connector type enum
- canonical string names
- connection-string schemes

into a concrete connector implementation when supported.

## Supported Connector Set

Current code-backed supported families include:

- `POSTGRESQL`
- `MYSQL`
- `FIREBIRD`
- `SCRATCHBIRD`
- `CASSANDRA`
- `CLICKHOUSE`
- `DUCKDB`
- `INFLUXDB`
- `MARIADB`
- `MILVUS`
- `MONGODB`
- `NEO4J`
- `OPENSEARCH`
- `REDIS`

`ODBC` is currently a recognized type but not a creatable runtime connector in the current factory.

## Capability Surface

Bootstrap scaffold connectors already advertise a minimum remote-engine capability set, including:

- metadata snapshot
- query passthrough
- prepared lifecycle
- show, describe, and comment surfaces
- degraded mode
- signoff readiness

## Degraded Mode

Degraded mode is a first-class connector capability. A connector may remain admissible for metadata or bounded passthrough even when stronger capabilities are absent.

## Migration Relationship

For engines that do not support natural replication or cannot be forced into a ScratchBird-style transactional streaming model, the connector capability set governs what migration strategy is possible:

- metadata-first discovery
- query passthrough
- staged copy
- prepared or audited bounded execution
- degraded inspection-only mode

The engine shall not assume replication simply because a remote connector exists.
