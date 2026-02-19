# ScratchBird Developer Guide (Beta 0.1.0)

## 1. Scope

This guide covers the current implementation baseline for engine developers.
It is intentionally implementation-backed and excludes speculative scope.

## 2. Architecture Model

### 2.1 Layering

1. Client protocol entry
2. Listener process
3. Parser agent process
4. Engine endpoint (SBLR execution)
5. Core storage/catalog/transaction subsystems

### 2.2 Critical Boundaries

- Engine executes SBLR; parser produces SBLR.
- Dialect-specific parsing lives in parser agents, not in core execution.
- Listener is database-owned during bootstrap and runtime routing.

## 3. Code Map

- Server orchestration: `src/server/`
- Listener runtime: `src/network/sb_listener_main.cpp`
- Parser implementation: `src/parser/`
- SBLR executor/runtime: `src/sblr/`
- Core engine services: `src/core/`
- Public interfaces: `include/scratchbird/`
- Unit and integration tests: `tests/`

## 4. Listener Ownership and Collision Rules

Current behavior in 0.1.0:

- Before listener launch, server performs a bind feasibility check.
- If bind would collide, listener bootstrap is skipped safely.
- Listener startup receives explicit database-owner context and engine endpoint.
- Multiple listeners can run with distinct owners/ports.

Validation tests:

- `tests/unit/test_service_controller_listener_bootstrap.cpp`

## 5. Build, Test, and Release Workflow

### 5.1 Build

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

### 5.2 Test

```bash
ctest --test-dir build --output-on-failure
```

Expected 0.1.0 baseline: `3390` tests, all passing.

### 5.3 Release Packaging

- Runtime artifacts: `release/beta/bin`
- QA artifacts: `release/beta/tests`
- Package split:
  - `release/beta/packages/runtime-only`
  - `release/beta/packages/qa`

## 6. Extension Pattern: UDR SQL Render Endpoint

Added in this baseline:

- API contract header:
  `include/scratchbird/udr/language_udr_sql_render_endpoint.h`
- Runtime implementation:
  `src/sblr/language_udr_sql_render_endpoint.cpp`
- Contract tests:
  `tests/unit/test_language_udr_sblr_sql_render_endpoint_contract.cpp`

Design intent:

- deterministic request validation
- deterministic vnext error codes
- stable SQL rendering contract per opcode/profile

## 7. Engineering Rules for 0.2.0 Cycle

- Keep parser/engine boundaries explicit.
- Any catalog refactor must ship with migration and regression gates.
- Parser normalization changes require deterministic acceptance tests.
- Driver compatibility must be revalidated after parser/catalog changes.
- Performance decisions are benchmark-driven, not estimate-driven.

## 8. Where to Put New Specs and Plans

Use in-tree planning and audit tracks:

- `docs/audit/`
- `docs/planning/`
