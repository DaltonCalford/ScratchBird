# EPFC-038 Firebird Deferred Provider Matrix Evidence

- Timestamp (UTC): `2026-03-04T15:33:04Z`
- Row: `EPFC-038`
- Linked rows: `FB-EMU-022`, `FB-EMU-023`
- Runtime surface: `ScratchBird/src/protocol/adapters/firebird_adapter.cpp`
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
/home/dcalford/CliWork/ScratchBird/build/tests/scratchbird_tests --gtest_filter='ProtocolAdapterDialectsFirebird.FirebirdPolicyRejectsUnsupportedConfiguredAuthMethodAtConnect:ProtocolAdapterDialectsFirebird.FirebirdPolicyAllowsPasswordConfiguredMethodAtConnect:ProtocolAdapterDialectsFirebird.FirebirdPolicyAllowsScram512ConfiguredMethodAtConnect:ProtocolAdapterDialectsFirebird.FirebirdPolicyRejectsLegacyAuthPluginDeterministically:ProtocolAdapterDialectsFirebird.FirebirdPolicyRejectsWinSspiPluginDeterministically'
```

Result:

```text
[==========] Running 5 tests from 1 test suite.
[ RUN      ] ProtocolAdapterDialectsFirebird.FirebirdPolicyRejectsUnsupportedConfiguredAuthMethodAtConnect
[       OK ] ProtocolAdapterDialectsFirebird.FirebirdPolicyRejectsUnsupportedConfiguredAuthMethodAtConnect (0 ms)
[ RUN      ] ProtocolAdapterDialectsFirebird.FirebirdPolicyAllowsPasswordConfiguredMethodAtConnect
[       OK ] ProtocolAdapterDialectsFirebird.FirebirdPolicyAllowsPasswordConfiguredMethodAtConnect (0 ms)
[ RUN      ] ProtocolAdapterDialectsFirebird.FirebirdPolicyAllowsScram512ConfiguredMethodAtConnect
[       OK ] ProtocolAdapterDialectsFirebird.FirebirdPolicyAllowsScram512ConfiguredMethodAtConnect (0 ms)
[ RUN      ] ProtocolAdapterDialectsFirebird.FirebirdPolicyRejectsLegacyAuthPluginDeterministically
[       OK ] ProtocolAdapterDialectsFirebird.FirebirdPolicyRejectsLegacyAuthPluginDeterministically (0 ms)
[ RUN      ] ProtocolAdapterDialectsFirebird.FirebirdPolicyRejectsWinSspiPluginDeterministically
[       OK ] ProtocolAdapterDialectsFirebird.FirebirdPolicyRejectsWinSspiPluginDeterministically (0 ms)
[==========] 5 tests from 1 test suite ran. (0 ms total)
[  PASSED  ] 5 tests.
```

## Deterministic policy coverage

1. Unsupported configured auth method at connect emits deterministic Firebird error response.
2. Supported configured methods (`PASSWORD`, `SCRAM_SHA_512`) continue via `op_accept_data` auth negotiation.
3. Deferred provider/plugin `Legacy_Auth` rejects deterministically with `isc_login` and SQLSTATE `0A000`.
4. Deferred provider/plugin `Win_Sspi` rejects deterministically with `isc_login` and SQLSTATE `0A000`.

## EPFC-038 recommendation

Promote `EPFC-038` to `Mitigated` with this artifact and targeted Firebird policy tests as dated evidence.
