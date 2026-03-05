# EPFC-039 MySQL COM Family Rollup Evidence

- Timestamp (UTC): `2026-03-04T15:19:58Z`
- Scope rows: `EPFC-040`, `EPFC-041`, `EPFC-042`, `EPFC-043`, `EPFC-044`, `EPFC-045`, `EPFC-049`, `EPFC-051`, `EPFC-053`
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
/home/dcalford/CliWork/ScratchBird/build/tests/scratchbird_tests --gtest_filter='EmulatedParserBoundaryContractsTest.MySqlUnsupportedComRejectContractDeterministic:EmulatedParserBoundaryContractsTest.MySqlProcessKillAndCloneContracts'
```

Result:

```text
[==========] Running 2 tests from 1 test suite.
[ RUN      ] EmulatedParserBoundaryContractsTest.MySqlUnsupportedComRejectContractDeterministic
[       OK ] EmulatedParserBoundaryContractsTest.MySqlUnsupportedComRejectContractDeterministic (0 ms)
[ RUN      ] EmulatedParserBoundaryContractsTest.MySqlProcessKillAndCloneContracts
[       OK ] EmulatedParserBoundaryContractsTest.MySqlProcessKillAndCloneContracts (0 ms)
[==========] 2 tests from 1 test suite ran. (0 ms total)
[  PASSED  ] 2 tests.
```

## Contract assertions exercised

1. Deterministic reject contracts (`errno=1235`, `SQLSTATE=42000`, policy row tag in message):
1.1 `COM_SLEEP` (`EPFC-040`)
1.2 `COM_TIME` (`EPFC-042`)
1.3 `COM_DELAYED_INSERT` (`EPFC-043`)
1.4 `COM_CONNECT` (`EPFC-044`)
1.5 `COM_CONNECT_OUT` (`EPFC-045`)
1.6 `COM_DAEMON` (`EPFC-049`)
1.7 `COM_END` (`EPFC-053`)

2. Implement/simulate contracts:
2.1 `COM_PROCESS_KILL` (`EPFC-041`) non-zero thread-id route to `KILL <thread_id>` query surface and returns OK packet.
2.2 `COM_CLONE` (`EPFC-051`) deterministic simulation success with warning/info payload.

## Promotion recommendation

Promote rows above to `Mitigated` with this artifact and test file as dated evidence.
