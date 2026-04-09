# BUFFER_POOL_SEGMENTED_OWNERSHIP_EVICTION_AND_DIRTY_PUBLICATION_MODEL

## Status

Current code-backed authority.

## Purpose

This document defines the current ScratchBird buffer-pool memory model so another agent does not collapse it into a generic shared cache with vague LRU behavior.

## Governing boundary

The buffer pool is a bounded in-memory page residency and dirty-publication subsystem.

It is not:

1. recovery truth
2. WAL authority
3. visibility truth

MGA, TIP, durable page images, and forced-write publication remain authoritative.

## Structural model

The current buffer pool is:

1. fixed-size
2. backed by a shared frame array
3. logically segmented into policy domains and ownership partitions
4. observable through frame-state, queue-state, and domain-accounting structures

## Core configuration surfaces

The current configuration model includes at least:

1. `pool_size`
2. `page_size`
3. buffer profile
4. pool layout
5. per-domain budget configuration
6. replacement protected percentage
7. ghost-history percentage
8. prefetch windows and debt caps
9. background-writer enablement and delay
10. dirty-ratio thresholds

## Current layout and policy model

The important current distinction is:

1. the physical frame array is shared
2. the policy model is segmented
3. home ownership and current ownership are tracked per frame

The implementation already exposes:

1. `PolicyDomain`
2. `WorkloadClass`
3. `MgaPageClass`
4. `WritebackQueueState`
5. `ResidencyTier`
6. `LifecycleState`
7. `DirtyState`
8. `ThrashDetectorState`

## Ownership partition model

Each frame has:

1. a home partition
2. a current owner partition
3. an identity
4. lifecycle scaffolding

Each ownership partition tracks:

1. owned frames
2. free frames
3. victim cursor

The current code supports:

1. claiming a local free frame
2. reclaiming a free frame that belongs home to the requesting partition
3. stealing a truly free frame from another partition
4. transferring ownership between partitions when required

## Miss-path algorithm

On a segmented miss, the current algorithm is structurally:

1. resolve the effective owner partition
2. recheck the partition-local mapping to avoid duplicate loads
3. optionally try the ring frame path for ring-eligible strategies
4. if ring claim fails, claim or reclaim a free frame from segmented ownership
5. purge stale page-table mappings for the chosen frame
6. read the page from disk into the frame
7. assign frame identity and initial pin state
8. apply automatic MGA classification and access tracking
9. publish the frame into the partition-local page table

## Ring and strategy model

The current buffer pool already distinguishes access strategies, including:

1. normal
2. sequential
3. vacuum
4. bulk write

Ring paths exist to constrain pressure from scan-style or bulk paths so they do not overrun hotter residency tiers.

## MGA classification model

Frames carry MGA-oriented hints and snapshots, including:

1. oldest interesting transaction
2. prune-safe horizon hint
3. dead-version bytes
4. chain-depth hint
5. GC-touch generation
6. speculative-prefetch markers
7. commit-fence membership

This means the buffer pool is already aware of MGA-specific page posture, not only generic recency.

## Residency and lifecycle model

The runtime distinguishes:

1. `LegacyShared`
2. `RingOnly`
3. `Probationary`
4. `Protected`
5. `PinBiased`

Lifecycle is explicit:

1. free
2. loading
3. valid
4. flushing
5. evicting
6. error

## Dirty publication model

Dirty-state is staged explicitly:

1. `Clean`
2. `DirtyUnscheduled`
3. `DirtyQueued`
4. `DirtyInFlight`
5. `DirtyFlushedPendingFsync`
6. `DirtyFailed`

`DirtyFlushedPendingFsync` is particularly important:

1. the page image may already be written
2. the engine-wide forced-write fence is not yet complete
3. this is publication staging, not WAL semantics

## Writeback queues

Current writeback reasons are observable and classified as:

1. foreground help
2. background age
3. checkpoint
4. metadata priority
5. write combine
6. repair retry

These queues explain scheduling posture only. They do not redefine durability truth.

## Commit-fence behavior

Current MGA-policy tests prove that commit-fence members such as transaction-state pages are protected under pressure and move into a `PinBiased` posture rather than being treated as ordinary evictable scan traffic.

## Pressure and exhaustion behavior

Current tests prove the following pressure model:

1. the pool is bounded
2. clock-sweep-style eviction remains active under pressure
3. pinned frames block eviction
4. the system is expected to degrade gracefully under pressure rather than crash
5. the pool is expected to recover to a useful steady state after pressure subsides

## Shutdown behavior

Current shutdown behavior is ordered through:

1. stopping background writer activity
2. flushing dirty pages
3. clearing ownership and ghost structures
4. clearing page-table mappings

## Required implementer interpretation

Another agent shall preserve these current truths:

1. segmented policy over a shared frame array
2. ownership-aware miss handling
3. MGA-aware page classification
4. staged dirty publication with forced-write boundary
5. graceful bounded-pressure behavior
6. no reinterpretation of the buffer pool as recovery truth
