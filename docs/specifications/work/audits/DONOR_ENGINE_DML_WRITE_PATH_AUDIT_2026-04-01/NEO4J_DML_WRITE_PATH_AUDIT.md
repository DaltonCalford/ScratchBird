# Neo4j DML Write-Path Audit

## Architectural Summary

Neo4j is a donor for index-update orchestration more than for base storage shape. Its most useful write-path ideas are explicit batch application of index updates and a specialized tree writer mode for consecutive inserts.

## Insert Optimizations

- `GBPTree` exposes a batched single-threaded writer mode that is explicitly more efficient for consecutive inserts.
- This is a simple but important donor lesson: a tree should expose a dedicated batched writer mode when locality allows it.

## Update/Delete Optimizations

- DML operations trigger explicit index update callbacks for add, remove, and change cases.
- The update path is clear about whether a property or label mutation means insert, delete, or replacement work for the index layer.

## Index Maintenance Optimizations

- `IndexUpdatesWorkSync` combines updates from multiple transactions into one larger apply job when `parallelApply` is disabled.
- When parallel apply is enabled, the engine passes that fact through explicitly so updaters can adapt.
- This reduces repetitive small-job overhead and can collapse concurrent update streams into fewer physical index-apply passes.

## Reliability And Publication Pattern

- The relevant publication barrier is the combined batch of index updates rather than each tiny logical mutation being applied in isolation.
- That is a good donor for ScratchBird exact and near-exact families where many concurrent transactions touch the same structures.

## Best Borrow Candidates For ScratchBird

- A dedicated batch index-apply layer that can combine concurrent transaction output.
- Batched-writer modes for exact trees when locality is favorable.
- Explicit per-update-mode handling instead of treating all updates as "delete plus insert but hidden."

## Local Source Anchors

- `community/storage-engine-util/src/main/java/org/neo4j/storageengine/util/IndexUpdatesWorkSync.java`
- `community/kernel/src/main/java/org/neo4j/kernel/impl/index/schema/NativeIndexUpdater.java`
- `community/index/src/main/java/org/neo4j/index/internal/gbptree/GBPTree.java`
- `community/kernel/src/main/java/org/neo4j/kernel/impl/newapi/Operations.java`
