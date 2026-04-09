Status: reconstructed_required

# Family Metrics Refresh Staleness and Replan Trigger Model

## Purpose

This document defines how the optimizer treats stale family-native metrics, when metrics refresh is required, and when replanning is required.

## Canonical Rule

Family-native metrics participate in planning only when their freshness class and confidence are known. Staleness may degrade a family’s cost position, but it does not silently remove the family from primary-class eligibility.

## Freshness Classes

Metrics freshness shall be classified as:

- `CURRENT`
- `AGED`
- `STALE_DEGRADED`
- `UNUSABLE`

## Refresh Triggers

Metrics refresh may be triggered by:

- large data-volume change
- index maintenance event
- resident-index warm-load or rebuild
- statistics invalidation
- explicit administrative refresh
- repeated optimizer uncertainty for the same family

## Replan Triggers

The planner shall consider replanning when:

- a currently executing or cached statement crosses a metrics epoch boundary
- accelerator admission state changes materially
- a resident index transitions between unloaded, warming, admitted, and degraded states
- family metrics move from `UNUSABLE` or `STALE_DEGRADED` to a stronger class

## Cached Plan Rule

Cached plans may survive aged metrics only when the plan-cache policy still considers them safe. If a family’s metrics or admission state materially change candidate ranking, the plan cache shall mark the plan for invalidation or explicit degraded reuse policy.

## Explanation Rule

When staleness changes the winner or suppresses a preferred candidate, the optimizer trace shall expose:

- freshness class before planning
- penalties applied
- whether refresh was attempted
- whether replanning was required or skipped

## Non-Guarantees

This file does not require synchronous refresh before every plan. It requires explicit freshness classes, penalties, and replan boundaries.
