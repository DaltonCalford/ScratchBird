# MGA Observability and Operator Diagnostics

Status: current_authority

## Purpose

Operator diagnostics must expose MGA-specific state directly. A WAL-shaped dashboard is non-conforming because ScratchBird truth comes from transaction inventory, page versions, and visibility horizons.

## Required MGA counters

The engine shall expose at minimum:

- `OIT`
- `OAT`
- `OST`
- next transaction identifier
- active transaction count
- prepared transaction count
- committed transaction count since startup
- rolled-back transaction count since startup
- update conflict count
- wait-for-write conflict count
- savepoint rollback count

## Required visibility diagnostics

The engine shall expose at minimum:

- average visible head depth per table or segment
- max visible chain depth
- invisible but retained version count
- prepared-version retained count
- rolled-back retained version count
- versions blocked from reclaim by active snapshots

## Required sweep diagnostics

The engine shall expose at minimum:

- sweep debt in pages and versions
- last completed sweep pass identifier
- sweep duration
- pages skipped due to verification failure
- pages skipped due to derivative-lane requirements
- pages or families quarantined from destructive cleanup

## Required lock diagnostics under MGA

Because ordinary conflicts are write-write only at the row-version level, the engine shall distinguish:

- write-write conflicts
- metadata lock waits
- structural page-latch waits
- deadlock or cycle detection events

Read visibility misses must not be reported as row-lock waits.

## Required index diagnostics

The engine shall expose at minimum:

- candidate hits examined per family
- candidate hits rejected by MGA visibility
- exact-recheck rejects where the family requires exact recheck
- dead entries pending cleanup
- family-local split and structural-repair counts
- planner-family metrics packet state for every planner-visible index
- metrics freshness and confidence for optimizer-visible index statistics

## Required typed planner-metrics diagnostics

For every planner-visible index, observability shall expose the current typed metrics envelope and its family payload. At minimum this includes:

- planner family class
- queryability state
- last refresh XID
- confidence class
- dead fraction
- bloat ratio
- recheck ratio estimate
- maintenance backlog
- publish lag
- reclaim lag
- family payload version

If the planner is consuming a synthesized or estimated payload, observability must expose that fact explicitly.

## Required derivative diagnostics

The engine shall expose health and lag for:

- write-after log
- shadow capture
- temporal archive export

These diagnostics must be labeled as derivative-lane health, not durability truth.

## Required derivative queue-state diagnostics

For every active derivative profile, the engine shall expose at minimum:

- queue depth
- oldest pending age
- retryable failure count
- quarantined failure count
- last delivery success time
- last delivery failure time
- active backpressure class
- last quarantined identity or sequence marker when quarantine is non-empty

## Required shadow-group diagnostics

For every active shadow group or shadow fleet, the engine shall expose at minimum:

- group identity
- member tablespace ids
- member shadow ids
- group state
- last verified time
- member-sync freshness summary
- failover readiness classification

The operator surface must distinguish:

- individual shadow degradation
- full-group degradation
- promotion-ready state
- already-promoted live-route state

## Required restore and continuity diagnostics

The engine shall expose at minimum:

- last restore or promotion boundary identifier
- last failback boundary identifier when present
- continuity-marker counts for pre-boundary and post-boundary derivative lanes
- current live route identity for the primary and each durable tablespace

These diagnostics are inspection aids only and must not be labeled as recovery authority.

## Required operator distinction

The observability surface must make the following distinction explicit:

- local MGA durability health
- derivative shipping health

It is non-conforming to present derivative backlog or quarantine as if committed local truth were uncertain when local durability remains healthy.

## Conformance rule

If a family is planner-visible, the operator diagnostics surface must expose enough metrics to explain planner behavior. A hidden or heuristic-only planner metric without an observable operator surface is non-conforming.
