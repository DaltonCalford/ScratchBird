# EPFC-037 PostgreSQL Deferred Provider Matrix Evidence

- Timestamp (UTC): `2026-03-04T15:28:47Z`
- Row: `EPFC-037`
- Linked rows: `PG-EMU-021`, `PG-EMU-022`, `PG-EMU-023`
- Runtime surface: `ScratchBird/src/protocol/adapters/postgresql_adapter.cpp`
- Test surface: `ScratchBird/tests/unit/test_protocol_adapter_dialects.cpp`

## Build evidence

Command:

```bash
cmake --build /home/dcalford/CliWork/ScratchBird/build --target scratchbird_tests -j8
```

Result:

```text
[100%] Built target scratchbird_tests
```

## Targeted policy tests

Command:

```bash
/home/dcalford/CliWork/ScratchBird/build/tests/scratchbird_tests --gtest_filter='ProtocolAdapterDialectsC1.PostgreSQLPolicyRejectsUnsupportedConfiguredAuthMethodAtStartup:ProtocolAdapterDialectsC1.PostgreSQLPolicyRejectsPeerConfiguredMethodAtStartup:ProtocolAdapterDialectsC1.PostgreSQLPolicyRejectsUnsupportedSaslMechanismDeterministically:ProtocolAdapterDialectsC1.PostgreSQLPolicyRejectsScramPlusChannelBindingDeterministically:ProtocolAdapterDialectsC1.PostgreSQLPolicyGssEncNegotiatedDisableIsDeterministic'
```

Result:

```text
[==========] Running 5 tests from 1 test suite.
[ RUN      ] ProtocolAdapterDialectsC1.PostgreSQLPolicyRejectsUnsupportedConfiguredAuthMethodAtStartup
[       OK ] ProtocolAdapterDialectsC1.PostgreSQLPolicyRejectsUnsupportedConfiguredAuthMethodAtStartup (0 ms)
[ RUN      ] ProtocolAdapterDialectsC1.PostgreSQLPolicyRejectsPeerConfiguredMethodAtStartup
[       OK ] ProtocolAdapterDialectsC1.PostgreSQLPolicyRejectsPeerConfiguredMethodAtStartup (0 ms)
[ RUN      ] ProtocolAdapterDialectsC1.PostgreSQLPolicyRejectsUnsupportedSaslMechanismDeterministically
[       OK ] ProtocolAdapterDialectsC1.PostgreSQLPolicyRejectsUnsupportedSaslMechanismDeterministically (0 ms)
[ RUN      ] ProtocolAdapterDialectsC1.PostgreSQLPolicyRejectsScramPlusChannelBindingDeterministically
[       OK ] ProtocolAdapterDialectsC1.PostgreSQLPolicyRejectsScramPlusChannelBindingDeterministically (0 ms)
[ RUN      ] ProtocolAdapterDialectsC1.PostgreSQLPolicyGssEncNegotiatedDisableIsDeterministic
[       OK ] ProtocolAdapterDialectsC1.PostgreSQLPolicyGssEncNegotiatedDisableIsDeterministic (0 ms)
[==========] 5 tests from 1 test suite ran. (0 ms total)
[  PASSED  ] 5 tests.
```

## Deterministic policy coverage

1. Startup policy reject for unsupported configured method -> SQLSTATE `0A000`.
2. Startup policy reject for configured `PEER` method -> SQLSTATE `0A000`.
3. SASL initial with unsupported mechanism (`OAUTHBEARER`) -> SQLSTATE `0A000` deterministic reject.
4. SCRAM-PLUS channel binding attempt -> SQLSTATE `0A000` deterministic reject.
5. GSSENC request receives deterministic negotiated disable (`'N'`) and startup/auth flow continues.

## EPFC-037 recommendation

Promote `EPFC-037` to `Mitigated` with this artifact and updated policy tests as dated evidence of deferred-provider deterministic negotiation behavior.
