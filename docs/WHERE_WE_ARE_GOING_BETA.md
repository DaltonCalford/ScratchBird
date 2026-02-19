# Beta 0.2.0 Direction

- Current baseline: `0.1.0` (initial early beta)
- Target baseline: `0.2.0`

## Objective

Deliver a beta hardening cycle that closes partial features, normalizes parser
behavior, validates driver compatibility after refactors, and makes
performance-based go/no-go decisions using repeatable benchmarks.

## Required 0.2.0 Work Items

### (a) Complete specs and implementation plans for all partial/planned items

- Deliver full specs (scope, invariants, contracts, acceptance tests).
- Deliver implementation plans with dependency ordering and verification gates.

### (b) Catalog refactor and optimization

- Refactor catalog internals for maintainability and performance.
- Add migration/recovery safety checks and regression tests.

### (c) Finish emulation parser parity

- Complete parser behavior for emulated engines.
- Run source-engine conformance suites where available.
- Document pass/fail deltas and remediation plans.

### (d) Native parser renormalization

- Normalize grammar/style behavior for deterministic dialect consistency.
- Re-run parser + executor regression suites.

### (e) Driver test confirmation after refactor/normalization

- Run driver test matrix against updated engine behavior.
- Record compatibility deltas and fixes.

### (f) Cross-engine speed/performance testing

- Benchmark against emulated source engines on identical hardware/OS.
- Include workload classes: OLTP mix, analytical scans, indexing-heavy flows.

### (g) Go/no-go and redesign gates

- Define acceptance thresholds per workload family.
- If thresholds fail, execute redesign/retuning workstream before release gate.

### (h) Installation bundles vs release packages

- Decide packaging strategy for installers vs archive-only distribution.
- Implement and validate selected bundle paths.

## Execution Artifacts

Canonical plan/audit documents:

- `docs/audit/BETA_0_1_0_IMPLEMENTATION_AUDIT_2026-02-19.md`
- `docs/planning/BETA_0_2_0_WORKPLAN_2026-02-19.md`
- `docs/planning/BETA_0_2_0_SPEC_BACKLOG_2026-02-19.md`
