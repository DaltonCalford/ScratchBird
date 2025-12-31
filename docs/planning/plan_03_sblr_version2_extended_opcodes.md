# Plan 03 - SBLR Version 2 and 16-bit Extended Opcodes

## Scope
Upgrade SBLR to version 2, switch extended opcode encoding to 16-bit, update all emitters/decoders across the codebase, and extend transaction/2PC/autocommit opcodes. No backward compatibility required.

## Priority
P0 (required for rename/move opcodes and future opcode growth).

## Last Updated
2025-12-31

## Status (Current)
Completed:
- SBLR_VERSION set to 2 and enforced in executor.
- ExtendedOpcode enum migrated to 16-bit; v2 bytecode generator writes 16-bit extended opcodes.
- PostgreSQL/MySQL emitters updated to write 16-bit extended opcodes.
- Executor decodes 16-bit extended opcodes via readExtendedOpcode().
- EXT_RENAME_OBJECT / EXT_MOVE_OBJECT / EXT_SET_AUTOCOMMIT and retaining opcodes are defined and handled in executor.
- ScratchBird parser v2 parses PREPARE/COMMIT/ROLLBACK PREPARED; bytecode generator emits EXT_PREPARE/COMMIT/ROLLBACK PREPARED.
- PostgreSQL parser emits PREPARE/COMMIT/ROLLBACK PREPARED opcodes; Firebird parser now supports SET TRANSACTION payload options.
- Added v2 parser/bytecode tests for transaction payload flags and 2PC opcodes.
- Added READ_COMMITTED_MODE payload flag and encoding (READ CONSISTENCY / RECORD VERSION / NO RECORD VERSION).
- Executor now handles PREPARE/COMMIT/ROLLBACK PREPARED and TransactionManager persists prepared transactions.
- Added executor tests for autocommit transitions and 2PC prepare/commit/rollback flows.
- Extended opcodes added for schema/database DDL (CREATE/DROP/ALTER SCHEMA/DATABASE) and emitted by v2 generator + PostgreSQL/MySQL parsers.

Partial / Outstanding:
- Transaction payload v2 emission is complete for ScratchBird v2; PostgreSQL/MySQL emitters still only write isolation/access/deferrable (and MySQL SET AUTOCOMMIT only).
- EXT_SET_AUTOCOMMIT is emitted by ScratchBird v2 with conflict action/error code support; MySQL still emits DEFAULT conflict action only.
- Wire protocol SBLR version checks outside executor need verification (native/protocol adapters).
- Missing tests for extended opcode streams beyond current transaction payload coverage.

## References
- `docs/specifications/Appendix_A_SBLR_BYTECODE.md`
- `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md`
- `include/scratchbird/sblr/opcodes.h`
- `src/sblr/executor.cpp`
- `src/sblr/bytecode_generator_v2.cpp`
- `include/scratchbird/sblr/bytecode_generator_v2.h`
- `src/parser/mysql/mysql_parser.cpp`
- `src/parser/postgresql/pg_parser_ddl.cpp`
- `src/parser/postgresql/pg_parser_dml.cpp`
- `src/parser/postgresql/pg_parser_expr.cpp`
- `src/parser/postgresql/pg_parser_misc.cpp`
- `src/parser/mysql/mysql_parser.cpp`
- `src/parser/postgresql/pg_parser.cpp`
- `src/parser/mysql/mysql_parser.cpp`

## Decision Gates (Resolved)
- SBLR_VERSION = 2; v1 is not supported.
- Extended opcodes are 16-bit after `Opcode::EXTENDED_OPCODE` (0xFF).
- Existing extended opcodes retain their numeric IDs in the low byte (e.g., 0x0028).

## Required Spec Updates (Already Approved)
- `docs/specifications/Appendix_A_SBLR_BYTECODE.md`: updated to v2 encoding and new opcodes.
- `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md`: version field = 2.
  - Add always-in-transaction semantics and attachment multiplexing (see Plan 16).

## Implementation Tasks
### 1) Opcode Definitions
- `include/scratchbird/sblr/opcodes.h`:
  - Set `constexpr uint8_t SBLR_VERSION = 2;`
  - Add `enum class ExtendedOpcode : uint16_t` and migrate all extended opcodes into it.
  - Add new opcodes:
    - `EXT_RENAME_OBJECT = 0x0100`
    - `EXT_MOVE_OBJECT = 0x0101`
    - `EXT_SET_AUTOCOMMIT = 0x0102`
    - `EXT_COMMIT_RETAINING = 0x0103`
    - `EXT_ROLLBACK_RETAINING = 0x0104`
    - `EXT_PREPARE_TRANSACTION = 0x0105`
    - `EXT_COMMIT_PREPARED = 0x0106`
    - `EXT_ROLLBACK_PREPARED = 0x0107`
  - Add helper functions:
    - `writeInt16` / `readInt16` if not already present.
    - `writeExtendedOpcode16(uint16_t op)` for bytecode writers.

