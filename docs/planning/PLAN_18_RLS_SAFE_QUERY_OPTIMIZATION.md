# PLAN_18_RLS_SAFE_QUERY_OPTIMIZATION.md

## Status
**Priority:** P0 – Security Critical  
**Phase:** Alpha Exit Gate  
**Owner:** Optimizer / Executor  

## Problem Statement
Current query optimization capabilities (predicate pushdown, join reordering, index access, LIMIT short-circuiting,
vector execution, and future parallelism) can bypass Row Level Security (RLS) unless explicitly constrained.
This represents a silent data-leak risk.

## Invariants (MUST HOLD)
1. MGA visibility is evaluated first.
2. RLS policies are evaluated before any user-visible filtering or limiting.
3. No optimizer rule may reorder, bypass, or partially evaluate RLS.

## Required Architecture Change
Introduce an explicit optimizer-visible plan node:

```
SeqScan/MVCCScan
  → RLSFilter (security barrier)
      → UserFilter
          → Project
              → Limit
```

## Tasks
- OPT-A1: Add RLSFilter plan node (non-movable barrier)
- OPT-A2: Modify planner to always emit RLSFilter for SELECT
- OPT-A3: Constrain predicate pushdown across barrier
- OPT-A4: Constrain join reordering across barrier
- OPT-A5: Forbid index-only scans missing policy columns
- OPT-A6: Make LIMIT/OFFSET RLS-safe
- OPT-A7: Make plan cache key include RLS context hash
- OPT-A8: EXPLAIN output must show RLSFilter placement

## Tests Required
- RLS + LIMIT leak tests
- RLS + ORDER BY + index tests
- RLS + JOIN reorder tests
- RLS + plan cache cross-user isolation tests

## Exit Criteria
- No RLS bypass possible under any optimizer transformation
- All tests pass
- EXPLAIN clearly documents security barrier
