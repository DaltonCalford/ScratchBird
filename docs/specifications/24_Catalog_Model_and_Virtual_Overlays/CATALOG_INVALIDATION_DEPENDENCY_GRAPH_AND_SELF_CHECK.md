# Catalog Invalidation, Dependency Graph, and Self-Check

## Purpose

This file defines how committed catalog change is propagated to dependent consumers. ScratchBird does not rely on a magical globally coherent metadata bus. Invalidation is produced explicitly by durable catalog operations and consumed explicitly by caches, planners, parser-assist layers, and runtime bind paths.

## Canonical producers

The following operations are invalidation producers:

- `setObjectDefinition(...)`
- `replaceDependencies(...)`
- `clearDependenciesFor(...)`
- committed schema-epoch append via `appendSchemaEpochCatalogEntry(...)`
- publication catalog CRUD
- permission/grant/revoke catalog mutation
- object drop, rename, move, or ownership/ACL mutation

A transactional producer is not globally visible until commit publication succeeds.

## Canonical consumers

The following are invalidation consumers:

- parser-assist catalog caches
- planner and execution bind artifacts
- query result cache
- permission cache
- metadata and statistics caches
- publication/subscription readers

Consumers are derived views only. They are never authoritative over the durable catalog.

## Commit-bound invalidation procedure

For a transaction that performs catalog mutation, the engine must apply this procedure:

1. record transactional catalog/object-definition/dependency mutation
2. stage invalidation intent keyed by affected object UUID, dependency family, and prospective schema epoch
3. keep those intents transaction-local until terminal outcome is known
4. on commit:
   - append the new schema epoch if `DDL` batches exist
   - make the new committed epoch the publication anchor
   - retire local caches directly affected by the mutation
   - expose committed delta information to parser-assist consumers
5. on rollback:
   - discard all pending invalidation intents
   - do not expose a new schema epoch or committed delta

## Dependency graph rules

`replaceDependencies(...)` is the authoritative dependency refresh primitive. `clearDependenciesFor(...)` is the authoritative dependent-side teardown primitive.

Required invariants:

- every `DDL`-managed durable object with dependency-bearing semantics must have dependency rows refreshed or removed as part of the same transaction
- a committed object-definition change without corresponding dependency refresh is invalid
- a dropped object must not retain committed dependent-edge rows that claim the old object still exists
- dependency consumers must treat committed schema epoch change as the boundary after which rebinding is mandatory if cached dependency state is stale

## Consumer revalidation rules

A consumer may cache derived metadata, but it must re-check before bind or use:

- current committed schema epoch
- object-definition signature or equivalent definition identity
- permission/security epoch if authorization-sensitive
- dependency-invalidating mutation markers when available

If the cached state does not match the current committed anchor, the consumer must discard and rebuild. It must not continue with a stale artifact merely because invalidation delivery lagged.

## Parser-assist delta rules

Parser bulk-delta publication is commit-bound. The parser-assist layer must receive:

- committed adds/updates/deletes only
- one monotonic anchor advance per committed schema epoch transition
- `reset_required = true` when an anchor cannot be advanced incrementally

A failed statement or rolled-back transaction must not produce a committed parser delta.

## Self-check requirements

The metadata subsystem must be able to verify at least these invariants:

- dependency rows do not reference missing dependents or missing referenced objects
- canonical object-definition rows exist for durable `DDL`-managed objects that require them
- schema-epoch manifests reference only committed transactional batches
- parser delta anchors are monotonic and never skip silently from a stale anchor without either valid delta rows or `reset_required`
- publication catalog rows and metadata caches do not claim a newer epoch than the durable schema-epoch table

## Refusal rules

The engine must fail closed if it cannot prove:

- which objects were invalidated by a committed mutation
- whether a consumer artifact is bound to the current committed schema epoch
- whether parser delta output is complete for the requested anchor range
