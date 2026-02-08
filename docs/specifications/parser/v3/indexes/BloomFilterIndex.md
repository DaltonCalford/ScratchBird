# Bloom Filter Index Specification for ScratchBird

**Storage Layout Authority:** On-disk page headers, slot arrays, free-space rules, and page-type layouts are authoritative in `../storage/PAGE_TYPES_AND_LAYOUTS.md`. Any structs here are logical field groupings; do not infer byte-accurate layout from this file.



**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)

Status: Authoritative (V3)
Last Updated: 2026-02-08

---

## Overview

Bloom Filter is an **auxiliary index** providing fast, probabilistic membership checks. It **never** returns record UUIDs by itself; it is a pre-filter that reduces work for exact indexes or heap scans. It must not be used for correctness decisions.

**Key property:** false positives allowed; false negatives are not allowed (unless misconfigured or corrupted).

---

## Authoritative Algorithm (Normative, 2026-02-07)

### Parameters

- `m`: number of bits in the filter
- `k`: number of hash functions
- `n`: expected number of inserted values
- False positive rate `p ≈ (1 - e^{-k n / m})^k`

Recommended defaults:
- `m = 10 * n` bits
- `k = round((m / n) * ln 2)`

### Hashing

Use double-hashing (Kirsch–Mitzenmacher):

```
h1 = hash64(key, seed1)
h2 = hash64(key, seed2)
for i in 0..k-1:
  idx = (h1 + i * h2) mod m
```

### Insert

1. Normalize key to bytes (same normalization as the base index for that column).
2. For i in 0..k-1, set bit `idx`.

### Query (Membership)

1. Normalize key to bytes.
2. For i in 0..k-1, check bit `idx`.
3. If any bit is 0 → **definitely not present**.
4. If all bits are 1 → **possibly present**.

### Delete

Standard Bloom filters cannot delete safely. Two options:

- **Counting Bloom Filter (optional):** maintain 4/8-bit counters and decrement on delete.
- **Rebuild (default):** ignore deletes and rebuild when GC threshold is reached.

---

## MGA Compliance

- Bloom filter does **not** store per-record metadata.
- Correctness is maintained by validating results via the base index or heap using MGA visibility rules.
- Rebuilds must scan **visible record versions** only.

## Record Identity Requirements

If a bloom filter stores any row references (reserved in V3), it must use
`record_uuid` with optional `SBRecordPtr` cache hints. Legacy TID encodings are not permitted.

---

## Data Model

### Meta Page


**Logical Fields:**

- `header` (PageHeader)
- `index_uuid` (UUID)
- `table_uuid` (UUID)
- `column_id` (uint16_t)
- `bit_count` (uint64_t): m
- `hash_count` (uint32_t): k
- `seed1` (uint32_t)
- `seed2` (uint32_t)
- `total_inserts` (uint64_t)
- `total_rebuilds` (uint64_t)
- `bitset_first_page` (uint32_t)
- `bitset_page_count` (uint32_t)
- `epoch` (uint64_t)


### Bitset Pages


**Logical Fields:**

- `header` (PageHeader)
- `next_page` (uint32_t)
- `bitset_bytes` (uint32_t): payload bytes
- `bits[]` (uint8_t)


---

## Core API

```cpp
Status bloom_insert(UUID index_uuid, const void* key, size_t key_len);
bool   bloom_maybe_contains(UUID index_uuid, const void* key, size_t key_len);
Status bloom_rebuild(UUID index_uuid, ErrorContext* ctx);
```

---

## DML Integration

- **INSERT:** call `bloom_insert` for indexed columns.
- **UPDATE:** if column value changes, call `bloom_insert` for the new value.
- **DELETE:** no action (rebuild handles stale bits).

---

## Garbage Collection

Bloom filters are rebuilt when stale or after GC triggers:

- Trigger when delete/update count exceeds `bloom_rebuild_threshold`.
- Trigger when OIT advances significantly and stale bits are likely.
- Rebuild uses a **full table scan** and MGA visibility rules.

**GC Contract:** `removeDeadEntries()` schedules a rebuild; it does not attempt to remove bits directly.

---

## Query Planner Integration

- Bloom filter can be attached to a base index (BTREE/HASH/GIN/IVF). It is a **cheap rejector**.
- Planner may apply bloom before exact index or heap scan when selectivity is low.

---

## Testing Requirements

1. False negative test (must be zero under normal operation).
2. False positive rate within configured bounds.
3. Rebuild correctness vs. MGA-visible rows.
4. Crash recovery: bitset pages validated by checksum and epoch.

---

## Reserved Enhancements (Not Supported in V3)

- Counting Bloom filter mode
- Partitioned bloom filters per segment to reduce rebuild cost
- Dynamic resize based on load factor

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
