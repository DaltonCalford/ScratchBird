# Cluster Routing and Admission

Status: current_authority

## Current authority

Current section authority is limited to:
- workload-governance routing and admission metadata
- cluster-write-safety primitives such as routing epoch, fencing token, and session pin
- bounded routing or admission snapshots consumed by current runtime surfaces

## Current guarantees

- routing/admission state may be recorded and consumed now
- write-safety primitives exist and are current implementation-relevant boundaries
- current section `25` does not need a full distributed router to define these bounded surfaces

## Non-claims

- full distributed route selection algorithms
- complete cluster failover, rebalancing, or multi-hop admission control parity
