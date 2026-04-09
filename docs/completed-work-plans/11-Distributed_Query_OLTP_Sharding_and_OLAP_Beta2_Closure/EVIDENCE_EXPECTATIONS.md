# Evidence Expectations

For each research ticket, the evidence set must include all of the following.

## Required packet structure

For each research ticket create:

- `docs/work-plans/11-Distributed_Query_OLTP_Sharding_and_OLAP_Beta2_Closure/evidence/<ticket>/README.md`
- a local-source findings report
- a downloaded web-source manifest
- any downloaded technical specs under
  `docs/reference/workspace_library/technical_specs/<topic>/`
- any downloaded whitepapers under
  `docs/reference/workspace_library/whitepapers/<topic>/`
- any downloaded open-source implementation references under
  `docs/reference/workspace_library/third_party_implementations/<topic>/`
- a consolidated research packet under `docs/reference/reference_library/`
  when the topic benefits from a reusable packet

## Required research content

1. Current ScratchBird truth
   - current canonical boundaries
   - current local code evidence where relevant
   - known exclusions and fail-closed boundaries
2. Commercial and donor implementations
   - official vendor docs
   - standards docs if any
   - open-source implementations where useful
   - whitepapers and algorithm papers
3. Decision synthesis
   - best-fit design options
   - current-architecture-compatible choice
   - why non-selected approaches were refused
4. Implementation guidance
   - process flow
   - state machine
   - metadata shapes
   - algorithms
   - failure or refusal classes
   - observability and certification obligations

## Low-reasoning-agent standard

Each final Beta 2 canonical spec must be detailed enough that a low-reasoning
implementation agent can:

- identify all required catalogs, state rows, markers, or metadata
- implement the runtime flow without inventing missing states
- know exactly when to refuse, rebalance, quarantine, or replay
- know which background tasks, metrics, and gates are required
- avoid confusing donor inspiration with ScratchBird truth

## MGA compliance rule

Every research synthesis must explicitly state:

- how the chosen design preserves MGA truth
- whether any exchange log, shard commit log, or replay lane is derivative only
- what would be refused rather than approximated
