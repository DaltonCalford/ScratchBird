# OID Mapping Strategy (PostgreSQL Emulation)

**Status:** Draft (Alpha)
**Last Updated:** 2026-01-20
**Scope:** PostgreSQL OID mapping for emulated system views and catalogs

---

## 1) Purpose

PostgreSQL exposes 32-bit OIDs for catalogs and monitoring views. ScratchBird
uses UUIDs internally, so a stable mapping is required for PostgreSQL parity.

This document defines how UUID-backed objects map to OIDs for:
- `pg_stat_*` views
- `pg_catalog` system views
- `information_schema` compatibility

---

## 2) Scope

**In scope:**
- Database, schema, table, index, column OID mapping
- Monitoring view OIDs (pg_stat_* columns)
- Stable OID persistence across restarts

**Out of scope:**
- User-defined OID assignment DDL
- Cross-node global OID allocation (cluster-only)

---

## 3) Core Rules

1. OIDs are **stable** within a database.
2. OIDs are **deterministic** for the same UUID (with collision resolution).
3. OIDs are **not** reused until explicitly reclaimed.
4. OID `0` means "unknown" and must not be assigned.

---

## 4) Mapping Algorithm

### 4.1 Deterministic OID Seed

- Compute a 32-bit hash of the UUID using `xxHash32` with a fixed seed.
- If the hash is `0`, increment by 1.

### 4.2 Collision Resolution

- If the computed OID is already assigned to another UUID, probe by incrementing
  (`oid = (oid + 1) % 2^32`) until an unassigned OID is found.
- Store the final OID in the mapping table for persistence.

### 4.3 Reserved Ranges

- `1..10000`: reserved for system objects (bootstrapped at init)
- `10001..` for user objects

---

## 5) Catalog Storage

Add a mapping table to the system catalog:

```
struct OidMappingRecord {
    ID object_uuid;            // UUID of the object
    uint32_t oid;              // 32-bit OID
    uint16_t object_type;      // DATABASE|SCHEMA|TABLE|INDEX|COLUMN|VIEW
    uint64_t created_time;
    uint32_t is_valid;
};
```

**Table name:** `pg_oid_map` (system view) backed by `sys.oid_map`.

---

## 6) Exposure Rules

- `pg_stat_*` views use mapped OIDs for `datid`, `relid`, and related columns.
- If no OID mapping exists and mapping is disabled, return `NULL`.
- Emulated queries may request OID mapping on-demand; map lazily on first use.

---

## 7) Monitoring Integration

Mapping is required for:
- `pg_stat_activity.datid`
- `pg_locks.database`, `pg_locks.relation`
- `pg_stat_database.datid`
- `pg_stat_all_tables.relid`

See `MONITORING_DIALECT_MAPPINGS.md` for per-column mapping.

---

## 8) Security

- OID mapping is internal and read-only.
- External users cannot assign or change OIDs.

---

## 9) Related Specifications

- `docs/specifications/operations/MONITORING_DIALECT_MAPPINGS.md`
- `docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`
- `docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
