# Resident Vector Index Memory and Eviction Model

## Purpose

This document defines the host-memory and device-memory lifecycle for resident
vector and ANN index families.

The governing rule is:
- durable MGA storage is authoritative
- resident index structures are derivative working sets
- eviction, refresh, and prewarm must be explicit and governed

## Current code-backed authority

Current code-backed memory authority already proves:
- host buffer policy domains
- residency tiers
- dirty-state vocabulary
- thrash detection posture
- domain budgets and borrowing
- MGA-specific writeback staging, including
  `DirtyFlushedPendingFsync`

Current code also proves that multiple vector and ANN surface types route into a
common ANN runtime-family model.

## Canonical memory classes

Resident vector and ANN families shall classify memory into:
- `AUTHORITATIVE_HOST_PAGES`
- `HOST_RESIDENT_DERIVATIVE`
- `DEVICE_RESIDENT_DERIVATIVE`
- `HYBRID_RESIDENT_DERIVATIVE`
- `REBUILD_SCRATCH`

Only `AUTHORITATIVE_HOST_PAGES` participates in durable MGA truth.
All other classes are derivative working state.

## Resident-state machine

The canonical resident-state machine is:
- `UNLOADED`
- `LOADING`
- `READY`
- `REFRESH_PENDING`
- `DEGRADED`
- `EVICTING`
- `EVICTED`
- `FAILED`

Transitions must be externally inspectable.

## Budget classes

Every resident index shall be accounted against at least:
- host working-set bytes
- device working-set bytes
- rebuild scratch bytes
- refresh queue bytes
- pinned hotset bytes

Budgets shall be enforceable by workload governance or a directly delegated
resource-governance lane.

## Load-on-first-use rule

For families marked resident, the minimum required behavior is:
1. keep the durable index in MGA storage
2. on first use, load or build the derivative search structure
3. record the resident-state transition
4. keep the derivative structure resident until explicit governance or pressure
   policy changes that state

This is required reconstructed behavior for vector and similar families.

## Pinned hotset rule

A pinned hotset is a governance-controlled resident set that should remain in
memory or device memory once loaded.

Pinned hotset members:
- may not be evicted by ordinary opportunistic cache replacement
- may only be demoted or evicted by explicit pressure policy, maintenance
  action, or health degradation policy
- must expose their pin reason and current readiness state

## Eviction rules

Eviction order shall respect these priorities:
1. never evict authoritative durable pages as a substitute for derivative
   resident management
2. prefer evicting non-pinned derivative structures before pinned hotset members
3. prefer evicting degraded or stale resident structures before healthy hotset
   structures
4. if eviction is unavoidable, record:
   - resident class
   - bytes released
   - reason
   - whether fallback stayed available

Eviction may degrade latency. It may not lose committed truth.

## Refresh rules

When authoritative index or heap state changes, the resident structure shall
move to `REFRESH_PENDING` until refreshed.

Refresh may be:
- incremental
- batch
- full rebuild

The chosen mode must not publish derivative state ahead of the authoritative MGA
update boundary.

## Thrash and fairness controls

Resident vector structures shall participate in explicit anti-thrashing policy.
At minimum the runtime must track:
- repeated load or evict oscillation
- pinned hotset pressure
- borrowed budget pressure
- prewarm usefulness
- admission-driven forced fallback count

These signals are advisory policy inputs only. They do not redefine MGA truth.

## Required operator surfaces

The memory model shall expose at minimum:
- resident bytes per index
- resident class and location
- pinned-hotset membership
- last load time
- last refresh time
- last eviction time
- last eviction reason
- fallback availability

## Required reconstructed extension

The rebuild requires explicit canonical support for:
- device-backed resident working sets
- hybrid host/device resident structures
- hotset prewarm jobs
- resident-index eviction reasons and readiness markers

## Non-guarantees

Resident derivative structures do not guarantee:
- permanent residency across restart
- immunity from health-driven demotion
- freedom from rebuild after upgrade or capability mismatch

They guarantee only that the residency and eviction model is explicit,
observable, and subordinate to authoritative MGA storage.
