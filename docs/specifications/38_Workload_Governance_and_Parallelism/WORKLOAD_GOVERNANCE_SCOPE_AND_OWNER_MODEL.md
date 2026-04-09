# Workload Governance Scope and Owner Model

This file owns the top-level workload-governance boundary for ScratchBird.

## Governance scope matrix

| Topic | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| server/runtime governance | current_bounded | workload governance exists in bounded server/runtime form where current code surfaces prove it | not a distributed resource manager |
| governance ownership | current_bounded | runtime governance remains outside parser and outside client contract ownership | not a client-side quota authority |
| per-workload classes | partial | workload classes may exist in bounded form where current runtime sections expose them | not a mature enterprise policy taxonomy |
| cluster-wide governance | fail_closed | no multi-node governance fabric is implied | not a scheduler for cluster coordination |

## Canonical rules

1. Governance claims must stay attached to current server/runtime surfaces.
2. Client tooling may expose controls, but does not own enforcement truth.
3. If a policy or quota surface is not directly proven, it remains fail-closed.

## Explicit non-guarantees

- no cluster resource manager
- no universal workload taxonomy
- no mature policy DSL guarantee
