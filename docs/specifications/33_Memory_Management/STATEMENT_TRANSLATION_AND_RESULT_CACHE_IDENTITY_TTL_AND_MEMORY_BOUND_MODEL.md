# Statement Translation and Result Cache Identity TTL and Memory Bound Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the current cache identity, TTL, eviction, and memory-bound
contracts for the process-local statement, translation, and result-adjacent
cache families that affect SQL compilation and reuse.

## Current code-backed authority

The current rebuild pass is grounded in:

- `scratchbird/pool/statement_cache.h`
- `src/pool/statement_cache.cpp`
- `scratchbird/protocol/translation_cache.h`
- `src/protocol/translation_cache.cpp`

## Cache-family separation rule

The current implementation does not expose one undifferentiated generic cache.

At minimum, current code proves separate families for:

- prepared or pooled statement cache
- protocol translation cache
- other result or plan caches owned elsewhere

These families must not be collapsed into one cache identity model in canon.

## Statement-cache identity model

Current statement cache identity is built from:

- normalized statement fingerprint
- parameter-type signature
- schema version id
- privilege signature

The statement fingerprinter can normalize:

- whitespace
- literals
- case

Current statement cache configuration includes:

- total statement count bound
- total memory-byte bound
- per-connection statement bound
- TTL defaults, minimum, and maximum
- per-statement-type cache enablement
- minimum execution count to cache
- minimum and maximum statement size
- invalidation on schema change
- optional invalidation on statistics change
- fingerprinting controls

## Statement classification and filtering

Current statement types include:

- `SELECT`
- `INSERT`
- `UPDATE`
- `DELETE`
- `DDL`
- `DML_OTHER`
- `DCL`
- `TCL`
- `UTILITY`
- `UNKNOWN`

The cache is not required to treat all statement types equally.
Current configuration already allows type-specific cache participation policy.

## Statement-cache state model

Current cache-entry states are:

- `VALID`
- `PREPARING`
- `INVALID`
- `EVICTING`

These states are part of the runtime cache contract and are not decorative enum
values.

## Statement-cache TTL and memory contract

Each cached statement currently tracks:

- creation time
- last access time
- last execution time
- expiry time
- hit count
- miss count
- execution count
- error count
- total, min, max, and average execution time
- memory footprint
- plan-memory footprint

TTL behavior is explicit:

- entries have a TTL
- remaining TTL can be inspected
- TTL can be refreshed
- expiration is checked against wall-clock time

Memory accounting is also explicit and includes:

- object overhead
- SQL text
- fingerprint
- referenced tables
- referenced schemas
- referenced functions
- parameter-type metadata
- privilege signature
- result-column metadata

## Statement eviction policy model

Current statement-cache configuration exposes multiple eviction policies:

- `LRU`
- `LFU`
- `ARC`
- `FIFO`

This means cache eviction policy is part of the public runtime behavior of the
cache subsystem even where some policies may currently share common internal
machinery.

## Translation-cache identity model

The current protocol translation cache is a dedicated SQL-to-SBLR bytecode cache
keyed by:

- dialect
- SQL text
- schema version
- privilege signature

This cache is not fingerprint-based in the same way as the pooled statement
cache. It stores dialect-local translation output directly.

## Translation-cache bounds and behavior

Current translation-cache configuration includes:

- maximum entry count
- maximum bytes
- TTL
- enabled flag

Current translation-cache behavior includes:

- LRU ordering
- expiry by steady-clock TTL
- entry-size estimation
- single-entry eviction
- invalidate-all support
- hit, miss, eviction, current-entry, and current-byte statistics

The cache uses exclusive access for `get(...)` because retrieval mutates access
metadata and LRU ordering.

## Translation-cache telemetry rule

Current translation-cache behavior is tied to runtime metrics emission for:

- hits
- misses
- evictions

Therefore translation-cache use is an observable operator surface, not merely a
private optimization.

## Invalidation and security rule

Both statement-cache and translation-cache identity already include:

- schema version
- privilege signature

Therefore cached reuse is already bound to schema and authorization identity.

It is non-conforming to reuse either cache family across schema or privilege
boundaries that change the key identity.

## Governing memory rule

These caches are process-local memory consumers with explicit memory ceilings.

They are not authoritative storage, not correctness truth, and not a substitute
for MGA-visible catalog or execution state.

They may improve reuse and latency.
They may not weaken schema, privilege, or publication correctness.
