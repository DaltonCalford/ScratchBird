# Engine Time Source and Ordering Discipline

Status: current_authority

## Current authority

Current proof is limited to local monotonic-vs-wall-clock discipline and bounded listener/control-plane skew checks.

## Current guarantees

- deadlines, timeouts, retries, pacing, and scheduling must use monotonic time
- user-visible timestamps, audit timestamps, and similar publication surfaces use wall-clock time
- durable correctness does not come from wall-clock ordering
- bounded control-plane skew checks exist in current listener/control paths

## Non-claims

- cluster-wide clock discipline
- consensus-grade time ordering
