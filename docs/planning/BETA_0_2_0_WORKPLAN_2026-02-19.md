# Beta 0.2.0 Workplan

- Plan date: `2026-02-19`
- Input baseline: `0.1.0` implementation audit

## 1. Goal

Move from initial early beta (`0.1.0`) to hardened beta (`0.2.0`) by closing
all partial/planned tracks and enforcing performance-based release gates.

## 2. Required Tracks

### Track A: Partial/Planned Spec Completion

- Produce complete specifications and implementation plans for every
  partial/planned item.
- Deliver acceptance contracts and dependency maps.

### Track B: Catalog Refactor and Optimization

- Refactor catalog internals for consistency and maintainability.
- Add migration and rollback safety gates.
- Add catalog performance benchmarks.

### Track C: Emulation Parser Parity Completion

- Complete parser behavior for each emulated engine profile.
- Integrate conformance suite runners per source engine where available.
- Record deltas and remediations as tracked defects.

### Track D: Native Parser Renormalization

- Normalize parser style/shape and deterministic behavior.
- Gate on parser contract suite and SQL-to-SBLR determinism tests.

### Track E: Driver Revalidation

- Re-run driver compatibility suites after Tracks B and D.
- Block release on unresolved regression deltas.

### Track F: Performance Benchmark Program

- Run comparative benchmark suites against source engines.
- Use identical hardware/OS and controlled data/workload sets.

### Track G: Go/No-Go and Redesign Gates

- Define explicit thresholds per workload class.
- Trigger redesign workstream for any failed release-critical threshold.

### Track H: Packaging Strategy

- Decide installer bundles vs release-package-only strategy.
- Implement and validate selected distribution path(s).

## 3. Phase Schedule

### Phase 1 - Planning and Spec Closure

- Close Track A documentation deliverables.
- Freeze acceptance contracts for Tracks B-H.

### Phase 2 - Core Refactor and Parser Normalization

- Execute Tracks B and D.
- Run core regression gates.

### Phase 3 - Emulation and Driver Parity

- Execute Tracks C and E.
- Run conformance and driver suites.

### Phase 4 - Performance and Decision Gates

- Execute Tracks F and G.
- Produce release gate decision package.

### Phase 5 - Distribution Path

- Execute Track H and publish packaging docs.

## 4. Exit Criteria for 0.2.0

1. All tracks A-H closed or explicitly deferred with signed acceptance.
2. No unresolved release-critical regressions.
3. Performance go/no-go gate passes or redesign accepted and implemented.
4. Packaging strategy implemented and documented.
