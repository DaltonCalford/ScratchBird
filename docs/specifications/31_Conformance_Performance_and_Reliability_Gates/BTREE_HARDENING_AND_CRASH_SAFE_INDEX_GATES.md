# B-tree Hardening and Crash-Safe Index Gates

## Purpose
Define the mandatory gate suites that promote ScratchBird B-tree hardening from
design intent to release-blocking evidence.

## Scope
- crash-safe structural modification durability
- compressed-page search acceleration and separator rigor
- split-tolerant descent under concurrent search, scan, and cleanup
- reclaim quarantine, duplicate pressure, MGA churn, and validator coverage

## Hard Invariants
1. A B-tree promotion candidate is not releasable until crash-window,
   concurrency, and long-run validation evidence all pass.
2. Gate success requires canonical section 20 evidence surfaces; private ad hoc
   metrics are insufficient.
3. No gate may weaken MGA truth, metapage root truth, or reclaim-quarantine
   safety to improve performance.

## Gate Suite `G15_BTREE_HARDENING`

### `T31-G15-01` SMO Durability Crash-Window Matrix
- inject crash/restart at every SMO phase for split, merge, root change, and
  bulk publish
- acceptance:
  - startup restart repair converges to one legal tree
  - no page marked reclaimable remains reachable
  - `sb_btree_incomplete_smo` returns to zero after repair

### `T31-G15-02` Compressed-Page Search Acceleration
- run seeded search workloads across compressed internal and leaf pages with
  varied restart density
- acceptance:
  - decoded keys per probe remain within the canonical bound for the seed set
  - restart-anchor and jump-table logic return identical search answers to the
    validator walk
  - no decoded-keys regression alert fires in the passing profile

### `T31-G15-03` Separator Truncation and Pivot-Range Validation
- run split, rebalance, and rebuild workloads that stress minimal separators,
  high keys, and duplicate boundaries
- acceptance:
  - each parent separator preserves left/right key-range truth
  - suffix truncation never loses ordering or visibility correctness
  - separator-length and truncation-savings metrics are internally consistent

### `T31-G15-04` Split-Tolerant Concurrent Split/Search/Scan
- run concurrent search, insert, split, scan, and diagnostic workloads with
  failpoints on right-link publication and parent update
- acceptance:
  - readers and scanners return stable logical results after bounded retries
  - no orphan right-link, stale parent, or illegal fence/high-key outcome
  - split-retry counters and health surfaces remain consistent with observed
    retries

### `T31-G15-05` Deletion, Reclaim Quarantine, and Duplicate/Posting-List Gates
- run delete, merge, duplicate-heavy insert, posting-list compaction, and MGA
  churn workloads
- acceptance:
  - reclaimable pages enter quarantine before allocator reuse eligibility
  - duplicate/posting-list behavior preserves key completeness and visibility
  - dead-version-density and compact-before-split counters match the seeded
    workload outcome

### `T31-G15-06` Invariant Validator, Seeded Fuzzing, and Soak
- run deterministic tree-validator sweeps, seeded mutation fuzzers, and long
  duration soak workloads
- acceptance:
  - validator reports zero unrepaired structural violations
  - seeded fuzzing is reproducible from the recorded seed map
  - soak runs complete without unrepaired corruption, monotonic health drift, or
    evidence gaps

## Required Evidence
Each gate run must emit:
- failpoint seed map and workload seed/config hash
- B-tree health metrics bundle from section 20
- tree-validator output
- restart repair report
- benchmark seed and regression summary

## Promotion Criteria
1. all `T31-G15-*` suites pass across 10 repeated runs
2. failure rate is <= 1%
3. no run records unrepaired structural violations, illegal page reuse, or
   missing evidence artifacts
4. evidence artifacts are stored under `docs/specifications/work/`

## Cross-Section References
- `../18_Index_Framework/BTREE_STRUCTURAL_MODIFICATION_DURABILITY_PROTOCOL.md`
- `../18_Index_Framework/BTREE_COMPRESSED_PAGE_SEARCH_ACCELERATION.md`
- `../18_Index_Framework/BTREE_PIVOT_TUPLE_AND_SEPARATOR_KEYS.md`
- `../18_Index_Framework/BTREE_CONCURRENCY_AND_SPLIT_TOLERANT_DESCENT.md`
- `../18_Index_Framework/BTREE_PAGE_DELETION_MERGE_AND_RECLAMATION.md`
- `../18_Index_Framework/BTREE_DUPLICATE_KEY_AND_POSTING_LIST_MANAGEMENT.md`
- `../18_Index_Framework/BTREE_MGA_VERSION_CHURN_MANAGEMENT.md`
- `../18_Index_Framework/BTREE_VERIFICATION_AND_HARDENING_FRAMEWORK.md`
- `../20_Diagnostics_Audit_and_Observability/BTREE_HARDENING_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md`

## Legacy Mapping
| Historical source | Material preserved here |
| --- | --- |
| `specifications_old/indexes/BTREE_SPEC.md` | B-tree correctness and maintenance expectations elevated into release-blocking crash, concurrency, and validation gates |

## Gap Closure Mapping
- `SB-BTR-011`

## 2026-03-28 Audit Normalization Update

- Section `31` is normalized to the code-backed `partial` standard.
- Current gate authority is bounded to the shipped engine and driver gate entry points, especially `ScratchBird/docs/TEST.md`, `tests/conformance/public_beta/run_required_public_beta_gate.sh`, `tests/compatibility/*`, engine unit/integration/benchmark/stress suites, and driver build or implementation-gate reports under `ScratchBird-driver/docs/`.
- The required public-beta gate is the strongest current section-local release-gate authority, but it is still a bounded gate script and category set rather than proof of a fully unified enterprise certification framework.
- Compatibility manifests, benchmark suites, driver build matrices, and implementation gate reports are current evidence surfaces; they are not universal proof that every numbered section `31` gate is live, mandatory, and fully replayable.
- Performance, optimization, and scorecard language is bounded to the current benchmark or readiness evidence, not a completed cross-platform SLO certification program.
- Cluster gameday, operator runbook, replication, upgrade or rollback orchestration, full forensic shadow gating, and broad platform certification language remain bounded, checklist-oriented, or `target_state_only` unless direct gate scripts and replayable evidence bundles exist.
- Evidence artifact matrices and phase-dependency matrices are treated as planning or inventory surfaces unless matched by executed gate runners and preserved result artifacts.
- MGA recovery remains state-based and not WAL/redo replay; replay language in this section must stay compatible with current recovery audits.
