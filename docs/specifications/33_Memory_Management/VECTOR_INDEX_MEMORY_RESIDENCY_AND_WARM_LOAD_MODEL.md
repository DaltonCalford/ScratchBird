Status: reconstructed_required_with_current_substrate

# Vector Index Memory Residency and Warm-Load Model

## Purpose

This file defines the memory contract for vector and ANN index families whose
usefulness depends on a resident working set. It specifies what must stay in
memory, when it is loaded, how it is refreshed, and how it stays subordinate to
MGA truth.

## Current code-backed baseline

The current code proves:

1. vector search families execute in-process
2. HNSW supports:
   - create
   - open
   - insert
   - search
   - dead-entry removal
3. HNSW distance metrics currently include:
   - Euclidean
   - cosine
   - Manhattan
   - dot product
4. Search results are not user-visible merely because the vector index returns a candidate list; heap-visible tuple acceptance still governs the final result set
5. Vector indexes are part of the ordinary engine and page/database lifecycle, not a detached external service

## Required reconstructed residency rule

Vector and ANN families shall be resident-on-first-use.

For the purpose of this rule, the family set is:

- HNSW
- IVF
- vector flat when used as an ANN-ready search structure
- GPU-assisted vector families

### Resident-on-first-use means

On the first admitted use of a vector family index in a process:

1. load the family metadata from durable state
2. load or reconstruct the searchable in-memory structure
3. assign a resident generation
4. keep the searchable structure resident in host memory until one of the allowed eviction conditions occurs

## Canonical host-memory image

Each resident vector index shall maintain a canonical host-memory image
containing, as applicable:

- index identity
- family identity
- structural generation
- last durable generation observed
- graph or routing topology
- centroids or codebooks
- posting or candidate sets
- delete/dead-entry side structures
- resident metrics snapshot

This host-memory image is the canonical execution image for fast search.
Accelerator mirrors, if any, are derivative from it.

## Warm-load algorithm

The required warm-load algorithm is:

1. resolve index UUID and family identity
2. verify index metadata compatibility
3. verify the durable structural generation
4. allocate resident memory from the bounded vector/accelerator working-set budget
5. load family-local structural state
6. publish resident state only after the structure is internally complete
7. record a warm-load metric event

Partial resident images shall not be published for query use.

## Mutation algorithm

For inserts, updates, and deletions affecting a resident vector index:

1. apply the change under the family-local structural coordination discipline
2. update the canonical host-memory image
3. record dirty structural generation or dirty range state
4. flush durable family-local page changes according to the family publication order
5. update or invalidate any accelerator mirrors
6. retain the resident image in memory after the flush

The family shall not discard the resident structure after every write merely
because durable pages were updated.

## Eviction rules

Eviction is allowed only when:

1. the database closes
2. the process shuts down
3. a bounded memory-pressure policy explicitly evicts the resident image
4. an administrator explicitly unloads the family
5. a structural corruption or generation mismatch requires quarantine

Eviction shall publish:

- resident bytes released
- reload-required marker
- last clean durable generation
- reason for eviction

## MGA boundary

Vector index residency does not change MGA truth.

The acceptance order is:

1. vector family returns candidate rows
2. heap tuple lineage is checked
3. visibility is resolved under MGA transaction rules
4. only visible tuples become results

Resident vector structures accelerate candidate discovery. They do not override
heap visibility.

## Dirty-state and restart rule

Resident images are process-local and restart-local.

After process restart:

1. no prior resident image is trusted
2. the engine must warm-load from durable state again
3. optional derivative snapshots may speed load, but they are not authoritative truth

## Required metrics

Each resident vector family shall expose:

- resident bytes
- warm-load count
- warm-load latency
- reload count
- eviction count by reason
- dirty generation count
- candidate count
- visibility reject count
- dead-entry count
- dead-entry removal count

## Improvement candidates captured by this rebuild

The following are strong improvement candidates and should remain visible in the
rebuild trail:

1. prewarm policy for hot vector indexes
2. segmented resident budgets by family and database
3. shared resident image reuse across sessions in the same process
4. resident snapshot compaction for faster reopen
