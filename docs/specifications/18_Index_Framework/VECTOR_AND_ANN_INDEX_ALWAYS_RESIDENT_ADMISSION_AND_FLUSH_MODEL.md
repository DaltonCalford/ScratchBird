Status: reconstructed_required

# Vector and ANN Index Always Resident Admission and Flush Model

## Purpose

This document defines the canonical rule that admitted vector and ANN index families are always resident in memory while active, with durable flush and restart reconstruction controlled separately.

## Canonical Rule

Vector and ANN index families that are classified as resident shall:

- load from durable storage on first admitted use or explicit prewarm
- remain memory resident while admitted
- serve queries from the resident structure
- flush durable changes according to ordered durability rules

Ordinary best-effort cache eviction is non-conforming for admitted resident vector or ANN families.

## Admission Requirements

Admission requires:

- valid durable source image or canonical rebuild source
- memory budget approval
- compatibility and format validation
- acceleration or GPU state only if the family variant requires it

## Persistent Residency Rule

Once admitted, the index remains resident until one of:

- explicit administrative unload
- process shutdown
- catastrophic pressure policy with explicit degraded-state publication
- corruption or compatibility refusal
- rebuild or rewarm after restart

## Dirty-State Rule

If the resident structure changes, dirty state shall be tracked explicitly. Dirty resident state does not become durable truth until ordered flush completes.

## Flush Rule

Flush shall:

1. preserve the resident index as serving truth for admitted in-memory use
2. generate the durable update image or delta
3. write durable state
4. verify integrity markers
5. advance the durable flush marker
6. clear or advance dirty resident state

## Query Rule

Queries may use the resident index only as an access path. Final tuple or row acceptance remains subordinate to MGA visibility and heap or version truth.

## Non-Guarantees

This file does not require every vector-related family to share one exact data structure. It requires always-resident admission and ordered flush discipline for admitted resident families.
