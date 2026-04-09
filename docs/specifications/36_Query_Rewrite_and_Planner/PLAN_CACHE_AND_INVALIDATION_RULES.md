# Plan Cache and Invalidation Rules

This file defines the required plan-cache lifecycle and invalidation behavior for ScratchBird.

## Plan cache lifecycle states

| State | Meaning |
| --- | --- |
| uncached | no reusable plan instance exists |
| candidate | a plan has been built but not yet admitted to cache |
| admitted | cache key and dependency set are recorded |
| executable | the plan is admitted and valid for execution |
| stale | a dependency changed but the entry has not yet been purged |
| invalidated | the entry must not execute and must be rebuilt before reuse |
| evicted | the entry was removed for capacity, version, or policy reasons |

## Required cache key fields

A reusable plan key must include at least:

1. canonical lowered query shape hash
2. rewrite-before-search contract id
3. parameter type shape and arity
4. dialect or protocol lowering contract
5. capability and security context
6. schema dependency epoch or equivalent metadata revision set
7. relevant statistics revision set for cost-sensitive plans
8. planner-control profile
9. engine plan-payload contract version

If any one of these is missing, the plan must not be admitted as reusable.

## Admission algorithm

1. Build the final chosen plan.
2. Record the exact dependency set used by rewrite, planning, and cost estimation.
3. Build the cache key from the required key fields.
4. Validate that the plan payload contract and runtime mode are compatible with cache reuse.
5. If any dependency cannot be tracked, keep the plan uncached.
6. Otherwise admit the plan and mark it executable.

## Invalidation triggers

A plan must transition to invalidated if any of the following changes affect its dependency set:

1. referenced table, view, index, or materialized object definition
2. column type, nullability, collation, or ordering semantics used by the plan
3. permission or security context used at bind time
4. planner-control profile or relevant configuration flags
5. statistics revision used by the cost-sensitive decision path
6. access-family capability or recheck requirement for a referenced index family
7. engine payload contract version or runtime opcode contract
8. rewrite contract identity for the same statement shape

## Invalidation state machine

1. executable to stale when a dependency update is observed but the entry has not yet been examined
2. stale to invalidated before any further execution attempt
3. invalidated to uncached after purge
4. invalidated to candidate only through full rebuild from rewrite stage 1
5. admitted or executable to evicted only through cache policy, never as a substitute for invalidation

## Execution guard rules

1. A stale plan must not execute.
2. An invalidated plan must not execute.
3. A plan built for one payload contract or capability profile must not execute under another.
4. Protocol-level prepare support does not by itself prove planner-cache reuse; prepare and planner cache remain distinct concepts.
5. Prepared fast-path bundles are governed separately by `HIGH_PERFORMANCE_OLTP_PLAN_SHAPES_CONTENTION_AVOIDANCE_AND_PREPARED_EXECUTION_MODEL.md`; they may reference a reusable plan, but they are not themselves the plan cache.

## Conservative fallback rule

If the cache manager cannot prove that all relevant dependencies are unchanged, it must invalidate and rebuild the plan rather than guess that reuse is safe.

## Explicit non-guarantees

- no universal prepared-plan lifetime guarantee
- no guarantee that every statement is cache-admissible
- no guarantee that all caches across the engine share one invalidation bus unless a later section proves it
