# B-tree Optional Write and Lookup Acceleration

## Purpose
Define the optional post-hardening acceleration lane for deferred cold-page
updates and hot-key lookup acceleration.

## Scope
- deferred write buffering for cold nonresident leaf pages
- adaptive hot-key acceleration
- gating and proof obligations

## Hard Invariants
1. This lane is optional and may not weaken any correctness, durability, or
   restart rule in the mandatory B-tree hardening bundle.
2. No optional accelerator may ship before sections `SB-BTR-001` through
   `SB-BTR-009` and `SB-BTR-011` pass their gate suites.
3. Unique indexes and active shadow rebuilds may not use deferred leaf-update
   buffering.

## Optional Features
### Deferred Leaf-Update Buffer
Allowed only for:
- non-unique secondary B-tree indexes
- cold nonresident leaf pages
- buffered operations whose replay is metapage-anchored and restart-safe

### Adaptive Hot-Key Acceleration
Allowed only for:
- repeated point lookups with proven hot-key stability
- accelerators whose invalidation is tied to root publication sequence and
  duplicate/posting updates

## Disable Conditions
Must disable automatically when:
- failpoint or crash-window coverage is absent
- online rebuild or relocate is active
- reclaim quarantine backlog exceeds threshold
- diagnostics report accelerator inconsistency

## Acceptance Criteria
- optional subsystem ships only with replayable correctness proof
- measurable improvement exists on write-heavy or hot-key workloads
- disabling the subsystem restores canonical behavior deterministically

## Cross-Section References
- `BTREE_STRUCTURAL_MODIFICATION_DURABILITY_PROTOCOL.md`
- `BTREE_VERIFICATION_AND_HARDENING_FRAMEWORK.md`
- `../31_Conformance_Performance_and_Reliability_Gates/BTREE_HARDENING_AND_CRASH_SAFE_INDEX_GATES.md`

## Legacy Mapping
| Historical source | Material preserved here |
| --- | --- |
| none | new optional post-hardening extension scope |

## Gap Closure Mapping
- `SB-BTR-010`
