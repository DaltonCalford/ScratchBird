Last updated: 2026-02-24

# MY-EMU-012 Command Parity Progress

## Scope of this cycle
- Advance MySQL command-wire parity for `COM_CHANGE_USER` in the MySQL protocol adapter path.
- Add direct protocol-level unit coverage for command handling and bound database enforcement.

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

## Verification evidence
- Build:
  - `cmake --build build -j8 --target scratchbird_tests`
- Focused MySQL adapter parity tests:
  - `build/tests/scratchbird_tests --gtest_filter='ProtocolAdapterDialectsC3.MySQL*'`
  - Result: `8/8` passed.
- Bound/emulated session guard regression:
  - `build/tests/scratchbird_tests --gtest_filter='ProtocolAdapterDialectsBinding.*:QueryCompilerV3Test.EmulatedSession*'`
  - Result: passed.
- Wire capture harness stability:
  - `scripts/emulation/generate_wire_capture_parity.py` now force-cleans orphaned listener/parser children tied to the temporary control directory.
  - Emulated auto-start PATH now includes `build/src` explicitly to ensure protocol listener/parser binaries resolve consistently.
  - Live MySQL emulated handshake is now valid and non-zero after parser rebuild (`MY-EMU-011` no longer shows 4-byte zero greeting).

## Status
- `MY-EMU-012`: still `in_progress`.
- Remaining closure items:
  - full live native-vs-emulated command wire replay (`COM_QUERY`/`COM_STMT*`) through listener/parser lanes;
  - finalize parity deltas in upstream MTR execute-mode captures.
