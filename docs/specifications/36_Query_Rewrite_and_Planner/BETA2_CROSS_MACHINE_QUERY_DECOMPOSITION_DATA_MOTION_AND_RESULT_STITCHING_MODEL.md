# Beta 2 Cross Machine Query Decomposition Data Motion And Result Stitching Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 planner contract for decomposing one canonical ScratchBird
plan into remote-executable fragments, explicit data-motion operators, and one
deterministic result-stitching path.

## Governing rules

1. Distributed query is not a second planner. One canonical plan is decomposed
   into fragments after ordinary Beta 2 search chooses a winning shape.
2. Remote fragments execute derivative work only. MGA visibility, commit truth,
   and security truth remain local authoritative state.
3. Every cross-machine edge is represented by one explicit motion class.
4. Result stitching is deterministic and explainable.

## Canonical fragment graph

Each distributed plan shall publish a fragment graph with:

- `query_uuid`
- `plan_uuid`
- `fragment_uuid`
- `fragment_role`: `COORDINATOR`, `LEAF_SCAN`, `LOCAL_PARTIAL`, `REMOTE_PARTIAL`,
  `REMOTE_FINAL`, `MERGE_ONLY`
- `location_scope`: `LOCAL_NODE`, `NODE_SET`, `SHARD_SET`, `PLACEMENT_POLICY`
- `required_properties`
- `output_layout_digest`
- `motion_in`
- `motion_out`
- `attempt_policy`

The planner shall never publish anonymous remote work units.

## Admitted motion classes

- `LOCAL_ONLY`
- `GATHER`
- `GATHER_MERGE`
- `BROADCAST`
- `REPARTITION_HASH`
- `REPARTITION_RANGE`

No Beta 2 implementation may hide repartition or broadcast semantics inside a
 generic "remote exchange" node.

## Pushdown eligibility

Pushdown is legal only when the fragment boundary preserves:

- required predicate semantics
- required ordering semantics
- required exactness or recheck posture
- security and tenant visibility constraints
- donor-visible or native transaction-snapshot scope

The planner shall admit remote pushdown for:

- base scans
- filter
- projection
- local partial aggregate
- repartition-ready join build or probe branches
- top-N when the ordering contract is preserved

The planner shall refuse pushdown for:

- operators that require coordinator-only mutable session state
- operators that cross a fail-closed security boundary
- operators whose exactness contract would be weakened remotely

## Decomposition algorithm

1. Start from the chosen canonical physical plan.
2. Mark operator subtrees with eligible execution locations.
3. Cut the plan only at legal motion boundaries.
4. Create fragment envelopes for each cut subtree.
5. Assign one explicit motion class to every cut edge.
6. Attach required ordering, exactness, and collation properties to each
   fragment output.
7. Publish one local coordinator fragment that performs final stitching,
   residual recheck, or final aggregate where required.

## Result stitching rules

- `GATHER` concatenates peer-compatible result streams.
- `GATHER_MERGE` requires sort-key-compatible fragment outputs and performs one
  stable merge.
- `REPARTITION_HASH` and `REPARTITION_RANGE` require one final consumer stage
  that owns correctness for downstream join or aggregate completion.
- partial aggregates must identify the final aggregate operator explicitly.
- ranked, approximate, or recheck-required paths must preserve their trust class
  through the stitch stage.

## Explain and diagnostics

`EXPLAIN` and plan traces shall expose:

- fragment count
- fragment locations
- motion classes
- rows and bytes per exchange edge
- remote versus local CPU time
- stitch operator role

## Refusal rules

- `DIST_QUERY_FRAGMENT_LOCATION_MISSING`
- `DIST_QUERY_MOTION_CLASS_UNRESOLVED`
- `DIST_QUERY_PUSH_DOWN_REFUSED`
- `DIST_QUERY_RESULT_STITCH_PROPERTY_MISMATCH`

## Metrics

- fragment launch count
- remote rows and bytes shipped
- exchange spill bytes
- gather-merge fan-in
- coordinator stitch CPU and latency

## Cross-section requirements

- section `25` owns runtime fragment execution and exchange admission
- section `24` owns node-capability, cost, and exchange-policy rows
- section `36` owns decomposition, pushdown, motion choice, and result stitch
  semantics
