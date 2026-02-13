# OID Mapping Strategy (PostgreSQL Emulation)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

PostgreSQL exposes 32-bit OIDs for catalogs and monitoring views. ScratchBird
uses UUIDs internally, so a stable mapping is required for PostgreSQL parity.

This document defines how UUID-backed objects map to OIDs for:
- `pg_stat_*` views
- `pg_catalog` system views
- `information_schema` compatibility

## Scope

In scope:
- Database, schema, table, index, column OID mapping
- Monitoring view OIDs (pg_stat_* columns)
- Stable OID persistence across restarts

Out of scope:
- User-defined OID assignment DDL (rejected in V3)
- Cross-node global OID allocation (rejected in V3)

## Core Rules

1. OIDs are stable within a database.
2. OIDs are deterministic for the same UUID (with collision resolution).
3. OIDs are not reused until explicitly reclaimed.
4. OID `0` means "unknown" and MUST NOT be assigned.

## Mapping Algorithm

### Deterministic OID Seed

- Compute a 32-bit hash of the UUID using `xxHash32` with a fixed seed.
- If the hash is `0`, increment by 1.

### Collision Resolution

- If the computed OID is already assigned to another UUID, probe by incrementing:
  `oid = (oid + 1) % 2^32` until an unassigned OID is found.
- Store the final OID in the mapping table for persistence.

### Reserved Ranges

- `1..10000`: reserved for system objects (bootstrapped at init)
- `10001..`: user objects

## Catalog Storage

The mapping table is stored in the system catalog as `sys.oid_map`.

Logical schema for the mapping table:
- `object_uuid` (UUID v7)
- `oid` (u32)
- `object_type` (enum: DATABASE|SCHEMA|TABLE|INDEX|COLUMN|VIEW)
- `created_time` (u64)
- `is_valid` (bool)

## Exposure Rules

- `pg_stat_*` views use mapped OIDs for `datid`, `relid`, and related columns.
- If no OID mapping exists and mapping is disabled, return `NULL`.
- Emulated queries may request OID mapping on-demand; map lazily on first use.

## Related Specs

- `docs/specifications/parser/v3/operations/MONITORING_DIALECT_MAPPINGS.md`
- `docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_STRUCTURE.md`
- `docs/specifications/parser/v3/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
