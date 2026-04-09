# Beta 2 High Performance OLTP Service Class And Node Specialization Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 runtime model for high-rate OLTP service classes, node
specialization, hot-key mitigation, and protection of OLTP latency from
analytics and maintenance.

## Governing rules

1. OLTP latency protection outranks background throughput.
2. OLTP service classes must use existing scheduler and budget infrastructure;
   no hidden bypass queues are allowed.
3. Prepared point reads and writes receive one privileged fast path.
4. Hotspot mitigation must be deterministic and operator-visible.

## Service classes

- `OLTP_CRITICAL`
- `OLTP_STANDARD`
- `OLTP_BURST`
- `HTAP_BACKGROUND`
- `BULK_LOAD`
- `MAINTENANCE`

Each class shall publish:

- queue ceiling
- memory ceiling
- remote-fragment eligibility
- spill eligibility
- preemption rank

## Node specializations

- `INGRESS_OLTP`
- `PRIMARY_WRITER`
- `LOCAL_READ`
- `ANALYTICAL`
- `MAINTENANCE_HEAVY`

Nodes may advertise multiple roles, but every deployed cluster must explicitly
publish which role combinations are legal.

## Admission and preemption

1. `OLTP_CRITICAL` may preempt `HTAP_BACKGROUND`, `BULK_LOAD`, and
   non-safety-critical maintenance.
2. `OLTP_STANDARD` may not be starved by unlimited analytical scans.
3. Admission must fail closed before OLTP consumes the last protected reserve
   of another higher-priority class.

## Hot-key mitigation

When hot-key thresholds are exceeded, the runtime shall apply one or more
declared policies:

- route-local micro-batching
- key-salt or hash-spread routing where schema policy allows it
- split proposal emission for load-based range families
- dedicated writer placement for the affected shard or range

The runtime may not silently change key semantics.

## Leveraged Beta 1 substrate

The OLTP lane shall reuse:

- commit-group batch apply
- same-key exact update suppression
- cold-page exact-secondary buffering
- right-edge mitigation on ordered exact families

## Metrics

- p50, p95, and p99 latency by service class
- admission wait time by service class
- hot-key detection count
- preemption count
- OLTP versus HTAP interference count

## Refusal rules

- `OLTP_SERVICE_CLASS_UNBOUND`
- `OLTP_PROTECTED_RESERVE_EXHAUSTED`
- `OLTP_HOT_KEY_POLICY_REQUIRED`
- `OLTP_SPECIALIZED_NODE_MISSING`

## Cross-section requirements

- section `25` owns service classes, admission, preemption, and node roles
- section `36` owns the plan shapes that qualify for OLTP fast paths
- section `31` owns OLTP benchmark and contention certification
