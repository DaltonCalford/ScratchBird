# FirebirdSQL DML Write-Path Audit

## Architectural Summary

Firebird is the strongest donor for keeping visibility truth, undo/backout, and exact index correctness under one owner. Its optimizer breadth is smaller than PostgreSQL's, but its DML path is unusually coherent: record versioning, savepoints, garbage collection, and index maintenance all stay close to the same VIO and transaction core.

## Insert Optimizations

- Inserts fit naturally into Firebird's MGA record-version model because the engine is already built around versioned records instead of destructive replacement.
- Exact index insertion is tightly coupled to record lifecycle code instead of being delegated to a loosely related subsystem.
- `idx.cpp` includes multi-worker index-creation tasks, which shows Firebird is willing to parallelize heavy build work even while keeping runtime semantics tight.

## Update/Delete Optimizations

- `vio.cpp` stores old versions, chases back versions, and prepares modify or erase operations with transaction and savepoint awareness.
- Backout is native. Update and delete logic is written assuming savepoint rollback and version chase are part of the normal lifecycle.
- Garbage collection is horizon-based and closely tied to version maturity rather than immediate physical destruction.
- Firebird supports background garbage collection and explicit garbage-collection calls for record and index cleanup.

## Index Maintenance Optimizations

- `idx.cpp` handles duplicate checking, foreign-key checks, and index insertion with transaction awareness.
- `btr.cpp` and related code keep B-tree invariants close to transactional semantics rather than treating the index as independent truth.
- The physical B-tree implementation uses compressed jump nodes and duplicate-aware ordering to keep navigation compact.
- Segment selectivity and compound-key behavior are treated as first-class optimization inputs.

## Reliability And Publication Pattern

- Firebird's publication truth is the transaction and record-version system itself. Exact index state is not allowed to outrun or contradict that truth.
- Cleanup is staged: visible lineage first, mature-version retirement later.

## Best Borrow Candidates For ScratchBird

- Keep exact index truth under the same owner as lineage truth.
- Make savepoint/backout a first-class part of DML, not an exception path.
- Keep index garbage collection visibility-aware and exact.
- Use parallel build only for bulk/rebuild workflows, not as a substitute for clean runtime semantics.

## Local Source Anchors

- `src/jrd/vio.cpp`
- `src/jrd/idx.cpp`
- `src/jrd/btr.cpp`
- `src/jrd/tra.cpp`
