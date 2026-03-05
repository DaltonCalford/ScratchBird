# EPFC-028 / EPFC-026 bounded PostgreSQL lane re-run (`drop_if_exists` + `tablespace`) after parity patch attempt

- Captured UTC: `2026-03-04T20:47:57Z`
- Scope: implement highest-frequency PostgreSQL `drop_if_exists` parity deltas and rerun bounded upstream lane.

## Implementation increments

1. `ScratchBird/src/sblr/executor.cpp`
- Added `isPostgreSqlDialectContext(...)` helper to classify PostgreSQL context from:
  - `conn_ctx_->dialect_tag()` (`POSTGRESQL|POSTGRES|PG`), or
  - active v3 container module override (`v3_module_dialect`).
- Added scoped v3 module-dialect override in `executeCanonicalV3(...)` so legacy handlers invoked from v3 PG containers can consume PG context.
- Wired PG context helper into:
  - `executeDropSequence()`
  - `executeDropUser()`
  - `executeDropRole()`
  - `executeDropGroup()`
  - v3 `SBLR3_DROP_TABLESPACE` branch

2. `ScratchBird/src/protocol/adapters/protocol_adapter.cpp`
- Added `DialectTagExecutionScope` in `ProtocolAdapter::executeBytecode(...)`:
  - when session `dialect_tag` is empty/`scratchbird`, temporarily applies protocol dialect tag during executor dispatch,
  - restores original session tag afterward.

## Validation

### Targeted unit lane (same 7-test subset)

Command:

```bash
ctest --test-dir /home/dcalford/CliWork/ScratchBird/build --output-on-failure -R 'ParserV3IndexManagementTest\.(ParsesDropTablespaceSurface|ParsesAlterTablespaceActions)|SBLRVNextExecutorDispatchContractTest\.(AlterTablespaceOpcodeRoutesWithoutUnknownOpcodeReject|DropSequenceUserGroupOpcodesRouteWithoutUnknownOpcodeReject)|ParserV3NativeExtensionSurfaceTest\.(ParsesPublicationSubscriptionLifecycleSurfaces|ParsesAdminClusterAndServiceControlSurfaces|RejectsRemovedVacuumAndClusterShowAliases)'
```

Result:

1. `7/7` passed (revalidated after each build).

### Bounded upstream lane (`drop_if_exists tablespace`, `dbname=main`)

Command:

```bash
SCRATCHBIRD_PG_HOST=127.0.0.1 \
SCRATCHBIRD_PG_PORT=16432 \
SCRATCHBIRD_PG_DB=main \
SCRATCHBIRD_PG_USER=pg_admin \
SCRATCHBIRD_PG_USE_UPSTREAM=1 \
SCRATCHBIRD_PG_REGRESS_TESTS='drop_if_exists tablespace' \
tests/compatibility/postgresql/scripts/run_postgresql_ctest.sh
```

Runs:

1. `20260304_154357` (post executor helper + v3 override patch): `0/2` passed.
2. `20260304_154608` (post protocol adapter execution-scope patch): `0/2` passed.
3. `20260304_154718` (after bounded pre-clean of fixture objects): `0/2` passed.

Observed in all reruns:

1. No `IRX_0403` occurrences.
2. No `BRG_0406` occurrences.
3. PostgreSQL notice/error text for `DROP USER|ROLE|GROUP|SEQUENCE IF EXISTS` remained non-parity (`User '...' not found`, etc.).

## Additional diagnostic observation

1. PostgreSQL-wire session still reports:
- `SHOW dialect_tag;` -> `scratchbird`
- `SHOW parser;` -> `scratchbird`

2. Fixture-state instability persists in bounded lane:
- `DROP TABLE IF EXISTS test_exists;` did not reliably remove prior object state, causing subsequent `CREATE TABLE test_exists` and `CREATE INDEX test_index_exists` to hit `already exists` in later reruns.

## Status

1. `EPFC-028` remains `InProgress`.
2. Dispatch routing closure remains intact (`IRX_0403`/`BRG_0406` absent), but PostgreSQL notice/error parity closure for high-frequency `drop_if_exists` deltas was not achieved in this increment.
3. Bounded lane now also indicates a persistent drop-table cleanup semantic gap that must be resolved to keep reruns deterministic.

## Artifacts

1. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_154357/upstream/pg_regress.out`
2. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_154357/upstream/regression.diffs`
3. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_154608/upstream/pg_regress.out`
4. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_154608/upstream/regression.diffs`
5. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_154718/upstream/pg_regress.out`
6. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_154718/upstream/regression.diffs`
7. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_154718/upstream/results/drop_if_exists.out`
8. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_154718/upstream/results/tablespace.out`
