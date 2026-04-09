# Non Guarantee and Partial Bulk Data Behavior

This file owns the explicit exclusions for section 39.

## Non-guarantee matrix

| Topic | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| full backup ecosystem | fail_closed | only bounded native tooling and validation truth may be claimed | not a mature enterprise backup suite |
| logical bulk data surface parity | fail_closed | bulk data semantics remain narrower than donor COPY or dump ecosystems | not syntax parity by inference |
| operational certification | fail_closed | no certification-grade backup or restore program is implied without artifact proof | not planning-as-proof |
| disaster-recovery breadth | fail_closed | disaster-recovery claims remain bounded to current validated restore truth | not HA/DR platform equivalence |

## Canonical rules

1. Section 39 must over-disclose exclusions rather than widen operational claims.
2. Missing mode coverage remains fail-closed even if analogous donor features exist.
3. Planning and future work are not current guarantee surfaces.

## Explicit non-guarantees

- no complete enterprise backup matrix
- no guaranteed logical movement parity across clients
- no broad disaster-recovery platform claim
