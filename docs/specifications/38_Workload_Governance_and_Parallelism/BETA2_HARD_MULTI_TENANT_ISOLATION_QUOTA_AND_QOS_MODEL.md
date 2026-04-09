# Beta 2 Hard Multi-Tenant Isolation Quota And QoS Model

## Purpose

Define hard tenant isolation, quotas, reservations, and workload QoS so one
tenant cannot exhaust shared ScratchBird capacity beyond admitted policy.

## Governing rules

1. Tenant identity is a first-class admission and accounting key.
2. Reservations, ceilings, and burst rules are durable policy objects.
3. Fail-closed refusal is preferred over undocumented noisy-neighbor behavior.
4. MGA publication, maintenance, replay, and replication work all charge to the
   owning tenant unless declared global infrastructure work.

## Tenant identity

The canonical tenant key is `tenant_uuid`, bound to one or more:

- database roots
- schema roots
- service namespaces
- emulation roots where applicable

## Policy objects

- `sb_tenant_policy`
  - `tenant_uuid`
  - `service_class`
  - `cpu_quota_pct`
  - `io_quota_mb_s`
  - `concurrency_limit`
  - `maintenance_limit`
  - `network_limit`
  - `burst_policy`
  - `status`
- `sb_tenant_override_lease`
  - `lease_uuid`
  - `tenant_uuid`
  - `override_class`
  - `expires_at`
  - `operator_uuid`

## Isolation classes

- `HARD_RESERVED`
- `CAPPED_SHARED`
- `BURST_SHARED`
- `SUSPENDED`

## Admission flow

1. Resolve `tenant_uuid`.
2. Load the active tenant policy and override lease if any.
3. Check concurrency and queue budget.
4. Check memory reservation and ceiling with section 33.
5. Check IO and maintenance capacity.
6. Admit, queue, throttle, or refuse the request with one stable reason code.

## Guaranteed resources

`HARD_RESERVED` tenants shall have explicit minimums for:

- foreground worker slots
- memory reservation
- IO share floor
- maintenance execution slice

Reserved capacity may not be silently consumed by other tenants.

## Burst rules

`BURST_SHARED` tenants may consume unused shared headroom only when:

- no hard-reserved tenant is below its floor
- emergency reserve remains intact
- burst consumption stays within the declared burst window

## Maintenance and background work

Maintenance jobs must carry `tenant_uuid` or `global_infrastructure` identity.
Tenant-owned maintenance debt may be slowed or refused when the tenant exceeds
its declared policy.

## Refusal rules

- `TENANT_QUOTA_CPU_EXCEEDED`
- `TENANT_QUOTA_IO_EXCEEDED`
- `TENANT_CONCURRENCY_LIMIT_EXCEEDED`
- `TENANT_MAINTENANCE_LIMIT_EXCEEDED`
- `TENANT_SUSPENDED`

## Metrics

- admitted, queued, throttled, and refused work by tenant
- tenant queue depth and wait time
- resource-floor satisfaction for hard-reserved tenants
- burst consumption and burst refusals

## Cross-section requirements

- section 33 owns memory reservation enforcement
- section 38 owns admission, queueing, and execution slice policy
- section 20 owns tenant-visible diagnostics and audit
