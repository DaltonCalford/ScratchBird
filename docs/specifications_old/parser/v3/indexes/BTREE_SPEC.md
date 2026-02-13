# B-Tree Index Specification for ScratchBird

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

ScratchBird B-Tree is the primary ordered index. It supports equality, range, and ordered scans and is fully MGA‑compliant using record UUIDs.

---

## Authoritative Algorithm (Normative, 2026-02-07)

### On-Disk Page Layout


**Logical Fields:**

- `base` (SBIndexPageHeader)
- `level` (uint16_t): 0 = leaf
- `count` (uint16_t): number of entries
- `free_lower` (uint16_t): start of free space
- `free_upper` (uint16_t): end of free space
- `right_sibling` (uint64_t): page id of right sibling (0 if none)
- `left_sibling` (uint64_t): page id of left sibling (0 if none)
- `key_len` (uint16_t)
- `flags` (uint16_t): ENTRY_DELETED, ENTRY_HAS_PREFIX
- `payload_len` (uint32_t): bytes of payload
- `data[]` (uint8_t): key bytes + payload
- `meta` (SBIndexEntryMeta): record_uuid, record_txn, flags
- `child_page` (uint64_t): child page id


### Key Encoding

- Keys are encoded in **binary comparable** form.
- For multi-column keys, concatenate field encodings with length prefix.

### Search (Point / Range)

1. Descend from root to leaf:
   - For internal node, binary search keys to pick child.
2. At leaf, binary search entries.
3. For each matching key, filter by MGA visibility:
   - resolve record header by UUID
   - `sb_find_visible_version`
4. Range scan: follow right sibling links, emitting visible entries.

### Insert

1. Descend to leaf.
2. Insert entry into leaf in sorted order.
3. If leaf overflows:
   - Split leaf at median.
   - Promote separator key to parent.
   - Update sibling links.
4. Recursively split internal nodes if needed.

### Delete

1. Create deleted record version in heap (`RHD_DELETED`).
2. Insert new leaf entry for deleted version (logical delete).
3. Old entries remain until sweep.

### Merge / Rebalance

- If a node underflows below `min_fill`:
  - Try to borrow from sibling.
  - Otherwise merge with sibling and delete separator from parent.
- Rebalance proceeds upward.

---

## MGA Compliance

- Leaf entries store `SBIndexEntryMeta` for each record version.
- Visibility is determined by TIP and record headers.
- Index GC removes entries only after OIT advance.

---

## Locking / Concurrency

- Use latch coupling (crabbing):
  - Acquire child latch before releasing parent latch.
- Write path uses exclusive latches on nodes being modified.
- Read path uses shared latches.
- Deadlock avoidance: latch order root → leaf.

---

## Garbage Collection

- `removeDeadEntries(dead_record_uuids)` removes leaf entries for dead versions.
- Internal nodes are pruned when leaf entries are removed and underflow.

---

## API Requirements

```cpp
Status btree_insert(const Key& key, const SBIndexEntryMeta& meta, ErrorContext* ctx);
Status btree_search(const Key& key, const SBTransactionSnapshot* snap,
                    SBTransactionManager* tm, std::vector<UUID>* out, ErrorContext* ctx);
Status btree_remove(const Key& key, const UUID& record_uuid, ErrorContext* ctx);
```

---

## Recovery / Consistency

On open:
- validate page headers and sibling links
- rebuild root if corrupted (offline repair required if multiple errors)


## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
