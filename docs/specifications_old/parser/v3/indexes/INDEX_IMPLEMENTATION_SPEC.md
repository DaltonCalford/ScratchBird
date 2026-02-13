# ScratchBird Index Implementation Specification

**Storage Layout Authority:** On-disk page headers, slot arrays, free-space rules, and page-type layouts are authoritative in `../storage/PAGE_TYPES_AND_LAYOUTS.md`. Any structs here are logical field groupings; do not infer byte-accurate layout from this file.



**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Firebird Source Alignment (Normative)
- `firebird/src/jrd/idx.cpp` (index maintenance under MGA)
- `firebird/src/jrd/vio.cpp` (visibility checks during index access)

---

## Global Requirements

All index implementations must:

1. Use **record UUIDs** as stable identity.
2. Validate visibility using **TIP + record headers**.
3. Perform **logical deletion** via record versions (`RHD_DELETED`).
4. Remove entries only during **index GC** after OIT advances.

---

## Standard Index Metadata


**Logical Fields:**

- `record_uuid` (UUID)
- `record_ptr` (SBRecordPtr): cache hint
- `record_txn` (uint64_t)
- `record_flags` (uint32_t)
- `back_version_uuid` (UUID): optional


---

## Standard Index Page Header


**Logical Fields:**

- `header` (PageHeader)
- `index_uuid` (UUID)
- `page_type` (uint16_t)
- `level` (uint16_t)
- `page_epoch` (uint64_t)


---

## Visibility Rules (Required)

- Resolve record header by UUID.
- Use `sb_find_visible_version` to determine visibility.
- Skip versions with `RHD_DELETED`.

---

## GC Rules (Required)

An entry is removable when:

1. Record version is COMMITTED.
2. `record_txn < OIT`.
3. Record version is deleted or superseded.

---

## API Requirements

Each index must implement:

```cpp
Status insert(const Key& key, const SBIndexEntryMeta& meta, ErrorContext* ctx);
Status search(const Key& key,
              const SBTransactionSnapshot* snap,
              SBTransactionManager* tm,
              std::vector<UUID>* results,
              ErrorContext* ctx);
Status remove(const Key& key, const UUID& record_uuid, ErrorContext* ctx);
Status removeDeadEntries(const std::vector<UUID>& dead_record_uuids,
                         uint64_t* entries_removed_out,
                         uint64_t* pages_modified_out,
                         ErrorContext* ctx);
```

---

## Concurrency Requirements

- Readers must be non-blocking.
- Writers must publish new versions atomically.
- GC must yield to foreground work.

---

## Testing Requirements

- MGA visibility
- OIT-based GC
- Concurrency (readers + writers + GC)

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
