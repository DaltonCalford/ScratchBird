# MySQL DML Write-Path Audit

## Architectural Summary

MySQL's strongest write-path donor is InnoDB. The important lesson is not the whole redo or mini-transaction architecture. The important lesson is how InnoDB separates clustered truth from secondary acceleration structures, buffers some secondary work when pages are cold, and uses purge to retire stale versions only when it is safe.

## Insert Optimizations

- The change buffer exists to reduce random disk access for non-unique secondary indexes whose target leaf pages are not in memory.
- Instead of forcing a random page read, InnoDB records the secondary change in a dedicated buffer tree keyed by target page and merges it later when the page is read or background merge runs.
- B-tree paths distinguish optimistic from pessimistic insertion, allowing cheap leaf-level changes when page conditions are favorable and escalating only when necessary.
- Clustered primary storage keeps the physical truth in one place and makes secondaries thinner than a fully duplicated row structure.

## Update/Delete Optimizations

- Updates and deletes are versioned through undo, then later retired by purge.
- `row0purge.cc` removes delete-marked clustered records and secondary entries only when the purge view and latch conditions make it safe.
- Purge eligibility checks explicitly prevent deletion of secondary entries that may still be referenced by a visible clustered version.
- Foreground delete is logical first; physical cleanup is deferred.

## Index Maintenance Optimizations

- Secondary indexes are treated as acceleration structures over clustered/versioned truth.
- The change buffer is narrowly scoped to eligible non-unique secondaries, which is the right kind of deferral. It is not a generic excuse to postpone all index work.
- Insert-buffer bitmap and free-space tracking exist specifically to make later merge succeed safely.
- The storage layer distinguishes optimistic and pessimistic index insert/delete paths to keep easy cases cheap.

## Reliability And Publication Pattern

- InnoDB ties deferred secondary work to explicit merge rules and crash-safe metadata.
- The change buffer comments are explicit that free-space bookkeeping must never outrun what crash recovery can safely prove.
- Purge is treated as a correctness-sensitive background stage, not just a compaction convenience.

## Best Borrow Candidates For ScratchBird

- A narrow cold-page secondary delta buffer for eligible exact secondaries.
- Strict merge eligibility and hard backlog limits for deferred index work.
- Horizon-based purge rules for removing stale exact-index entries.
- Optimistic-versus-escalated exact-tree operations.

## Local Source Anchors

- `storage/innobase/include/ibuf0ibuf.h`
- `storage/innobase/ibuf/ibuf0ibuf.cc`
- `storage/innobase/row/row0ins.cc`
- `storage/innobase/row/row0upd.cc`
- `storage/innobase/row/row0purge.cc`
- `storage/innobase/btr/btr0btr.cc`
- `sql/join_optimizer/interesting_orders.h`
