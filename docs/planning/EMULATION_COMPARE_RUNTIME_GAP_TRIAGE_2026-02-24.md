# Emulation Compare Runtime Gap Triage (2026-02-24)

Last updated: 2026-02-24
Scope: `ENG-FIREBIRD-003`, `ENG-MYSQL-003`, `ENG-POSTGRESQL-003` compare lanes.

## 1) Evidence snapshot

Current compare gate command logs:
- `artifacts/baseline/firebird/p6s1w3/eng-firebird-003-compare/command_log.txt`
- `artifacts/baseline/mysql/p6s1w3/eng-mysql-003-compare/command_log.txt`
- `artifacts/baseline/postgresql/p6s1w3/eng-postgresql-003-compare/command_log.txt`

## 2) Gap classification

### ENG-FIREBIRD-003
- Status: blocked
- Class: client capability / toolchain
- Observed behavior:
  - Compare command fails immediately with explicit guard:
    `generic sb_isql fallback is not valid for Firebird emulation compare runs`.
- Concrete gap:
  - `sb_fb_isql` wrapper is not available; generic native client cannot provide Firebird wire parity.
  - SQL workload is intentionally not started to avoid false protocol evidence.

### ENG-MYSQL-003
- Status: blocked
- Class: client capability / toolchain
- Observed behavior:
  - Compare command fails immediately with explicit guard:
    `generic sb_isql fallback is not valid for MySQL emulation compare runs`.
- Concrete gap:
  - `sb_my_isql` wrapper is not available; generic native client cannot provide MySQL wire parity.
  - SQL workload is intentionally not started to avoid false protocol evidence.

### ENG-POSTGRESQL-003
- Status: blocked
- Class: client capability / toolchain
- Observed behavior:
  - Compare command fails immediately with explicit guard:
    `generic sb_isql fallback is not valid for PostgreSQL emulation compare runs`.
- Concrete gap:
  - `sb_pg_isql` wrapper is not available; generic native client cannot provide PostgreSQL wire parity.
  - SQL workload is intentionally not started to avoid false protocol evidence.

## 3) Cross-lane runtime blockers

1. Wrapper client availability gap:
   - `ScratchBird-driver` FDW wrappers are not currently buildable in this tree:
     - `sb_my_isql`: unresolved `scratchbird::fdw::MySQLAdapter::*`
   - No viable `sb_pg_isql` / `sb_fb_isql` compare client is currently available in the active build output.

2. Generic native fallback is intentionally blocked:
   - Compare scripts now fail fast when only generic `sb_isql` is available.
   - This prevents invalid native-protocol runs from being misclassified as emulation parity evidence.

3. Parser signal is currently blocked by client availability:
   - For all three lanes, SQL execution is not attempted until protocol-correct wrappers are available.
   - Parser/AST/executor parity cannot be concluded from current compare runs.

## 4) Parser gap status (current)

- Confirmed parser gap count from compare lanes: `0` (not reachable yet).
- Unknown parser gap count: `all` compare workload statements in the curated lists remain unexecuted due connection/runtime gating.

## 5) Targeted next actions

1. Restore protocol-correct compare clients:
   - Build/provide `sb_fb_isql`, `sb_my_isql`, and `sb_pg_isql` (or equivalent protocol-correct wrappers).
   - For this tree, unblock FDW wrapper link errors first.

2. Keep generic fallback blocked for compare lanes:
   - Maintain explicit fail-fast guard to prevent false native-protocol evidence.

3. Re-run `ENG-FIREBIRD-003`, `ENG-MYSQL-003`, `ENG-POSTGRESQL-003` once wrappers are available:
   - Promote first SQL parse/execute failures into parser gap inventory.
   - Separate parser gaps from client/toolchain gaps in evidence bundles.
