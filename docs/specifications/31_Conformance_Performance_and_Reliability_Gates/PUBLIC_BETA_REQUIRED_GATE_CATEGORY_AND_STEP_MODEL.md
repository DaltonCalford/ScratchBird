# Public Beta Required Gate Category and Step Model

## Purpose

Define the current public-beta hard gate script as a bounded certification surface built from exact CTest and script steps.

## Canonical Entry Point

The current required gate runner is:

- `tests/conformance/public_beta/run_required_public_beta_gate.sh`

## Category Model

The runner tracks pass and fail counts by category.

Current categories are:

- `wire_protocol`
- `transaction_semantics`
- `security_enforcement`
- `end_to_end_sql`
- `modal_nosql`
- `cluster_infra`

## Step Execution Model

Each step is executed by:

- exact CTest regex match
- or a script step with log capture and retry handling

Each step produces:

- category
- step id
- pass or fail result
- log path

in the gate result ledger.

## Shared Fixture Handling

Some script steps depend on a shared dynamic example fixture. The runner supports:

- initial fixture refresh
- per-script-step refresh
- transport-failure-triggered repair refresh

This is part of the gate contract because a stale fixture can invalidate otherwise healthy compatibility steps.

## Gate Evidence Classes

The current runner intentionally certifies:

- protocol framing and compatibility
- end-to-end native SQL paths
- MGA and restart semantics
- memory-model proof surfaces
- security enforcement
- modal or NoSQL parser surfaces
- cluster and replication surfaces

## Bounded Authority

The gate script explicitly states that it does not by itself certify:

- universal WAL-style recovery maturity
- full mature optimizer completeness
- universal metadata and DDL maturity

This limitation is part of the canonical reading of the gate.

## Required Artifacts

The public-beta gate writes:

- per-step logs
- `step_results.txt`
- category pass or fail tallies
- a timestamped result directory

## Interpretation Rule

The public-beta gate is a required bounded evidence pack, not a claim that the entire repository test tree has run. It is a curated release gate.
