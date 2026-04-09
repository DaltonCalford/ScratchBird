# Memory Observability and Debugging Boundary

Status: current_authority_beta1

## Purpose

Define the Beta 1 observability surface for memory state, pressure incidents,
grant feedback, JIT memory, and fault injection.

## Canonical live views

The engine shall expose at least:

1. `sb_memory_contexts`
2. `sb_memory_pressure_events`
3. `sb_memory_grant_feedback`
4. `sb_jit_memory_artifacts`

## `sb_memory_contexts`

The `sb_memory_contexts` view shall expose at least:

| Column | Meaning |
| --- | --- |
| `node_uuid` | node identity |
| `parent_uuid` | parent identity |
| `node_kind` | context or budget kind |
| `memory_domain` | owning domain |
| `owner_uuid` | database, schema, connection, statement, task, or object UUID |
| `owner_label` | human-readable label |
| `soft_limit_bytes` | soft threshold |
| `hard_limit_bytes` | hard threshold |
| `reserved_bytes` | currently reserved |
| `committed_bytes` | currently committed |
| `peak_bytes` | high-water mark |
| `reclaimable_bytes` | reclaimable bytes |
| `spillable_bytes` | spillable bytes |
| `nonspillable_bytes` | resident bytes |
| `breaker_state` | current breaker state |
| `pressure_generation` | monotonic pressure counter |
| `last_pressure_reason` | last transition reason |

## `sb_memory_pressure_events`

Each event row shall include:

- event UUID
- timestamp
- node UUID
- domain
- requested bytes
- granted or refused bytes
- action taken
- refusal or cancellation code
- statement UUID when applicable

## `sb_memory_grant_feedback`

This surface shall mirror the persisted grant-feedback catalog rows and expose:

- operator kind
- sample count
- last grant
- percentile values
- spill count
- oscillation state

## `sb_jit_memory_artifacts`

This surface shall expose:

- artifact or tracker UUID
- object UUID
- schema-root UUID
- compatibility key hash
- publish state
- code bytes
- metadata bytes
- active calls
- retirement reason

## Required metrics

The engine shall emit:

- current bytes, committed bytes, and peak bytes by domain
- pressure transitions by reason code
- spill bytes and spill groups by operator class
- cache trim counts by cache family
- JIT compile-scratch and code-heap usage
- grant denials and grant feedback updates
- memory debt backlog by class

## Fault injection

Beta 1 shall provide fault-injection controls for:

1. allocation failure after N allocations
2. forced hard-limit clamp at a node
3. forced reclaim-wait timeout
4. forced spill write failure
5. forced JIT code publication denial

## Incident bundle rules

When a statement is canceled or refused for memory reasons, the support bundle
shall capture:

- the failing node path
- current breaker states on the parent chain
- requested bytes
- shrink or spill actions attempted
- grant record used
- statement UUID

## Non-conforming behavior

The following are forbidden:

1. aggregating all caches into one metric with no family identity
2. claiming live memory introspection without node-level limits and usage
3. hiding JIT code bytes inside generic executor totals
