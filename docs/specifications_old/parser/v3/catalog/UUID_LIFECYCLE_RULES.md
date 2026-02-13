# Catalog UUID Lifecycle Rules (Authoritative)

Date: 2026-02-08  
Status: Authoritative (V3)

Purpose: define how UUID v7 identifiers are generated, persisted, ordered, and
validated for all system catalog objects.

## 1) Generation Scope

- All `SBDB$KEY_*` values are UUID v7, backed by the `SBDB$UUID_V7` domain.
- IDs are generated at **database creation time** for catalog bootstrap rows.
- New catalog objects (tables, indexes, constraints, users, etc.) receive a new
  UUID v7 at create time; IDs MUST NOT be reused.

## 2) UUID v7 Construction (RFC 9562)

All UUID v7 values MUST follow the RFC 9562 layout:

- 48-bit Unix epoch milliseconds (big-endian).
- 4-bit version field set to `0b0111` (version 7).
- 12-bit monotonic counter (or sub-millisecond counter).
- 2-bit variant `0b10`.
- 62 bits of random data.

### Monotonicity Rules

- Maintain a `(last_ms, last_counter)` pair per database instance.
- If `now_ms > last_ms`: set `counter = 0` and `last_ms = now_ms`.
- If `now_ms == last_ms`: increment `counter` by 1.
- If `counter` overflows (4096 values per ms): set `last_ms = last_ms + 1`
  and `counter = 0`.
- If `now_ms < last_ms` (clock rollback): treat `now_ms = last_ms`
  and increment `counter`.

These rules guarantee strictly monotonic UUIDs within a single instance.

## 3) Global Uniqueness

- IDs are unique within a database.
- Even with RFC 9562 randomness, collisions MUST be prevented by catalog
  primary keys and uniqueness checks at write time.

## 4) Persistence and Reuse

- IDs are persisted in catalog storage and never re-assigned.
- DROP does not recycle IDs; deleted rows retain their ID for audit/history
  tables if enabled.

## 5) Replication Semantics

- Replication MUST preserve IDs exactly.
- If a conflict arises (duplicate UUID), replication MUST abort with
  `SBX-CONSTRAINT-UNIQUE` and surface the conflicting object path.

## 6) Cross-Database & Cluster

- IDs are not required to be unique across different databases, but SHOULD be
  treated as globally unique identifiers for federation.
- When exporting/importing catalog objects, IDs MUST be preserved unless the
  import explicitly requests remap (not supported in V3).

## 7) Storage Encoding and Ordering

- Store UUIDs as 16 bytes in network byte order (big-endian).
- Compare UUIDs lexicographically by byte order to preserve time ordering
  of UUID v7 values.

## 8) Validation Rules

- Every `SBDB$KEY_*` column MUST validate:
  - Version field == 7.
  - Variant == RFC 4122 `0b10`.
  - Timestamp is within a reasonable window (no earlier than 1970-01-01).
- Non-v7 UUIDs MUST be rejected on catalog writes.
- Invalid UUID text representations MUST return SQLSTATE `22P02`.
