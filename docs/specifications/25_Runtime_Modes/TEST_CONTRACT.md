# Section 25 Test Contract

Section `25` is implementation-ready only if maintained evidence covers:
- local worker lifecycle, queueing, and scheduling surfaces
- workload-governance, routing-admission metadata, and SLO/error-budget evaluation surfaces
- local scheduler-backed maintenance execution and safety controls
- bounded parallel execution and listener/control-plane skew checks
- bounded cluster-write-safety routing, fencing, and epoch validation
- derivative `WAL_AFTER_*` debug/export scope where current code exposes it
- manager `DBBT` or `LPREFACE` binding admission and manager-proxy byte-relay
  behavior
- structured manager heartbeat or readiness inspection rows with bounded drift
  and queue posture fields
- bounded listener-control reachability and parser-pool readiness publication
- Beta 2 cluster membership epoch, fencing, and failover candidate selection
- synchronous HA acknowledgement rules and asynchronous DR lag publication
- explicit refusal of split-brain or stale-epoch promotion attempts
- distributed-query fragment admission, exchange-channel publication, spill, and
  retry rules
- OLTP service-class, preemption, hotspot-mitigation, and node-specialization
  behavior
- transactional event-topic, durable-queue, lease, dead-letter, and activation
  behavior
- scheduler job state, retry, alert, and message-sink behavior
- serverless autosuspend, resume, and warm-state refusal behavior
- replicated-topology role, read-route, and geo-failover fencing behavior
- memory-optimized OLTP lane admission, compiled-kernel invalidation, and
  hot-row refusal behavior
- shard split, merge, move, rebalance, and declared read-routing-mode behavior

## Excluded lanes

This section does not require proof of unsupported cluster consensus, membership, log replication, healing, or full distributed scheduler parity.
