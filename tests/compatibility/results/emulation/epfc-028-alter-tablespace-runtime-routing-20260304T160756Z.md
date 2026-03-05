# EPFC-028 Increment: `SBLR3_ALTER_TABLESPACE` runtime routing + payload closure

- Date (UTC): `2026-03-04T16:07:56Z`
- Scope: close executor unknown-opcode rejection path for `SBLR3_ALTER_TABLESPACE` and align emitter payload with explicit alter actions.

## Code changes

1. `ScratchBird/src/sblr/executor.cpp`
- Added `SBLR3_ALTER_TABLESPACE` handling inside v3 DDL mutation switch.
- Added payload decode for explicit `alterations` list:
  - action `0` (`SET_AUTOEXTEND`) with `autoextend_enabled`
  - action `1` (`SET_AUTOEXTEND_SIZE`) with `size_mb`
  - action `2` (`SET_MAXSIZE`) with `size_mb`
  - action `3` (`RENAME_TO`) with `new_name`
- Added compatibility fallback for legacy `options` payload (for in-flight payloads).
- Added `SBLR3_ALTER_TABLESPACE` to the outer vNext opcode family dispatch list that forwards into `executeDdlMutationOpcode`.

2. `ScratchBird/src/parser/v3_emitter.cpp`
- Replaced placeholder `options` payload for `AlterTablespaceStmt` with explicit `alterations` payload entries containing action-specific fields.

3. Tests authored (not executed in this increment)
- `ScratchBird/tests/unit/test_parser_v3_index_management.cpp`
  - Added `ParsesAlterTablespaceActions`.
- `ScratchBird/tests/unit/test_sblr_vnext_executor_dispatch_contract.cpp`
  - Added `AlterTablespaceOpcodeRoutesWithoutUnknownOpcodeReject`.

## Validation status

- No build/test execution was run in this increment.
- Next action: run targeted parser + vNext dispatch tests and then bounded upstream `pg_regress` tablespace lane to capture pass/fail delta after runtime route closure.
