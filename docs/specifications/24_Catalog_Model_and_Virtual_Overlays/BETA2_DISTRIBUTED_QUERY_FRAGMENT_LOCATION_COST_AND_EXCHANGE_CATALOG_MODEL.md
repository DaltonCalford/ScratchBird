# Beta 2 Distributed Query Fragment Location Cost And Exchange Catalog Model

Status: reconstructed_required_beta2

## Purpose

Define the catalog rows required to describe distributed-query node capability,
location cost, exchange policy, and operator-visible fragment history.

## Governing rules

1. Planner-visible distributed execution metadata must be catalog-backed.
2. Node capability and locality cost are separate from transient queue depth.
3. Runtime queue state may refresh frequently, but planner identity rows must be
   stable and versioned.

## Canonical row families

- `sb_dist_node_capability`
  - node uuid, node role class, locality label, supported fragment classes,
    exchange classes, max worker slots, memory class
- `sb_dist_locality_cost`
  - source locality, destination locality, latency class, bandwidth class,
    egress penalty, merge penalty
- `sb_dist_exchange_policy`
  - policy uuid, motion class, spill allowed, compression allowed,
    encryption required, max in flight bytes
- `sb_dist_fragment_profile`
  - profile uuid, fragment class, average rows, average bytes, cpu score,
    spill rate, failure rate
- `sb_dist_remote_queue_state`
  - node uuid, service class, queued fragments, active fragments, admitted
    bytes, last refresh time

## Publication rules

1. `sb_dist_node_capability` changes only on node-role or binary-capability
   publication.
2. `sb_dist_locality_cost` changes only through controlled topology or SLO
   policy mutation.
3. `sb_dist_fragment_profile` is updated from completed fragment telemetry, not
   speculative estimates.
4. `sb_dist_remote_queue_state` is a bounded refresh family and may be stale
   between refresh points.

## Overlay rules

- operator views shall expose queue state and fragment history without making
  them authoritative placement truth
- quarantined nodes remain visible to operators but are hidden from ordinary
  fragment placement

## Refusal rules

- `DIST_NODE_CAPABILITY_ROW_MISSING`
- `DIST_LOCALITY_COST_ROW_MISSING`
- `DIST_EXCHANGE_POLICY_REFUSED`
- `DIST_REMOTE_QUEUE_STATE_STALE`

## Cross-section requirements

- section `24` owns the catalog families and overlay visibility
- section `25` consumes queue state and capability rows during admission
- section `36` consumes locality and fragment-profile rows during planning
