# ScratchBird Index Implementation Guide


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)

**Version:** 2.0
**Date:** 2026-02-07
**Audience:** ScratchBird Developers

---

## Purpose

This guide defines the required steps and **MGA-compliant** patterns for implementing any index in ScratchBird. All index implementations must be record-version based (UUID + TIP visibility) and must not rely on PostgreSQL-style snapshot semantics.

---

## Required Index Entry Metadata

All index entries must include record-version metadata (either embedded or referenced):


**Logical Fields:**

- `record_uuid` (UUID): Stable record identity
- `record_ptr` (SBRecordPtr): Physical locator (cache hint)
- `record_txn` (uint64_t): rhd_transaction
- `record_flags` (uint32_t): RHD_DELETED, etc.
- `back_version_uuid` (UUID): optional


---

## MGA Visibility Pattern (Required)

Index entries **must** validate visibility by consulting record headers and TIP:

```cpp
bool index_entry_visible(const SBIndexEntryMeta& meta,
                         const SBTransactionSnapshot* snap,
                         SBTransactionManager* tm)
{
    const SBRecordHeader* rhd = resolve_record_header(meta.record_uuid, meta.record_ptr);
    if (!rhd) return false;

    if (!record_uuid_matches(rhd, meta.record_uuid)) {
        rhd = resolve_record_header_by_uuid(meta.record_uuid);
        if (!rhd) return false;
    }

    const SBRecordHeader* visible = sb_find_visible_version(rhd, snap, tm);
    return visible && (visible->rhd_flags & RHD_DELETED) == 0;
}
```

---

## DML Integration Rules

- **INSERT:** create record version, then add index entry for that version.
- **UPDATE (non-key):** no index change.
- **UPDATE (key change):** add new entry; old entry remains.
- **DELETE:** create deleted version (`RHD_DELETED`); index entry remains until sweep.

---

## GC Integration Rules

Indexes must implement:

```cpp
Status removeDeadEntries(const std::vector<UUID>& dead_record_uuids,
                         uint64_t* entries_removed_out,
                         uint64_t* pages_modified_out,
                         ErrorContext* ctx);
```

- Dead entries are removed only when record versions are dead by MGA rules.
- Index GC must be idempotent and safe under concurrency.

---

## Implementation Checklist

1. Define on-disk structures and meta pages.
2. Implement create/open APIs.
3. Implement insert/search/remove for record UUIDs.
4. Implement MGA visibility filter using TIP.
5. Implement index GC removal for dead record UUIDs.
6. Add DML integration hooks.
7. Add planner integration (cost model + hints).
8. Add tests (visibility, GC, concurrency).

---

## Example Entry Structure


**Logical Fields:**

- `key` (Key)
- `meta` (SBIndexEntryMeta)


---

## Common Pitfalls

- Using TIDs as stable identity (forbidden).
- Using xmin/xmax snapshot logic (forbidden).
- Physically removing entries on delete (forbidden; use sweep/GC).

---

## Testing Requirements

- MGA visibility correctness under concurrent insert/update/delete.
- Idempotent GC behavior.
- Stress tests with OIT advancement.

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
