Status: reconstructed_required_with_current_substrate

# JIT Artifact Store Layout Retention and Eviction Model

## Purpose

This document defines the canonical storage, retention, and eviction behavior for the JIT artifact store.

## Canonical Rule

The JIT artifact store is an engine-owned cache of compiled artifacts. It is never authoritative execution truth and may evict artifacts for compatibility or pressure reasons, but it shall preserve enough identity and reason tracking to explain reuse or loss.

## Artifact Store Fields

Each stored artifact shall preserve:

- artifact identity
- compatibility envelope
- creation time
- last use time
- reuse count
- eviction eligibility state
- invalidation reason if invalidated

## Layout Rule

The store shall be logically organized by:

- canonical statement or artifact identity
- compatibility envelope
- runtime target class
- policy state

The logical layout may be in-memory, on-disk, or mixed, but the identity rules remain the same.

## Retention Rule

Artifacts may be retained while:

- compatibility is intact
- memory or storage pressure remains below eviction thresholds
- policy continues to permit JIT use

## Eviction Rule

Eviction may be triggered by:

- memory pressure
- compatibility invalidation
- explicit policy disable
- cold-artifact aging
- runtime target change

Eviction shall never silently reclassify an incompatible artifact as reusable.

## Diagnostics Rule

The runtime shall expose:

- artifact-store hit count
- miss count
- invalidation count
- eviction count
- pressure-triggered eviction count
- explicit-policy eviction count

## Non-Guarantees

This file does not require persistence of artifacts across all process lifetimes. It requires deterministic retention and eviction behavior within the admitted store model.