### 2) Bytecode Writers (All Emitters)
- `include/scratchbird/sblr/bytecode_generator_v2.h`:
  - Change `writeExtendedOpcode(uint8_t)` to `writeExtendedOpcode(uint16_t)`:
    - writes `Opcode::EXTENDED_OPCODE` then 16-bit little-endian opcode.
- `src/sblr/bytecode_generator_v2.cpp`:
  - Update all calls to `writeExtendedOpcode(...)` to pass 16-bit values (cast to ExtendedOpcode or uint16_t).
- PostgreSQL parser emitters:
  - `src/parser/postgresql/pg_parser_misc.cpp`
  - `src/parser/postgresql/pg_parser_dml.cpp`
  - `src/parser/postgresql/pg_parser_expr.cpp`
  - `src/parser/postgresql/pg_parser_ddl.cpp`
  - Add helper `emitExtended(uint16_t op)` and replace patterns:
    - OLD: `emit(Opcode::EXTENDED_OPCODE); emit(opcode_byte);`
    - NEW: `emit(Opcode::EXTENDED_OPCODE); emitUInt16(opcode_16);`
- MySQL parser emitter:
  - `src/parser/mysql/mysql_parser.cpp`
  - Same pattern replacement as PostgreSQL.
- Firebird compiler:
  - `src/sblr/bytecode_generator_v2.cpp` already handles; ensure all extended opcodes are 16-bit.

### 3) Executor Decoder
- `src/sblr/executor.cpp`:
  - Enforce SBLR_VERSION == 2 (reject any other version).
  - Add `readExtendedOpcode()` that returns `uint16_t`.
  - Replace every `readByte()` immediately after EXTENDED_OPCODE with `readExtendedOpcode()`.
  - Update all `switch` statements on extended opcodes to use `uint16_t` / `ExtendedOpcode`.
- Ensure all comparisons of `Opcode::EXTENDED_OPCODE` remain byte checks only for the prefix.

### 4) Protocol and Tooling
- Ensure all compilation paths emit SBLR_VERSION=2:
  - `src/parser/postgresql/pg_parser.cpp` (version header)
  - `src/parser/mysql/mysql_parser.cpp`
  - `src/sblr/bytecode_generator_v2.cpp`
- If any network handler validates SBLR version (wire protocol), update to require 2.

### 5) SBLR Rename/Move Integration
- Implement EXT_RENAME_OBJECT / EXT_MOVE_OBJECT handlers per `plan_02_uuid_resolution_and_rename_move.md`.
- Ensure they are encoded as 16-bit extended opcodes.

### 6) Transaction Payload v2 (No Backward Compatibility)
**All transaction opcodes use explicit payloads; no silent defaults.**

**START_TRANSACTION payload (opcode 0x13):**
```
[START_TRANSACTION]
[flags:uint16]
[conflict_action:uint8]               // 0=DEFAULT,1=COMMIT,2=ROLLBACK,3=ERROR,4=KEEP
[conflict_error_code:int32]           // only if flags has CONFLICT_ERROR_CODE
[autocommit_mode:uint8]               // 0=UNCHANGED,1=ON,2=OFF (only if flags has AUTOCOMMIT)
[isolation_level:uint8]               // only if flags has ISOLATION
[access_mode:uint8]                   // only if flags has ACCESS_MODE (0=RW,1=RO)
[deferrable:uint8]                    // only if flags has DEFERRABLE (0=NOT,1=YES)
[wait_mode:uint8]                     // only if flags has WAIT_MODE (0=NO WAIT,1=WAIT)
[lock_timeout:uint32]                 // only if flags has LOCK_TIMEOUT
[reservations:list]                   // only if flags has RESERVATIONS
```

**SET_TRANSACTION payload (opcode 0x17):** identical to START_TRANSACTION payload (no in-place edits; always starts a new transaction per executor rules).

**COMMIT payload (opcode 0x14):**
```
[COMMIT]
[flags:uint8] // bit0=AND_CHAIN, bit1=AND_NO_CHAIN (optional), bit2=RETAINING (Firebird)
```

