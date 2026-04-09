# Evidence Expectations

For each research ticket, the evidence set must include all of the following.

## Required Packet Structure

For each research ticket `CPG-10-00N` create:

- `docs/work-plans/10-Commercial_Parity_Missing_Element_Research_and_Beta2_Closure/evidence/CPG-10-00N/README.md`
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

## Required Research Content

1. Current ScratchBird truth
   - current canonical boundaries
   - current local code evidence where relevant
   - known exclusions and fail-closed boundaries
2. Donor and commercial implementations
   - official vendor docs
   - standards docs if any
   - open-source implementations where useful
   - whitepapers and algorithm papers
3. Decision synthesis
   - best-fit design options
   - MGA-compatible choice
   - why non-selected donor approaches were refused
4. Implementation guidance
   - process flow
   - state machine
   - metadata shapes
   - algorithms
   - failure/refusal classes
   - observability and certification obligations

## Low-Reasoning-Agent Standard

Each final Beta 2 canonical spec must be detailed enough that a low-reasoning
implementation agent can:

- identify all required catalogs, state tables, markers, or metadata
- implement the runtime flow without inventing missing states
- know exactly when to refuse or quarantine
- know which background tasks and metrics are required
- avoid confusing donor inspiration with ScratchBird truth

## MGA Compliance Rule

Every research synthesis must explicitly state:

- how the chosen design preserves MGA truth
- whether any WAL, redo, log-shipping, or donor-journal element is optional
  derivative support only
- what would be refused rather than approximated
