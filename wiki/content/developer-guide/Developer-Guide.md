# ScratchBird Developer Guide (Beta 0.1.0)

## 1. Development Scope

This guide covers the implementation baseline that is actually built and tested
in the current workspace.

## 2. Runtime Architecture

### 2.1 Process Roles

- `sb_server`: orchestration and service controller
- `sb_listener_*`: protocol listener entrypoints
- `sb_parser_*`: protocol parser agent entrypoints
- engine runtime: SBLR execution and core services

### 2.2 Ownership and Routing

- Listener is owned by a database context during bootstrap/runtime.
- Startup checks port collisions before launch.
- Listener receives database-owner + engine endpoint data for parser/engine
  routing.

Implementation/test references:

- `src/server/service_controller.cpp`
- `tests/unit/test_service_controller_listener_bootstrap.cpp`

## 3. Parser and Execution Pipeline

1. SQL text input
2. Parser -> AST
3. AST -> SBLR bytecode container
4. Executor dispatch and core engine interaction

Key files:

- `src/parser/parser_v3.cpp`
- `src/sblr/query_compiler_v3.*`
- `src/sblr/executor.cpp`

## 4. Module Map

- `src/core/`: storage, catalog, transaction, lock, GC
- `src/network/`: listener runtime, sockets, control plane
- `src/server/`: service controller, bootstrap/orchestration
- `src/parser/`: parser implementations and AST
- `src/sblr/`: bytecode schema, compiler, executor
- `tests/`: unit/integration/perf/stress suites

## 5. Build and Test Commands

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Baseline expectation: `3355/3355` passing tests.

## 6. Release Packaging (Current)

- Runtime binaries: `release/beta/bin`
- Test binaries: `release/beta/tests`
- Split packages:
  - `release/beta/packages/runtime-only`
  - `release/beta/packages/qa`

## 7. Active Engineering Priorities (0.2.0)

- Catalog refactor/optimization.
- Emulation parser completion and parity validation.
- Native parser normalization and deterministic style consistency.
- Driver compatibility revalidation post-refactor.
- Cross-engine performance benchmarking and decision gates.
- Installer bundle strategy implementation.

## 8. Spec and Planning Sources

Active planning/audit artifacts:

- `../../../docs/audit/BETA_0_1_0_IMPLEMENTATION_AUDIT_2026-02-19.md`
- `../../../docs/planning/BETA_0_2_0_WORKPLAN_2026-02-19.md`
- `../../../docs/planning/BETA_0_2_0_SPEC_BACKLOG_2026-02-19.md`
