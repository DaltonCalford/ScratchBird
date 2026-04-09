# B-tree Verification and Hardening Framework

## Purpose
Define the validators, fuzzing, crash injection, and soak-testing framework
required to harden the B-tree to production-grade maturity.

## Scope
- page invariant validators
- full-tree consistency checks
- seeded fuzzers
- SMO crash injection and long-run stress

## Hard Invariants
1. Every structural B-tree contract must have an executable validator.
2. Every crash window in the SMO protocol must be fault-injectable.
3. Evidence bundles must be replayable from recorded seed and failpoint map.

## Validators
Required page-local validators:
- key ordering
- compression encoding and restart-anchor integrity
- sibling and high-key contract
- posting-list ordering and continuity
- page-class and reclaim metadata consistency

Required full-tree validators:
- child-range correctness
- parent pointer correctness
- metapage/root consistency
- duplicate logical-key correctness
- unreachable-page and orphan detection

## Fuzzing
The framework must support seeded randomized mixes of:
- insert
- delete
- update
- duplicate pressure
- compact
- split
- scan
- sweep/GC interaction

## Crash Injection
Mandatory failpoints include:
- split before right-page publication
- split after right-link publication, before parent publication
- merge after right-link redirect, before parent cleanup
- root publication before metapage sync
- rebuild publish before old-root retirement

## Soak and Regression
Nightly or staged hardening runs must capture:
- invariant failures
- metric regression
- lock contention by level
- restart repair counts

## Acceptance Criteria
- validators detect known corruption classes deterministically
- every structural operation is failpoint-covered
- long-run stress is reproducible from evidence bundles

## Cross-Section References
- `BTREE_STRUCTURAL_MODIFICATION_DURABILITY_PROTOCOL.md`
- `BTREE_MGA_VERSION_CHURN_MANAGEMENT.md`
- `../20_Diagnostics_Audit_and_Observability/BTREE_HARDENING_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md`
- `../31_Conformance_Performance_and_Reliability_Gates/BTREE_HARDENING_AND_CRASH_SAFE_INDEX_GATES.md`

## Legacy Mapping
| Historical source | Material preserved here |
| --- | --- |
| `specifications_old/indexes/BTREE_SPEC.md` | unit-level behavior elevated into invariant-driven validation |
| `specifications_old/indexes/INDEX_ARCHITECTURE.md` | architecture claims tied to executable hardening requirements |

## Gap Closure Mapping
- `SB-BTR-011`
