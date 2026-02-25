Last updated: 2026-02-24

# MY-EMU-012 Command Parity Progress

## Scope of this cycle
- Advance MySQL command-wire parity for `COM_CHANGE_USER`, `COM_STMT_PREPARE`, and `COM_STMT_EXECUTE` in the MySQL protocol adapter path.
- Add direct protocol-level unit coverage for command handling, prepared-statement framing, and bound database enforcement.

## Code touchpoints
- `include/scratchbird/protocol/adapters/mysql_adapter.h`
- `src/protocol/adapters/mysql_adapter.cpp`
- `tests/unit/test_protocol_adapter_dialects.cpp`

## Implemented in this cycle
- Added `COM_CHANGE_USER` dispatch handling in `MySqlAdapter::handleCommand`.
- Implemented `MySqlAdapter::handleComChangeUser` behavior:
  - Parse user/auth/db/charset/plugin fields from command payload.
  - Enforce bound-database selection using manager binding context.
  - Reset session state and prepared statements on user switch.
  - Reset and reconnect remote client state under the selected database.
  - Trigger auth switch to `mysql_clear_password` when required.
  - Decode clear-password payload and continue auth when plugin already matches.
- Fixed MySQL wire packet header emission in `MySqlAdapter::sendPacket`:
  - Corrected malformed header construction that emitted invalid length bytes.
- Fixed MySQL greeting auth plugin advertisement:
  - `sendHandshakePacket` now emits the configured plugin (`auth_plugin_name_`) instead of hard-coding `mysql_native_password`.
- Normalized default emulated server version strings to native-style values (removed `-ScratchBird` suffix) to reduce handshake wire drift.
- Added protocol-level prepared-statement command tests:
  - `MySQLComStmtPrepareReturnsPrepareOkPacket` validates wire-format `COM_STMT_PREPARE` response framing and statement id emission.
  - `MySQLComStmtExecuteUnknownStatementReturnsError` validates `COM_STMT_EXECUTE` unknown-id error packet mapping (`ERR`, `HY000`).

## Verification evidence
- Build:
  - `cmake --build build -j8 --target scratchbird_tests`
- Focused MySQL adapter parity tests:
  - `build/tests/scratchbird_tests --gtest_filter='ProtocolAdapterDialectsC3.*'`
  - Result: `14/14` passed.
- Bound/emulated session guard regression:
  - `build/tests/scratchbird_tests --gtest_filter='ProtocolAdapterDialectsBinding.*:QueryCompilerV3Test.EmulatedSession*'`
  - Result: passed.
- Wire capture harness stability:
  - `scripts/emulation/generate_wire_capture_parity.py` now force-cleans orphaned listener/parser children tied to the temporary control directory.
  - Emulated auto-start PATH now includes `build/src` explicitly to ensure protocol listener/parser binaries resolve consistently.
  - Live MySQL emulated handshake is now valid and non-zero after parser rebuild (`MY-EMU-011` no longer shows 4-byte zero greeting).
  - MySQL handshake report now includes baseline-aware normalized comparison so branch-version skew (`native 9.6` vs baseline `8.4`) is tracked separately from command-path parity.

## Status
- `MY-EMU-012`: still `in_progress`.
- Remaining closure items:
  - full live native-vs-emulated command wire replay (`COM_QUERY`/`COM_STMT*`) through listener/parser lanes;
  - finalize parity deltas in upstream MTR execute-mode captures.
