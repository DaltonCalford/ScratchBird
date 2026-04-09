# MongoDB DML Write-Path Audit

## Architectural Summary

MongoDB is the clearest donor for online index build and rebuild workflows. The key idea is not the document model itself. The key idea is that expensive index construction should happen as a sorted bulk build while concurrent writes continue, with intercepted write deltas drained and validated before final publish.

## Insert Optimizations

- MongoDB bulk-builds indexes by scanning collection data, generating keys, sorting them externally, and bulk-loading them into the storage engine in key order.
- Bulk-loading is explicitly called out as more efficient than random B-tree insertion because it builds a better-filled structure with less random churn.
- The `SortedDataInterface` exposes a dedicated bulk builder rather than pretending bulk and retail insert are the same operation.

## Update/Delete Optimizations

- During a hybrid index build, inserts, updates, and deletes continue hitting the collection as usual.
- Instead of mutating the incomplete index directly, key adds and key removals are intercepted into a temporary side-writes table.
- Updates are represented as both a key removal and a key insertion, which is exactly the right abstraction for deferred index catch-up.
- Duplicate-key and skipped-record tables push ambiguous or expensive final checks out of the long-running build phase and into the short final validation phase.

## Index Maintenance Optimizations

- MongoDB drains side writes in multiple stages: permissive drain, continued permissive drain while waiting for quorum, then final exclusive drain under an X lock.
- This staged drain lets large builds stay online without lying about correctness.
- Constraint rechecks happen at the end, when the remaining uncertainty set is smallest.

## Reliability And Publication Pattern

- Build state is durable in the catalog with `ready: false` until final publish flips it to `ready: true`.
- Replica-set builds use two-phase commit and commit quorum so publication is coordinated and lag is bounded.
- The important general lesson is the staged publish barrier, not the replica-set-specific oplog machinery.

## Best Borrow Candidates For ScratchBird

- Standardize shadow-build, side-log, drain, validate, publish for every expensive index family.
- Treat concurrent write interception as a first-class build subsystem.
- Keep temporary duplicate and skipped-record trackers rather than forcing all validation into the bulk phase.

## Local Source Anchors

- `src/mongo/db/index_builds/README.md`
- `src/mongo/db/index_builds/index_builds_coordinator.cpp`
- `src/mongo/db/storage/sorted_data_interface.h`
- `src/mongo/db/storage/wiredtiger/wiredtiger_record_store.cpp`
- `src/mongo/db/storage/wiredtiger/wiredtiger_index.cpp`