**ROLLBACK payload (opcode 0x15):**
```
[ROLLBACK]
[flags:uint8] // bit0=AND_CHAIN, bit1=AND_NO_CHAIN (optional), bit2=RETAINING (Firebird)
```

**COMMIT/ROLLBACK RETAINING:** use flags above. If RETAINING is set, executor commits/rolls back but keeps the transaction context open per Firebird semantics.

**2PC opcodes (extended, 16-bit):**
```
[EXT_PREPARE_TRANSACTION][gid:string]
[EXT_COMMIT_PREPARED][gid:string]
[EXT_ROLLBACK_PREPARED][gid:string]
```

**SET AUTOCOMMIT opcode (extended, 16-bit):**
```
[EXT_SET_AUTOCOMMIT]
[mode:uint8]           // 0=OFF,1=ON
[conflict_action:uint8]
[conflict_error_code:int32]  // only if conflict_action == ERROR
```

**Flags (START/SET TRANSACTION):**
- `0x0001` HAS_ISOLATION
- `0x0002` HAS_ACCESS_MODE
- `0x0004` HAS_DEFERRABLE
- `0x0008` HAS_WAIT_MODE
- `0x0010` HAS_LOCK_TIMEOUT
- `0x0020` HAS_RESERVATIONS
- `0x0040` HAS_AUTOCOMMIT
- `0x0080` HAS_CONFLICT_ERROR_CODE

## Known Hotspots (Must Be Updated)
- Bytecode writer API signature change (`writeExtendedOpcode`).
- All parser emit sites that use raw EXTENDED_OPCODE prefix.
- Executor extended opcode decoding in:
  - `executeExtendedOpcode` dispatch
  - SELECT/JOIN subquery decoding
  - MERGE extended opcode parsing
  - Window/aggregate extended opcode parsing

## Completion Checklist (Developer)
- [ ] SBLR_VERSION set to 2 and enforced in executor.
- [ ] All emitters write 16-bit extended opcodes.
- [ ] All decoder paths read 16-bit extended opcodes.
- [ ] EXT_RENAME_OBJECT and EXT_MOVE_OBJECT defined and encoded as 16-bit.
- [ ] Transaction payload v2 implemented for START/SET/COMMIT/ROLLBACK.
- [ ] New extended opcodes for AUTOCOMMIT and 2PC implemented.
- [ ] Network SBLR version check requires 2.
- [ ] All SBLR tests updated to v2.

## Completion Checklist (Auditor)
- [ ] Bytecode streams begin with VERSION opcode and version byte = 2.
- [ ] Any EXTENDED_OPCODE sequence is followed by 2 bytes, not 1.
- [ ] Executor rejects version != 2.
- [ ] No lingering `emit(EXTENDED_OPCODE); emit(byte)` patterns remain.
- [ ] SBLR rename/move opcodes decode correctly.

## Testing Requirements
- Unit test: encode/decode a minimal extended opcode and verify round-trip.
- Parser tests: MySQL and PostgreSQL emit 16-bit extended opcodes.
- Executor tests: extended opcode dispatch works with 16-bit values.
- Wire protocol test: SBLR_COMPILED with version 2 accepted; version 1 rejected.
- Transaction bytecode tests: START/SET with flags, COMMIT/ROLLBACK flags, RETAINING, 2PC opcodes, SET AUTOCOMMIT.

## Concrete Test Cases
- Emit EXT_SHOW_TABLES (existing extended opcode) and verify executor reads 0x00xx value.
- Emit EXT_RENAME_OBJECT and verify executor dispatches to rename handler.
- Validate that a bytecode stream with EXTENDED_OPCODE followed by 1 byte is rejected.
- Emit START_TRANSACTION with conflict_action + AUTOCOMMIT flag and verify executor decodes all fields.
- Emit COMMIT with RETAINING flag and verify executor receives retaining=1.
- Emit EXT_PREPARE_TRANSACTION/EXT_COMMIT_PREPARED/EXT_ROLLBACK_PREPARED and verify payload parsing.

## Acceptance Criteria
- All extended opcodes are 16-bit in bytecode.
- Executor rejects any SBLR stream not version 2.
- No 8-bit extended opcode decoding remains in codebase.

## Common Failure Patterns
- One or more emitters still write a single byte after EXTENDED_OPCODE.
- Executor uses `uint8_t` for extended opcodes, truncating new opcodes > 0xFF.
- Mixed v1/v2 bytecode in tests causing false positives.
