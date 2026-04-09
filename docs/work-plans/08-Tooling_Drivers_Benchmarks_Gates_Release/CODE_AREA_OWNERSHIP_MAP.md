# Code Area Ownership Map

## Primary Write Scopes

| Ticket | Primary write scope | Conflict surfaces | Parallelization rule |
| --- | --- | --- | --- |
| B1-08-001 | assigned section specs plus this package | all package control files | serial only |
| B1-08-002 | tests/conformance/public_beta/run_required_public_beta_gate.sh, docs/TEST.md, scripts/run_full_build_test_with_metrics.sh, package audit files | tooling, benchmark, driver, and release gate seams | serial with implementation tickets |
| B1-08-003 | ScratchBird section `30` code roots plus maintained `ScratchBird-driver` release-facing lanes | parser, tooling, driver, and admin-surface overlap | after ownership freeze |
| B1-08-004 | `ScratchBird-Benchmarks`, gate aggregation, artifact manifests, and release certification surfaces | shared benchmark runners, engine launchers, and release docs | after lane A foundation |
| B1-08-005 | final Beta 1 release, benchmark, and evidence gates | same shared runners and release manifests | after implementation tickets |

## Unsafe Parallel Boundaries

- any ticket that updates SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv
- any ticket that changes the same canonical spec file as another ticket
- any ticket that changes the same gate or benchmark artifact family
- any ticket that changes `ScratchBird-Benchmarks/scripts/run-benchmark.sh`
- any ticket that changes `ScratchBird-Benchmarks/scripts/start-engine.sh`
