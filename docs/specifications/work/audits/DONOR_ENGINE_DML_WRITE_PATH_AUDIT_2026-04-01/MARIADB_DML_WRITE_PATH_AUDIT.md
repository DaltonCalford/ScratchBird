# MariaDB DML Write-Path Audit

## Architectural Summary

MariaDB is useful as both a donor and a warning. It preserves much of the InnoDB-style clustered, undo, and purge discipline, but its current code also documents the removal of the old change buffer. That makes MariaDB a strong donor for upgrade safety, metadata rigor, and "remove a write-path optimization if it becomes too dangerous or too costly to maintain."

## Insert Optimizations

- MariaDB still exposes optimistic and pessimistic B-tree paths for exact index writes.
- `btr0btr.cc` also carries "instant" page and metadata handling, showing a willingness to encode structural format shortcuts directly into page-level rules when they can be validated.
- The historical change-buffer code remains as upgrade logic, which is useful evidence of how much complexity old secondary-write deferral accumulated.

## Update/Delete Optimizations

- `row0purge.cc` keeps the same core idea as InnoDB: delete logically first, then retire clustered and secondary records only when the purge view proves they are no longer needed.
- Purge explicitly reconstructs prior versions and checks whether secondary entries are still unsafe to remove.
- The engine remains careful about online or transitional index states during purge and cleanup.

## Index Maintenance Optimizations

- The most important MariaDB lesson is not a feature to copy directly. It is the startup-time upgrade path that merges old change-buffer state and prevents downgrade corruption after the feature was removed.
- Instant-format markers and metadata transitions show strong discipline around structural shortcuts.
- Online-DDL and metadata-state checks are treated as part of write safety, not only schema safety.

## Reliability And Publication Pattern

- MariaDB insists on upgrading or rejecting old buffered change state before normal operation continues.
- That is the correct pattern for any ScratchBird write-path optimization that changes on-disk or on-page meaning: validate, upgrade, then enable.

## Best Borrow Candidates For ScratchBird

- Defensive startup validation for any deferred-maintenance structures.
- Explicit format and metadata markers for fast-path structural modes.
- Aggressive willingness to remove or narrow an optimization if its ongoing correctness cost becomes too high.

## Local Source Anchors

- `storage/innobase/include/ibuf0ibuf.h`
- `storage/innobase/ibuf/ibuf0ibuf.cc`
- `storage/innobase/row/row0purge.cc`
- `storage/innobase/btr/btr0btr.cc`
- `sql/sql_update.cc`
