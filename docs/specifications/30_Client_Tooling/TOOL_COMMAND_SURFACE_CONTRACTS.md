# Tool Command Surface Contracts (Alpha)

## Purpose
Define canonical command-line surface rules for the shipped ScratchBird client
tools.

## Global CLI Rules
1. Connection-capable tools accept `--mode` with canonical values:
   `embedded|local-ipc|inet|managed`.
2. `--front-door-mode` is valid only for native INET mode and supports
   `direct|manager_proxy`.
3. Connection-capable tools accept `--sslmode` with canonical values:
   `disable|allow|prefer|require|verify-ca|verify-full`.
4. Tools may accept `--conn-opt KEY=VALUE` for repeatable advanced startup
   settings.
5. Tools that expose auth-policy controls must preserve:
   - `--client-flags`
   - `--auth-method-id`
   - `--auth-method-payload`
   - `--auth-required-methods`
   - `--auth-forbidden-methods`
   - `--auth-require-channel-binding`
   - `--workload-identity-token`
   - `--proxy-principal-assertion`
6. Parsing errors fail before any network or engine action.
7. Any shipped binary that exposes `--version` or `-V` MUST emit the shared
   ScratchBird lifecycle payload:
   - product version string
   - release channel
   - support phase
   - current LTS status
   - LTS cadence summary
   - deprecation notice minima
8. The lifecycle payload MUST be produced from one shared runtime definition so
   the engine, server, listener, and parser binaries cannot silently drift.
9. Parser-family binaries publish the same lifecycle state as the engine binary;
   parser dialect identity does not create a separate release channel or support
   policy.

## `sb_isql`
```text
sb_isql [DATABASE] [--host <h>] [--port <p>] [--user <u>] [--password]
        [--mode embedded|local-ipc|inet|managed]
        [--front-door-mode direct|manager_proxy]
        [--manager-auth-token <t>]
        [--sslmode <mode>]
        [--client-flags <n>]
        [--auth-method-id <id>]
        [--auth-method-payload <payload>]
        [--auth-required-methods <csv>]
        [--auth-forbidden-methods <csv>]
        [--auth-require-channel-binding <bool>]
        [--workload-identity-token <tok>]
        [--proxy-principal-assertion <tok>]
        [--conn-opt k=v ...]
        [--command <sql>] [--file <path>]
```

## `sb_fb_isql`
```text
sb_fb_isql [DATABASE] [shared connection options] [--file <path>]
```

## `sb_pg_isql`
```text
sb_pg_isql [DATABASE] [shared connection options] [--file <path>]
```

## `sb_my_isql`
```text
sb_my_isql [DATABASE] [shared connection options] [--file <path>]
```

## `sb_admin`
```text
sb_admin [shared connection options] <command> [command options]
```

Replay and audit administration commands MUST include canonical families for:
- replay open/close/status
- retention policy create/alter/drop/bind
- audit sink profile create/alter/drop
- derivative queue status and quarantine inspection
- shadow group list/status/promote/failback inspection
- schema history and DDL lineage inspection
- replay export and validation

Required bounded `sb_admin` operational families:
- `status`
- `derivative status`
- `derivative retry`
- `derivative quarantine list`
- `shadow list`
- `shadow promote`
- `shadow failback-status`

These families are operator surfaces over existing MGA, derivative, and shadow
contracts. They do not create WAL-style recovery control.

## `sb_backup`
```text
sb_backup [shared connection options] <command>
```

## `sb_security`
```text
sb_security [shared connection options] <command>
```

## `sb_verify`
```text
sb_verify [shared connection options] <command>
```

## `sbdriver-conformance`
```text
sbdriver-conformance --driver <lane> --suite <suite> [--output <path>]
```

## Command Determinism Rules
- identical command/config input produces identical startup payloads.
- `managed` mode is a normalized native INET mode plus
  `front_door_mode=manager_proxy`.
- emulation shell selection does not change the native transport policy fields.
- replay/admin command aliases must map 1:1 to the native replay/audit control
  surface without hidden side effects.
- `--version` output for shipped binaries must remain structurally identical for
  the shared lifecycle lines even when the binary-specific prefix differs.
- derivative or shadow-status commands must preserve the distinction between:
  - local MGA durability health
  - derivative shipping health
  - shadow-group readiness

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
