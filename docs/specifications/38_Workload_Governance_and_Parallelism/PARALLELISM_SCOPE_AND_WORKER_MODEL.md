# Parallelism Scope and Worker Model

This file owns the bounded parallel execution model.

## Parallelism matrix

| Topic | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| bounded worker-based execution | partial | worker-based parallel execution may exist where current executor/runtime surfaces prove it | not a claim of broad intra-query parallelism |
| background or maintenance workers | current_bounded | maintenance and governance workers may exist in bounded runtime form | not a general task-fabric model |
| query-parallel execution | partial | parallel query semantics remain bounded to concrete current executor support | not mature cost-based parallel planning |
| cross-node parallelism | fail_closed | no distributed execution model is implied | not a cluster compute fabric |

## Canonical rules

1. Parallelism claims must distinguish maintenance workers from user-query workers.
2. If a parallel operator or path is not directly proven, it remains fail-closed.
3. Section 38 may summarize worker models, but may not imply distributed execution.

## Explicit non-guarantees

- no universal intra-query parallelism guarantee
- no broad parallel-plan optimizer contract
- no cross-node execution claim
