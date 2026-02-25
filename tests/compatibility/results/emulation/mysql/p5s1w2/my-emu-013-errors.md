Last updated: 2026-02-24

# MY-EMU-013 Error/SQLSTATE Parity Progress

## Scope of this cycle
- Harden MySQL wire error mapping from ScratchBird core status codes to MySQL error codes and SQLSTATE values.
- Add direct protocol-level unit tests that validate encoded ERR packet fields under `CLIENT_PROTOCOL_41`.

## Code touchpoints
- `include/scratchbird/protocol/adapters/mysql_adapter.h`
- `src/protocol/adapters/mysql_adapter.cpp`
- `tests/unit/test_protocol_adapter_dialects.cpp`

## Implemented in this cycle
- Expanded `mapStatusToMySqlError` coverage for additional core statuses:
  - undefined object classes (`UNDEFINED_TABLE`, `UNDEFINED_COLUMN`, `UNDEFINED_FUNCTION`)
  - duplicate object classes (`DUPLICATE_COLUMN`, `DUPLICATE_OBJECT`)
  - auth/privilege classes (`INVALID_AUTHORIZATION`, `INVALID_PASSWORD`, `INSUFFICIENT_PRIVILEGE`)
  - transaction/locking classes (`LOCK_CONFLICT`, `SERIALIZATION_FAILURE`, `QUERY_CANCELED`)
  - numeric/string semantics (`DIVISION_BY_ZERO`, `OUT_OF_RANGE`, `NUMERIC_VALUE_OUT_OF_RANGE`, `STRING_DATA_RIGHT_TRUNCATION`)
  - connection/protocol classes (`CONNECTION_FAILURE`, `CONNECTION_DOES_NOT_EXIST`, `CONNECTION_CLOSED`, `PROTOCOL_VIOLATION`)
- Added test-only capability setter so unit tests can force protocol-41 wire format (`#` + SQLSTATE) when asserting ERR packet payloads.
- Added new MySQL adapter tests:
  - `MySQLErrorMappingUndefinedColumnUses42S22`
  - `MySQLErrorMappingInvalidAuthorizationUses28000`
  - `MySQLErrorMappingQueryCanceledUses70100`
  - `MySQLErrorMappingOutOfRangeUses22003`

## Verification evidence
- Build:
  - `cmake --build build -j8 --target scratchbird_tests`
- Focused MySQL adapter tests:
  - `build/tests/scratchbird_tests --gtest_filter='ProtocolAdapterDialectsC3.*'`
  - Result: `14/14` passed.

## Status
- `MY-EMU-013`: `in_progress`.
- Remaining closure items:
  - confirm parity against upstream MTR failure classes once execute-mode MTR runs are unblocked;
  - add mappings/tests for remaining error families surfaced by live compatibility suites;
  - finalize documentation with upstream-equivalent warning semantics and edge-case SQLSTATE normalization.
