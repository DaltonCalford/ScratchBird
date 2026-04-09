# Beta 2 Service Tiers Tenant Pools And Workload Governance Model

## Purpose

Define the operator-facing service-tier and tenant-pool model that binds the
existing quota, reservation, QoS, and OLTP substrate into a coherent product.

## Governing rules

1. Every database or tenant may be assigned to one service tier.
2. Service tiers resolve to explicit budgets, admission limits, and SLO policy.
3. Overcommit and burst are explicit policy, not silent behavior.
4. Tenant pools are catalog objects with membership history.

## Canonical metadata

- `sb_service_tier`
  - `tier_uuid`
  - `tier_name`
  - `cpu_budget`
  - `memory_budget`
  - `io_budget`
  - `burst_policy`
- `sb_tenant_pool`
  - `pool_uuid`
  - `pool_name`
  - `tier_uuid`
  - `membership_policy`
- `sb_tenant_pool_member`
  - `pool_uuid`
  - `tenant_uuid`
  - `joined_at`
  - `status`

## Control-plane flows

- tier publication
- tenant assignment
- budget enforcement
- burst admission
- penalty and quarantine on repeated abuse

## Refusal rules

- `SERVICE_TIER_UNKNOWN`
- `TENANT_POOL_MEMBERSHIP_REFUSED`
- `BURST_POLICY_EXHAUSTED`
- `GOVERNANCE_POLICY_CONFLICT`

## Metrics

- per-tier saturation
- tenant throttling events
- burst admissions
- fairness violations

## Cross-section requirements

- section `38` owns tiers and pools
- section `33` owns budget packets
- section `25` owns runtime admission enforcement
