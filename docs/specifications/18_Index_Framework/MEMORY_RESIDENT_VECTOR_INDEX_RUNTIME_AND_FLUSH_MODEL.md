# MEMORY_RESIDENT_VECTOR_INDEX_RUNTIME_AND_FLUSH_MODEL

## Status

Required reconstructed specification with current code-backed boundary.

## Purpose

This document defines the required runtime model for vector and ANN families that must remain memory-resident after first use while preserving MGA durability and anti-WAL truth rules.

## Current code-backed boundary

The current code-backed HNSW runtime proves the following:

1. HNSW is page-backed and durable in database pages
2. `HnswIndex::create()` initializes a durable root page through the page manager and buffer pool
3. `HnswIndex::open()` reopens the index from durable page state
4. insert, search, remove, and GC operate against the persisted page model
5. MGA visibility is enforced with `xmin`/`xmax` and TIP-based visibility rules
6. dead entry removal persists across flush and restart

What current code does not yet prove is a global always-resident search image that is loaded once and then kept warm for the lifetime of the index runtime.

## Canonical required rule

Selected vector families shall be memory-resident after first use.

At minimum this rule applies to:

1. `HNSW`
2. `IVF`-class vector families
3. `VECTOR_FLAT`-class exact vector families when catalog policy marks them resident-required
4. any future ANN family whose catalog/runtime policy marks `resident_required = true`

## Truth model

The durable database state remains authoritative.

The memory-resident search image is derivative runtime state.

That means:

1. durable pages and committed catalog metadata remain recovery truth
2. the resident image is rebuilt from durable truth
3. the resident image is discarded on restart and reconstructed from durable truth
4. no WAL-style replay authority is introduced

## First-use load algorithm

On first use of a resident-required vector family:

1. resolve the durable catalog metadata for the index
2. load the durable page-backed structure from the database
3. construct the resident search image in process memory
4. publish the resident image into the runtime cache only after structural validation succeeds
5. fail closed if the durable image cannot be validated

## Steady-state runtime model

Once loaded:

1. searches execute against the resident search image
2. inserts, deletes, and updates mutate the resident image and the durable page-backed image under the same transaction rules
3. uncommitted changes must not become globally visible through the resident image
4. MGA visibility remains authoritative over candidate acceptance

## Commit and rollback rules

Because ScratchBird is always in a transaction:

1. commit publishes the transaction outcome and keeps the resident image aligned with committed state
2. rollback discards the uncommitted resident delta and immediately returns the runtime to the next transaction context
3. DDL and DML changes that affect resident vector indexes follow the same transaction rules

## Flush and checkpoint rules

Resident vector families shall use ordered publication:

1. resident mutation is not itself durable
2. dirty durable pages and any family-local flush queues must be forced to durable storage according to the MGA forced-write discipline
3. checkpoint and maintenance processes may compact or rewrite the durable representation, but the resident image must remain derivable from committed durable state

## GC and reclaim rules

Resident vector families remain subordinate to MGA reclaim rules:

1. soft-deleted or dead vector entries remain visible or invisible according to MGA rules until reclaim eligibility is proven
2. resident cleanup may hide dead candidates from future searches when visibility rules permit
3. destructive reclaim occurs only after GC proves the version is reclaimable

## Required runtime observability

Resident-required vector families shall expose at least:

1. resident loaded state
2. resident bytes
3. load timestamp or generation
4. dirty resident delta count
5. flush backlog
6. resident rebuild count
7. resident invalidation count

## Current implementation gap

Current HNSW code proves durable page-backed operation and MGA-aware search/GC, but it does not yet prove the full always-resident runtime image defined here.

This document is therefore a reconstructed required specification:

1. the current code-backed baseline is preserved
2. the intended resident-vector runtime is made explicit for future implementation and audit
