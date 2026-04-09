Status: current_authority

# Workload Routing, Admission, SLO, and Autoscale Catalog Model

## Purpose

This file defines the catalog-backed object model for workload routing,
admission policy, SLO evaluation, burn events, and autoscale policy.

## Current code-backed authority

The current catalog layer already persists and validates:

1. workload classes
2. workload routes
3. admission policies
4. admission bindings
5. SLO profiles
6. SLO bindings
7. SLO windows
8. SLO burn events
9. autoscale policies

## Workload class model

Each workload class row currently carries, at minimum:

- class ID
- class name
- match kind
- match text
- priority
- optional max latency
- cross-shard allowance flag

Current constraints proved by tests:

1. duplicate logical class identities are rejected
2. persisted class rows are retrievable by class ID

## Workload route model

Each route row currently carries, at minimum:

- route ID
- class ID
- route name
- target kind
- target label
- optional bound role
- transport
- route weight

Current constraints proved by tests:

1. duplicate logical routes are rejected
2. route rows are listed by class ID

## Admission policy model

Each admission policy row currently carries, at minimum:

- policy ID
- policy name
- max concurrent sessions
- max concurrent queries
- max queue depth
- CPU reject percentage
- memory reject percentage
- I/O reject percentage
- reject mode
- queue timeout

Beta 1 accelerator-governance extension on the same row:
- accelerator profile name
- accelerator memory budget bytes
- accelerator pinned residency target bytes
- accelerator concurrent build limit
- accelerator concurrent search limit
- accelerator prewarm policy
- accelerator fallback policy
- accelerator degraded-state override

## Admission binding model

Each admission binding row currently carries, at minimum:

- binding ID
- policy ID
- target kind
- class ID
- priority

Beta 1 accelerator-governance extension on the same row:
- accelerator device class
- accelerator device id, nullable
- accelerator device pool id, nullable

Current constraints proved by tests:

1. bindings are listable by policy ID
2. deleted bindings are no longer retrievable

## SLO profile model

Each SLO profile row currently carries, at minimum:

- profile ID
- profile name
- cluster node role
- availability target percentage
- latency p95 target
- latency p99 target
- error rate target percentage
- evaluation window
- short burn window
- long burn window
- moderate burn threshold
- high burn threshold
- critical burn threshold
- version

Current constraints proved by tests:

1. invalid threshold ordering is rejected
2. duplicate logical profile identities are rejected

## SLO binding model

Each SLO binding row currently carries, at minimum:

- binding ID
- SLO profile ID
- optional node ID
- role
- priority rank
- effective-from time
- optional effective-to time
- version

Current constraints proved by tests:

1. missing referenced profile is rejected
2. persisted bindings are retrievable by binding ID

## SLO window model

Each SLO window row currently carries, at minimum:

- window ID
- node ID
- role
- window start
- window end
- request count
- success count
- error count
- p95 latency
- p99 latency
- availability SLI percentage
- error-rate SLI percentage
- version

Current constraints proved by tests:

1. logically invalid request/success/error combinations are rejected
2. persisted windows are retrievable by window ID

## Burn event model

Each burn event row currently carries, at minimum:

- burn event ID
- node ID
- role
- SLO profile ID
- short burn rate
- long burn rate
- burn severity
- action plan
- event time
- optional resolved time

Current constraints proved by tests:

1. invalid resolved-time ordering is rejected
2. persisted burn events are retrievable by burn event ID

## Autoscale policy model

Each autoscale policy row currently carries, at minimum:

- autoscale policy ID
- role
- min nodes
- max nodes
- scale-out step
- scale-in step
- scale-out cooldown
- scale-in cooldown

Current constraints proved by tests:

1. `min_nodes > max_nodes` is rejected

## Catalog truth rule

These catalogs are the persistent truth for governance policy and evaluation
inputs. Runtime snapshots derive from these rows plus live counters and
telemetry.

## Fail-closed rule

The engine shall reject:

1. duplicate logical identities where uniqueness is required
2. invalid threshold orderings
3. invalid temporal orderings
4. invalid aggregate request/success/error relationships
5. bindings referencing missing required parents
