# Plan 01 - Task D Index GC Checklist (One Index at a Time)

Use this checklist per index type. Do not proceed to the next index until this list is fully complete and reviewed.

## 0) Preconditions
- [ ] Read `docs/archive/2026-01-04/planning/plan_01_index_gc_clarifications.md` for this index type.
- [ ] Identify the exact class + file for `removeDeadEntries()`.
- [ ] Confirm whether the GC logic already exists (if yes, do NOT re-implement; only add tests).

## 1) Implementation
- [ ] Implement/finish `removeDeadEntries(const std::vector<TID>&, ...)`.
- [ ] Use the correct TID format for this index (legacy uint64_t vs TID struct).
- [ ] Traverse the correct on-disk structure (leaf chain, bucket chain, tree, posting list/tree, etc.).
- [ ] Mark pages dirty only when modified.
- [ ] Update `entries_removed_out` and `pages_modified_out`.
- [ ] Use the index’s existing lock (do not invent a new one).

## 2) Error Handling
- [ ] Empty `dead_tids` is a no-op returning `Status::OK`.
- [ ] On pin/read error, return `Status::IO_ERROR` and log a warning.
- [ ] If structure corruption is detected, return `Status::INDEX_CORRUPTED`.

## 3) Visibility Rules (MGA)
- [ ] Do NOT re-check heap visibility; `dead_tids` are authoritative.
- [ ] If the index stores `xmin/xmax`, only physically remove when `xmax < OIT`.
- [ ] Use `TransactionManager::getOldestXid()` for OIT where needed.

## 4) Tests (Required)
- [ ] Unit test in `/tests/` for this index type:
  - Create table + index.
  - Insert rows (ensure index entries exist).
  - Delete rows (dead TIDs).
  - Run GC.
  - Assert index no longer returns deleted rows.
- [ ] Restart test: same dataset survives restart and GC still works.
- [ ] Negative test: empty `dead_tids` does nothing and returns OK.

## 5) Evidence for Review
- [ ] Provide file/line references for the implementation.
- [ ] Provide test names and locations.
- [ ] Provide summary of counters (entries_removed, pages_modified) from test logs.

