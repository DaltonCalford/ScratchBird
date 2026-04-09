# Evidence Expectations

For each research ticket, the evidence set must include all of the following.

## Required Packet Structure

For each research ticket `NEQ-12-00N` create:

- `docs/work-plans/12-ScratchBird_Native_Equivalent_Feature_Closure/evidence/NEQ-12-00N/README.md`
- `CURRENT_SCRATCHBIRD_TRUTH.md`
- `REFERENCE_MANIFEST.csv`
- `OPEN_SOURCE_AND_STANDARD_SOURCES.md`
- `DESIGN_OPTION_COMPARISON.md`
- `IMPLEMENTATION_NOTEBOOK.md`
- `EXAMPLES_AND_TEST_VECTORS.md`
- `PROCESS_FLOW_AND_STATE_MACHINE.md`

If the topic benefits from reuse across future work, also create a reusable
packet under `docs/reference/reference_library/`.

## Required Download Rules

All downloaded external sources must be placed under the canonical roots:

- `docs/reference/workspace_library/technical_specs/<topic>/`
- `docs/reference/workspace_library/whitepapers/<topic>/`
- `docs/reference/workspace_library/third_party_implementations/<topic>/`

Every evidence packet must reference the local downloaded copy, not only the
remote URL.

## Required Research Content

1. Current ScratchBird truth
   - current canonical boundaries
   - current implementation proof where present
   - known exclusions and fail-closed boundaries
2. External references
   - standards documentation
   - official project documentation
   - whitepapers and algorithm papers
   - high-quality open-source implementations
3. Decision synthesis
   - at least two design options when alternatives exist
   - selected design and why it fits ScratchBird
   - why non-selected donor designs were refused
4. Implementation guidance
   - process flow
   - state machine
   - metadata and catalog shapes
   - algorithms or pseudocode
   - background workers
   - refusal and quarantine classes
   - observability and certification requirements
5. Examples
   - DDL examples where applicable
   - SQL or UDR usage examples
   - failure examples
   - recovery or operator workflow examples

## Required Spec-Ticket Deliverables

Every spec ticket must produce or expand a canonical spec with all of:

- purpose
- governing rules
- explicit non-goals
- data model and catalog objects
- process flow
- state machines
- algorithms or deterministic lowering rules
- background workers and maintenance tasks
- refusal rules and error classes
- metrics and observability
- examples
- cross-section requirements
- implementation notes for low-reasoning agents

## Low-Reasoning-Agent Standard

Each final canonical spec must be detailed enough that a low-capability,
low-reasoning implementation agent can:

- identify every required catalog row, metadata object, and worker
- implement the runtime flow without inventing states
- know exactly when to refuse, retry, quarantine, or degrade
- know which example cases to test first
- distinguish donor inspiration from ScratchBird truth

## MGA Compliance Rule

Every research synthesis and every final spec must state:

- how the design preserves MGA truth
- whether any log, journal, or queue is derivative rather than authoritative
- which donor-inspired behaviors are intentionally refused

## Examples Rule

Every ticket must gather or produce concrete examples:

- at least `3` positive-path examples
- at least `2` refusal or failure-path examples
- at least `1` operator workflow example

Examples must be local to the evidence packet and must not require outside
guessing to understand the intended behavior.
