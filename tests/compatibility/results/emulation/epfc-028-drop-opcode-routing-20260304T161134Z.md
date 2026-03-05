# EPFC-028 Increment: DROP opcode runtime routing closure (`SEQUENCE` `USER` `GROUP`)

- Date (UTC): `2026-03-04T16:11:34Z`
- Scope: eliminate vNext unknown-opcode dispatch failures for `SBLR3_DROP_SEQUENCE` `SBLR3_DROP_USER` `SBLR3_DROP_GROUP` and propagate drop flags from emitter for role/user/group.

## Code changes

1. `ScratchBird/src/sblr/executor.cpp`
- Added v3 DDL mutation handling for:
  - `SBLR3_DROP_SEQUENCE`
  - `SBLR3_DROP_USER`
  - `SBLR3_DROP_GROUP`
- Added these opcodes to outer vNext DDL mutation dispatch list.
- Legacy flag translation implemented:
  - `DROP SEQUENCE`: v3 flags (`if_exists=0x01` `cascade=0x02`) mapped to legacy byte (`cascade=0x01` `if_exists=0x02`).
  - `DROP USER` / `DROP GROUP`: pass through `if_exists`/`cascade` in legacy expected positions.

2. `ScratchBird/src/parser/v3_emitter.cpp`
- `DropRoleStmt` now emits flags (`if_exists` and `cascade`).
- `DropUserStmt` now emits flags (`if_exists` and `cascade`).
- `DropGroupStmt` now emits flags (`if_exists` and `cascade`).

3. Tests authored (not executed in this increment)
- `ScratchBird/tests/unit/test_sblr_vnext_executor_dispatch_contract.cpp`
  - Added `DropSequenceUserGroupOpcodesRouteWithoutUnknownOpcodeReject`.

## Validation status

- No build/test execution was run in this increment.
- Next action: run targeted vNext dispatch and parser tests, then rerun bounded upstream `pg_regress` lane containing `tablespace` and `drop_if_exists` to confirm IRX_0403 deltas.
