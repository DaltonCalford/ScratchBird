# EPFC-039 MySQL COM Family Rollup Evidence

- Timestamp (UTC): `2026-03-04T15:23:06Z`
- Scope rows: `EPFC-040..EPFC-053`
- Runtime surface: `ScratchBird/src/ipc/external_agents/mysql_parser_agent.cpp`
- Test surface: `ScratchBird/tests/unit/test_emulated_parser_boundary_contracts.cpp`

## Build evidence

Command:

```bash
cmake --build /home/dcalford/CliWork/ScratchBird/build --target scratchbird_tests -j8
```

Result:

```text
[100%] Built target scratchbird_tests
```

## Targeted test evidence

Command:

```bash
/home/dcalford/CliWork/ScratchBird/build/tests/scratchbird_tests --gtest_filter='EmulatedParserBoundaryContractsTest.MySqlUnsupportedComRejectContractDeterministic:EmulatedParserBoundaryContractsTest.MySqlProcessKillAndCloneContracts:EmulatedParserBoundaryContractsTest.MySqlDeferredComRejectContractDeterministic'
```

Result:

```text
[==========] Running 3 tests from 1 test suite.
[ RUN      ] EmulatedParserBoundaryContractsTest.MySqlUnsupportedComRejectContractDeterministic
[       OK ] EmulatedParserBoundaryContractsTest.MySqlUnsupportedComRejectContractDeterministic (0 ms)
[ RUN      ] EmulatedParserBoundaryContractsTest.MySqlProcessKillAndCloneContracts
[       OK ] EmulatedParserBoundaryContractsTest.MySqlProcessKillAndCloneContracts (0 ms)
[ RUN      ] EmulatedParserBoundaryContractsTest.MySqlDeferredComRejectContractDeterministic
[       OK ] EmulatedParserBoundaryContractsTest.MySqlDeferredComRejectContractDeterministic (0 ms)
[==========] 3 tests from 1 test suite ran. (0 ms total)
[  PASSED  ] 3 tests.
```

## Row closure mapping

1. `Mitigated` evidence rows:
1.1 `EPFC-040`, `EPFC-041`, `EPFC-042`, `EPFC-043`, `EPFC-044`, `EPFC-045`, `EPFC-049`, `EPFC-051`, `EPFC-053`

2. `Deferred` evidence rows (`defer_core_dependency` with deterministic reject contract):
2.1 `EPFC-046` (`COM_REGISTER_SLAVE`)
2.2 `EPFC-047` (`COM_BINLOG_DUMP`)
2.3 `EPFC-048` (`COM_TABLE_DUMP`)
2.4 `EPFC-050` (`COM_BINLOG_DUMP_GTID`)
2.5 `EPFC-052` (`COM_SUBSCRIBE_GROUP_REPLICATION_STREAM`)

## Defer linkage contract

Deferred rows above are linked to core dependency umbrella `EPFC-028` until replication channel/log-stream/group-stream runtime is implemented.

## Umbrella promotion recommendation

`EPFC-039` is eligible for `Mitigated` promotion because each child row in `EPFC-040..EPFC-053` is now either `Mitigated` or `Deferred` with dated evidence and defer linkage.
