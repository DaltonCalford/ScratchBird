# EPFC-028 / EPFC-026 bounded PostgreSQL lane update (`drop_if_exists` + `tablespace`)

- Captured UTC: `2026-03-04T20:23:23Z`
- Scope: validate post-route behavior after `SBLR3_ALTER_TABLESPACE` and `SBLR3_DROP_SEQUENCE|DROP_USER|DROP_GROUP` runtime dispatch closure.

## Targeted unit validation

Command:

```bash
ctest --test-dir /home/dcalford/CliWork/ScratchBird/build --output-on-failure -R 'ParserV3IndexManagementTest\.(ParsesDropTablespaceSurface|ParsesAlterTablespaceActions)|SBLRVNextExecutorDispatchContractTest\.(AlterTablespaceOpcodeRoutesWithoutUnknownOpcodeReject|DropSequenceUserGroupOpcodesRouteWithoutUnknownOpcodeReject)|ParserV3NativeExtensionSurfaceTest\.(ParsesPublicationSubscriptionLifecycleSurfaces|ParsesAdminClusterAndServiceControlSurfaces|RejectsRemovedVacuumAndClusterShowAliases)'
```

Result:

1. `7/7` tests passed.
2. Dispatch contract test for `SBLR3_ALTER_TABLESPACE` now validates route behavior (no forced failure expectation).

## Bounded upstream lane execution

### Attempt A (`dbname=regression`)

Run id: `20260304_152148_epfc028_pgext`  
Result: failed before semantic comparison due endpoint binding policy.

Observed in lane output:

1. `psql: ... FATAL: Database switch denied by manager binding context` for both tests.

### Attempt B (`dbname=main`)

Run id: `20260304_152225_epfc028_pgext_main`  
Command lane: `pg_regress ... drop_if_exists tablespace`  
Result: `0 passed / 2 failed` (expected upstream diff artifacts produced).

## Key routing observation

Across bounded upstream outputs for attempt B:

1. No `IRX_0403` occurrences.
2. No `BRG_0406` occurrences.
3. `DROP SEQUENCE`, `DROP USER`, `DROP GROUP`, `DROP TABLESPACE`, and `ALTER TABLESPACE` statements execute through semantic handlers and return domain-specific outcomes instead of unknown-opcode rejects.

## Remaining parity blockers from bounded lane

1. `drop_if_exists`: PostgreSQL notice/error text parity gaps remain (`IF EXISTS` notice surfaces and message shapes differ).
2. `tablespace`: broader catalog/result-shape gaps remain (`pg_class`/`pg_tablespace` introspection and related semantics).

## Artifacts

1. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_152148_epfc028_pgext/upstream/pg_regress.out`
2. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_152225_epfc028_pgext_main/upstream/pg_regress.out`
3. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_152225_epfc028_pgext_main/upstream/regression.out`
4. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_152225_epfc028_pgext_main/upstream/regression.diffs`
5. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_152225_epfc028_pgext_main/upstream/results/drop_if_exists.out`
6. `ScratchBird/tests/compatibility/postgresql/results/ctest/20260304_152225_epfc028_pgext_main/upstream/results/tablespace.out`

## Tracker recommendation

1. Keep `EPFC-028` as `InProgress` (dispatch-path blocker cleared; semantic parity remains).
2. Keep `EPFC-026` as `InProgress` and continue bounded-to-broader upstream parity burn-down.
