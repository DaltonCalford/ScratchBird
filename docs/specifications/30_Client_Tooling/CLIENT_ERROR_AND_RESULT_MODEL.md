# Client Error and Result Model (Alpha)

## Purpose
Define deterministic client-library and tool-facing result/error model.

## Result Object Contract
`sb_result_t` must expose:
- result shape id
- column metadata
- row iterator
- execution summary
- correlation id

## Status Categories
- `OK`
- `WARN`
- `ERROR`

## Error Object Fields
```text
error_domain:u16
error_code:string
sqlstate_or_equivalent:opt<string>
message:string
correlation_id:string
request_id:opt<u64>
instruction_index:opt<u32>
opcode_symbol:opt<string>
```

## Domain Mapping
- protocol
- handshake
- security
- execution
- verifier
- service
- internal

## Deterministic Mapping Rules
1. wire `ERROR_FRAME` maps 1:1 to `sb_error_t`.
2. section-22 verifier errors preserve `SBLR-E-XXXX` codes.
3. unknown remote errors map to `CLIENT-ERR-UNKNOWN_REMOTE` with preserved raw payload.

## CLI Exit Code Contract
- `0` success
- `1` user error (syntax/config/policy)
- `2` remote execution error
- `3` transport/protocol error
- `4` authentication/authorization error
- `5` internal client runtime error

## Rendering Modes
- human readable
- json stable schema
- compact log format

JSON error schema keys:
- `status`
- `error_domain`
- `error_code`
- `message`
- `correlation_id`
- `details`


## 2026-03-28 Audit Normalization Update

- Section `30` is normalized to the code-backed `partial` standard.
- Current authority is bounded to the shipped `ScratchBird-driver` surfaces, especially `tracks/p3/drivers/*`, shared connectivity docs, and the concrete CLI/runtime seams.
- Direct native and manager-proxy are the current portable client contract.
- Local runtime modes such as `embedded` and `local-ipc` are bounded tooling/runtime surfaces, not universal parity claims for every maintained language driver.
- The C/C++ lane in the current driver repo is intentionally IP-only; current CLI `embedded` mode is routed through local IPC in the present beta C++ runtime.
- Tool command truth is bounded to the shipped `sb_isql`, `sb_admin`, `sb_backup`, `sb_security`, `sb_verify`, and `sbdriver-conformance` surfaces.
- Recovery language follows MGA/session-repair rules and explicitly excludes WAL-style transaction replay.
- Forensic replay, migration/passthrough, and replication control narratives remain bounded, checklist-only, or target-state-only unless a shipped lane-local control surface is proven.
- Driver-lane claims must stay tied to the current maintained lane set and must not assume universal cross-language parity from section-outline text alone.

## 2026-03-28 Hardening Promotion Update

- Section `30` now carries explicit bounded authority for current maintained `ScratchBird-driver` `p3` lanes.
- Embedded and linked-library language is bounded by the current IP-only C/C++ lane plus tool-local `embedded` or `local-ipc` seams.
- Direct native and manager-proxy remain the current portable client baseline.
- CLI authority is bounded to shipped `sb_isql`, `sb_admin`, `sb_backup`, `sb_security`, `sb_verify`, and `sbdriver-conformance`.
- Error and reconnect language is bounded to deterministic MGA/session repair and explicitly excludes whole-transaction replay.
- Installer, replay, migration, passthrough, and replication client-control claims remain bounded or `target_state_only` unless maintained lane-local proof is promoted.
