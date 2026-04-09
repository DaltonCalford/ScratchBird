Status: current_authority_beta1

# Process-Local Memory Arenas and Cache Ownership Model

## Purpose

This file defines the Beta 1 process-local arena layout, cache segregation,
rebalancing order, and derivative-memory shrink policy.

## Governing rule

ScratchBird does not assume one undifferentiated heap for all runtime memory.

The specification distinguishes:

1. durable-page residency
2. process-local reusable cache memory
3. per-session state
4. transient execution memory
5. spillable temporary page memory
6. resident index working sets
7. JIT metadata and artifact staging memory
8. executable code memory

## Canonical process-local arenas

Process-local memory shall be split into:

1. durable-page residency arena
2. derivative cache arena
3. session and transaction arena
4. transient execution arena
5. spillable temporary page arena
6. resident index arena
7. JIT metadata arena
8. JIT code arena

## Current code-backed arenas

The current codebase proves distinct ownership domains for:

1. buffer and page residency
2. result and query-result cache memory
3. translation and statement cache memory
4. permission cache memory
5. LSM block cache memory
6. JIT queue, runtime, and artifact-adjacent memory
7. connection-pool or session reuse memory

## Derivative cache classes

The derivative cache arena is subdivided into:

1. statement cache
2. translation cache
3. permission cache
4. executor result cache
5. pool-layer result cache
6. storage sidecar block cache
7. metadata and statistics helper cache
8. JIT artifact metadata cache

Each class shall publish:

- hard limit
- reserved floor
- shrink priority
- identity invalidation rules
- reason codes for trim or eviction

## Pressure order

Under memory pressure, the required reclamation order is:

1. transient execution memory
2. executor and pool result caches
3. translation and statement caches
4. metadata and permission caches when safe to recompute
5. storage sidecar caches
6. optional resident-index warm memory
7. JIT metadata cache and cold retired code handles
8. durable-page residency only under page-eviction discipline

This order prevents the engine from evicting authoritative locality layers
before cheaper derivative layers.

## Cache floors

Beta 1 requires non-zero floors for:

- permission cache
- translation cache
- statement cache
- metadata cache

Beta 1 default floors:

| Cache class | Floor |
| --- | --- |
| permission cache | `8 MiB` |
| translation cache | `16 MiB` |
| statement cache | `32 MiB` |
| metadata cache | `32 MiB` |

Result caches and storage sidecar caches may shrink to zero.

## Rebalancing rules

1. A cache class may borrow from another only through the budget tree.
2. Borrowed budget shall record donor class, recipient class, bytes, and
   reason code.
3. Borrowed budget is revocable under pressure.
4. Rebalancing may not violate cache identity or invalidation rules.

## Sample shrink plan

```cpp
ShrinkPlan buildShrinkPlan() {
  return {
      .steps = {
          shrinkExecutorResultCache,
          shrinkPoolResultCache,
          shrinkTranslationCache,
          shrinkStatementCache,
          shrinkPermissionCache,
          shrinkStorageSidecarCache,
          demoteResidentWarmState,
          retireColdJitMetadata,
      }
  };
}
```

## Beta 1 required expansion

The Beta 1 model requires:

1. explicit arena tagging for every major cache family
2. per-arena observability counters
3. deterministic eviction reason codes
4. bounded resident-index budgets separated from ordinary operator scratch memory

## Non-authority boundaries

The following are not acceptable:

1. treating JIT staging memory as durable cache truth
2. treating resident vector structures as interchangeable with transient scratch memory
3. evicting durable-page residency first while derivative caches remain hot
4. merging all derivative cache metrics into one undifferentiated "cache bytes"
