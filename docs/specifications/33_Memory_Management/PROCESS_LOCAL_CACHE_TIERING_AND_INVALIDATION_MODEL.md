Status: current_authority_with_reconstructed_expansion

# Process-Local Cache Tiering and Invalidation Model

## Purpose

This file defines the process-local cache tiers used by ScratchBird and the
rules that invalidate, refresh, or evict them.

## Governing rule

All runtime caches are derivative.

No cache is authoritative truth over:

1. committed MGA-visible database state
2. committed catalog and policy epochs
3. canonical executable SBLR

## Current code-backed cache tiers

The current codebase already contains distinct cache tiers for:

1. buffer and page residency
2. LSM block caching
3. statement caching
4. translation caching
5. permission caching
6. query result caching
7. generic result caching
8. connection-pool local reuse state

These caches are not interchangeable and shall not be documented as one generic
"cache layer."

## Cache ownership boundaries

### Buffer and page cache tier

This tier owns resident durable page images and page-local access locality.

It is invalidated or refreshed by:

- page eviction
- checkpoint and recovery events
- structural generation changes
- corruption quarantine

### Translation and statement cache tier

This tier owns parser or compilation-adjacent reusable artifacts.

It is invalidated by:

- schema epoch changes
- dependency signature changes
- policy changes affecting executable semantics
- parser-local configuration changes

### Permission cache tier

This tier owns authorization decisions and permission closures.

It is invalidated by:

- security policy epoch changes
- quorum or cluster security status changes
- role, group, or shared-rights changes
- masking or sandbox policy changes

### Query result and generic result cache tier

This tier owns reusable execution results.

It is invalidated by:

- transaction visibility changes
- relevant table or index maintenance generation changes
- schema changes
- policy changes affecting visible rows or columns
- explicit memory-pressure eviction

## Snapshot and visibility rule

Result caches are subordinate to transaction visibility.

A result cache entry shall not be reused when the caller's visibility context,
schema visibility context, or security context is not compatible with the entry.

## Invalidation sources

The engine shall treat the following as first-class invalidation sources:

1. schema epoch
2. dependency signature
3. security policy epoch
4. transaction visibility boundary
5. index or storage structural generation
6. corruption quarantine or recovery state
7. memory pressure

## Reconstructed required expansion

The rebuild requires a unified cache invalidation vocabulary so all cache tiers
publish deterministic reason codes for:

- schema invalidation
- dependency invalidation
- security invalidation
- visibility invalidation
- structural generation invalidation
- pressure eviction
- operator flush

## Fail-closed rules

The following are non-conforming:

1. reusing a cached result across incompatible transaction visibility
2. reusing permission state across security epoch changes
3. reusing translation or statement artifacts after dependency invalidation
4. treating a buffer-resident page as newer than the durable publication order allows
