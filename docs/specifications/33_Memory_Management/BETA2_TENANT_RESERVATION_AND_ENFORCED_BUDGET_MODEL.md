# Beta 2 Tenant Reservation And Enforced Budget Model

## Purpose

Define tenant-scoped memory reservations, ceilings, emergency reserves, and
breaker interactions required by Beta 2 hard multi-tenant isolation.

## Governing rules

1. Tenant memory accounting is mandatory for all admitted foreground and
   background work.
2. Reservation and ceiling enforcement must be deterministic.
3. A tenant may exhaust its own reservation without consuming protected reserve
   of other tenants.

## Budget objects

- `tenant_reservation_bytes`
- `tenant_ceiling_bytes`
- `tenant_spill_threshold_bytes`
- `tenant_emergency_reserve_bytes`
- `tenant_cache_target_bytes`

## Charging domains

Tenant charging shall include:

- statement execution memory
- operator grants
- temp and spill buffers
- cache residency where the cache object is tenant-owned
- maintenance debt working memory
- protected replay and replication staging memory

## Breaker states

- `UNDER_RESERVATION`
- `AT_RESERVATION_USING_SHARED_HEADROOM`
- `AT_CEILING_THROTTLED`
- `EMERGENCY_ONLY`
- `REFUSED`

## Enforcement flow

1. Charge the request to `tenant_uuid`.
2. If usage is below reservation, admit.
3. If usage exceeds reservation but is below ceiling, admit only if shared
   headroom is available.
4. If usage exceeds ceiling, spill, throttle, or refuse based on request class.
5. Preserve emergency reserve for rollback, recovery, and durable cleanup.

## Memory-class rules

- foreground OLTP may spill only when the request class admits it
- schema publication, checkpoint repair, and rollback have priority access to
  emergency reserve
- plan cache and metadata cache must honor per-tenant targets where objects are
  tenant-owned

## Refusal rules

- `TENANT_MEMORY_CEILING_EXCEEDED`
- `TENANT_MEMORY_SHARED_HEADROOM_UNAVAILABLE`
- `TENANT_MEMORY_EMERGENCY_ONLY`

## Metrics

- reservation usage by tenant
- ceiling breaches
- spill transitions by tenant
- emergency-reserve consumption

## Cross-section requirements

- section 33 owns memory charging and breaker enforcement
- section 38 owns tenant admission and policy control
