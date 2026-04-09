# ScratchBird Index Optimizer Contradiction Register

Status: Active

Date: 2026-03-14

Primary sources:
- section 18 index engineering pack
- section 23 optimizer implementation contracts
- live ScratchBird optimizer and index-planning code

## Purpose
Record the highest-signal contradictions between the new family-aware index
contracts, existing optimizer implementation contracts, and live code behavior.

## Contradiction Register

| Id | Class | Contradiction | Canonical target | Current evidence | Required closure |
| --- | --- | --- | --- | --- | --- |
| `IX-OPT-01` | `spec_gap` | section 23 still used generic index path families | section 23 access-path and runtime-plan contracts | `ACCESS_PATH_ORDERING_AND_UPPER_STAGE_PLANNING.md` previously named generic `index scan` and `bitmap scan` families | completed canonical reconciliation in section 23; implementation still must adopt canonical path names |
| `IX-OPT-02` | `spec_gap` | pass order placed join search before access-family derivation | section 23 pass pipeline and architecture | `OPTIMIZER_PASS_PIPELINE.md` previously ran `P08_JOIN_ORDER_PLAN` before `P09_ACCESS_PATH_ANNOTATE` | completed canonical reconciliation; implementation and gate suite must follow new order |
| `IX-OPT-03` | `code_gap` | planner path payload cannot represent exactness, recheck, coverage, or candidate budget | section 18 and section 23 path descriptor contracts | live `PathType`, `AccessPathDescriptor`, and `RuntimePlanRelation` remain generic | extend runtime plan payloads and path descriptors in code |
| `IX-OPT-04` | `code_gap` | base-relation planning collapses to one winner before join search | section 23 join-search and access-path contracts | live `QueryPlanner` retains one `access_choices` leaf per relation | implement multi-candidate bundles into join search |
| `IX-OPT-05` | `code_gap` | predicate matching remains BTREE or LSM-centric and bypasses canonical lowering | section 18 taxonomy and family planner contracts | live semantic analyzer and planner do not enumerate most families | implement family lowering and predicate-family routing |
| `IX-OPT-06` | `code_gap` | index costing still uses hard-coded B-tree or heap-shaped heuristics | section 18 family metrics and section 23 cost governance | live cost model and query planner invent fixed heights and run counts | consume typed family metrics packets and confidence classes |
| `IX-OPT-07` | `code_gap` | generalized families cannot express lossy recheck semantics end to end | section 18 generalized-search and exactness contracts | live GiST and SP-GiST APIs return `bool` only and hardcode `OVERLAPS` search | add recheck-capable support-function and runtime-plan fields |
| `IX-OPT-08` | `code_gap` | index-only and visibility behavior still conflict with heap-authoritative MGA model | section 18 MGA publication and visibility contracts | live executor heap-fetches `INDEX_ONLY_SCAN`, while bitmap and columnstore still persist authority-like visibility state | align index-only legality and remove index-native visibility authority claims |
| `IX-OPT-09` | `spec_gap` | plan-cache and execution-artifact contracts under-key family-aware plan reuse | section 23 plan cache and catalog contracts | prior checklist and catalog tables omitted stats snapshot and family signature | completed canonical reconciliation; implementation schema and cache keys must follow |

## Severity Rule
- `code_gap` items block implementation claims.
- `spec_gap` items block authority claims until section text is reconciled.
- `naming_collision` items block explainability and support-surface claims.

## Immediate Status
- section-23 canonical text has been reconciled for `IX-OPT-01`, `IX-OPT-02`,
  and `IX-OPT-09`
- all `code_gap` items remain open and feed the conversion wave

## Required Next Closure Artifacts
1. runtime-plan payload conversion spec
2. access-path descriptor conversion spec
3. family-lowering implementation note
4. metrics-packet catalog integration spec
5. generalized recheck contract addendum
6. visibility and index-only contradiction resolution note
