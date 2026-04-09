Status: reconstructed_required

# Index Family Metrics Publication Freshness and Invalidation Model

## Purpose

This document defines how ScratchBird publishes per-family index metrics, marks their freshness, and invalidates them when underlying state changes.

## Canonical Rule

Every implemented index family shall publish metrics through a canonical family-metrics publication path with explicit freshness and invalidation state. Metrics publication is not optional background decoration; it is part of optimizer correctness for primary-class family parity.

## Publication Unit

The canonical publication unit shall preserve:

- index identity
- runtime family
- alias surface if any
- metrics payload identity
- publication epoch
- freshness class
- confidence class
- invalidation state

## Freshness Classes

The canonical freshness classes are:

- `CURRENT`
- `AGED`
- `STALE_DEGRADED`
- `UNUSABLE`

## Invalidation Triggers

Metrics invalidation shall be triggered by:

- index structure change
- significant data-volume or distribution change
- maintenance or rebuild event
- resident-index warm rebuild
- metadata or catalog epoch change affecting semantics
- explicit administrative refresh or invalidate request

## Publication Rule

Publication may be eager or deferred, but the system shall never present metrics as `CURRENT` after a known invalidating event that has not been reconciled.

## Planner Rule

The planner shall consume the same publication epoch and freshness markers that the metadata subsystem publishes. It shall not use hidden family-specific freshness assumptions outside the published model.

## Operator Rule

Operator inspection shall be able to see:

- last publication epoch
- current freshness class
- invalidation reason if currently invalidated
- confidence state

## Non-Guarantees

This file does not require every metrics refresh to be synchronous. It requires explicit publication, freshness, and invalidation state.
