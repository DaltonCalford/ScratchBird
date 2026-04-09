# Planner Strategy and Plan Stability

This file defines the required planner decision procedure, tie-break behavior, and plan stability envelope.

## Planner strategy model

ScratchBird planning is path-oriented.
The planner constructs candidate access or execution paths, evaluates legality and cost, chooses a winning path, then emits a runtime plan and runtime trace artifacts.
The planner is not allowed to claim global optimality unless a later section proves exhaustive search and certification.

## Required planner decision procedure

1. Accept only a frozen rewrite-before-search contract and its lowered access metadata.
2. Enumerate legal base access paths for each relation or logical stage.
3. Reject illegal paths before cost comparison.
4. Estimate selectivity, row counts, row width, startup cost, and total cost for each surviving path.
5. Build composite paths such as join, aggregate, sort, limit, and parallel wrapper candidates only when their legality preconditions hold.
6. Compare candidate paths inside each planning stage using the current cost model and tie-break rules.
7. If a parallel wrapper candidate is considered, compare its total cost against the corresponding serial path and reject the wrapper when its total cost exceeds the serial path.
8. Apply late wrappers such as gather, gather-merge, limit, or offset only after the underlying path has been costed.
9. Emit the chosen runtime plan plus rejected-path trace reasons.
10. Freeze the chosen plan for execution or cache admission.

## Tie-break rules

When two candidate paths are both legal and their semantics are equivalent, the planner must use the following order:

1. lower total cost wins
2. if total cost ties, lower startup cost wins for fast-start contexts and cursor-biased contexts; otherwise lower run cost wins
3. if cost still ties, prefer the path with fewer required rechecks
4. if still tied, prefer the path with fewer spill expectations
5. if still tied, prefer the simpler distribution mode, ordered as serial before parallel unless the workload contract explicitly requires parallel distribution
6. if still tied, prefer the path with the smaller structural search surface so plan identity remains stable

## Plan stability envelope

A plan may be treated as stable only within a fixed planning envelope:

1. same canonical lowered query shape
2. same rewrite-before-search contract id and frozen pass identity
3. same parameter type and arity shape
4. same security and capability context
5. same schema and object metadata epoch
6. same relevant statistics revision set
7. same planner-control profile and runtime mode
8. same engine opcode and payload contract version

If any one of those inputs changes, plan stability is not guaranteed.

## Stability classes

| Stability class | Meaning | Allowed claim |
| --- | --- | --- |
| deterministic_within_envelope | same envelope yields same chosen path and plan hash | allowed only when all envelope inputs are held constant |
| heuristically_stable | most path choices remain stable but some ties may move when cost inputs move | allowed only with explicit traceability |
| invalidated | envelope changed and the old plan must not be reused | mandatory when invalidation triggers fire |

## Parallel and adaptive boundary

1. Parallel path construction is allowed only as an explicit candidate family with separate legality and cost evaluation.
2. Parallel wrappers are not presumed correct or efficient by default; they must win the same legality and cost comparison as serial paths.
3. No adaptive replanning is permitted during execution unless a later section explicitly defines it.
4. Runtime instrumentation may annotate a plan after selection but may not replan silently.

## Required plan outputs

Every chosen plan must be able to report:

1. chosen access path family per stage
2. startup and total cost
3. estimated rows
4. distribution mode or serial mode
5. whether recheck is required
6. rejected-path reasons for materially considered alternatives

## Explicit non-guarantees

- no global optimality guarantee
- no exhaustive search guarantee across all path families
- no universal deterministic plan guarantee outside the stability envelope
- no adaptive or feedback-driven replanning guarantee

## Competitive parity closure requirements

The competitive-performance parity package requires explicit intra-query
parallel planning closure for admitted workload shapes.

For benchmark-governed scan, join, sort, aggregate, distinct, and window
processes, the planner shall:

1. enumerate legal serial and parallel candidates when the runtime can execute
   them
2. cost the parallel candidate with explicit setup, worker, gather, spill, and
   locality burden
3. preserve rejected-path reasons when serial wins over a legal parallel
   alternative
4. preserve rejected-path reasons when a parallel candidate is absent because
   legality, runtime support, or locality binding is missing

The parity package may not close while donor engines remain faster on a
benchmark-governed process because ScratchBird failed to enumerate or execute a
legal intra-query parallel candidate.
