# v3 Native Inet Parser Conformance

This suite validates the canonical `v3` parser over the native listener TCP path.

## Scope

- Transport path: `sb_isql --mode=local-ipc --ipc-method=tcp` (inet listener)
- Parser mode: native `v3` core parser
- Validation style:
  - Positive scripts emit deterministic `ASSERT|...` lines and are compared exactly
  - Negative scripts must fail and match expected parser-error substrings

## Why this suite exists

- Ensures parser behavior is validated through the listener path, not embedded/direct execution.
- Keeps deprecated alias rejection under test (where aliases are intentionally unsupported in `v3`).
- Provides broad parser-surface coverage for DDL, DML, transactions, security, modal statements, and diagnostics.

## Run standalone

```bash
bash tests/conformance/v3_native_inet/run_v3_native_inet_ctest.sh
```

It is also registered in CTest as `ConformanceV3NativeParserInet` and uses the compatibility example DB fixture.
