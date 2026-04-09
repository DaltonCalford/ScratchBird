Status: reconstructed_required

# Resident Index Warm Load Checkpoint and Flush Order Model

## Purpose

This document defines how resident-memory index families load from durable state, remain resident, and flush changes back to durable storage without violating MGA or durability ordering.

## Canonical Rule

For index families classified as resident, the in-memory structure is the active serving representation after admission, but durable storage remains the recovery and restart authority. Resident state shall therefore warm-load from durable state and flush back in an ordered way.

## Warm-Load Sequence

The canonical warm-load sequence is:

1. verify the durable index identity and format version
2. verify the last durable checkpoint or flush marker
3. allocate the resident index context
4. load the durable index image or canonical rebuild source
5. validate the loaded structure
6. publish the resident state as admitted for use

## First-Use Rule

A resident index may load on first use or through explicit prewarm. Once admitted, ordinary query execution shall use the resident state rather than repeatedly reconstructing from disk.

## Dirty-State Rule

Changes to a resident index create dirty resident state. Dirty resident state shall not be treated as durably published until the family’s ordered flush path completes.

## Flush Ordering

The canonical flush order is:

1. complete the owning transactional visibility decision
2. generate the durable index delta or refreshed durable image
3. write the durable target pages or segments
4. verify write completion and integrity markers
5. publish the new durable flush marker
6. clear or advance the resident dirty marker

## Checkpoint Interaction

Checkpoint may coordinate resident-index flush, but checkpoint is not allowed to publish a clean durable state for a resident index until the flush order above completes successfully.

## Failure Rule

If process failure occurs before the durable flush marker advances, restart shall trust durable state and reconstruct resident state from that durable authority. Stale resident in-memory state is never recovery truth.

## Pressure Rule

Resident indexes are protected working sets, but they may enter degraded service or explicit unload when policy permits. Any unload or degradation shall preserve whether dirty resident state needed flush before retirement.

## Diagnostics Requirements

The runtime shall expose:

- warm-load count
- warm-load failure count
- dirty resident bytes or units
- flush count
- flush failure count
- last durable flush marker
- resident admission and degraded state

## Non-Guarantees

This file does not require every index family to be resident. It defines the required lifecycle for those that are.
