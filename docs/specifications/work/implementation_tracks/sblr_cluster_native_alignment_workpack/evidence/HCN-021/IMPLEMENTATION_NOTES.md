# Implementation Notes - HCN-021

Code paths:
- `include/scratchbird/core/cluster_write_safety.h`
  - introduced `RoutingTarget`, `RoutingPlan`, `RoutingRequest`, `RoutingDecision`.
  - introduced `RoutingDecisionReason` and router API.
- `src/core/cluster_write_safety.cpp`
  - implemented deterministic hash routing and weighted bucket selection.
  - implemented stale epoch detection via `expected_routing_epoch` check.

Safety properties:
- deterministic hashing removes non-repeatable route decisions.
- stale routing metadata is rejected before write dispatch.
