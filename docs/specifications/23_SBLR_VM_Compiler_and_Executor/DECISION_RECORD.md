# Decision Record

## Current decisions

1. Planner authority is code-first and centered on `QueryPlanner::planStatement(...)`, `StatementPlanRequest`, and `StatementPlanResult`.
2. The authoritative compile path is parse, emit, finalize, validate, then execute or explain; there is no separate stable VM linker contract.
3. Optimizer and runtime-plan truth are the current planner, join-ordering, index-family, statistics, selectivity, and plan-payload paths.
4. Cache truth is split: `VNextPlanCache` is current plan-cache authority; result-cache behavior is narrower and separate.
5. Execution diagnostics, budget, spill, lock, and GC claims in this section are limited to the current planner and payload surfaces it directly owns.
6. Execution-artifact catalog claims remain bounded to catalog exposure actually shared with section `24`.
7. Native-compilation, compute-capsule, distributed scheduler, and bulk-load narratives are not current implementation authority unless explicitly promoted.
