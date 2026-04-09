# Native Replication SQL Control Contract (Alpha)

## Purpose
Define strict native-parser SQL/control contracts for one-way and bi-directional replication channels with deterministic state transitions, policy controls, diagnostics, and conflict handling.

## Scope
- Native parser SQL/control surface only.
- Parser maps commands to canonical control SBLR; engine remains SQL-agnostic.
- Covers publisher/subscriber and peer/peer replication channels.

## Hard Invariants
1. Channel mutation commands require `EXPECT VERSION`.
2. One-way and bi-directional semantics are explicit and non-interchangeable.
3. Split-brain fence blocks unsafe control operations until cleared.
4. Result set schemas are fixed per command class.
5. Every mutation persists channel event evidence before success response.

## Canonical SQL Grammar

### 1. Create Replication Channel
```sql
CREATE REPLICATION CHANNEL <channel_name>
DIRECTION <ONE_WAY|BIDIRECTIONAL>
SOURCE SERVER <source_server_name>
TARGET SERVER <target_server_name>
PUBLICATION <publication_name>
SUBSCRIPTION <subscription_name>
OPTIONS (
  ddl_policy = <BLOCK|MANUAL_APPROVE|SAFE_ONLY|FULL>,
  conflict_policy = <SOURCE_WINS|TARGET_WINS|LAST_COMMIT_WINS|ORIGIN_PRIORITY|MANUAL_REQUIRED>,
  max_retry_count = <uint16>,
  retry_backoff_base_ms = <uint64>,
  retry_backoff_max_ms = <uint64>,
  lag_warn_ms = <uint64>,
  lag_critical_ms = <uint64>,
  split_brain_fence_enabled = <true|false>
);
```

### 2. Alter Replication Channel State
```sql
ALTER REPLICATION CHANNEL <channel_name>
SET STATE <INIT|SNAPSHOT|CATCHUP|STREAMING|PAUSED|DEGRADED|FENCED|STOPPED|FAILED>
EXPECT VERSION <mode_version_uint64>
[APPROVAL TOKEN <approval_token_text>];
```

### 3. Alter Replication Channel Policy
```sql
ALTER REPLICATION CHANNEL <channel_name>
SET POLICY <ddl_policy|conflict_policy|max_retry_count|retry_backoff_base_ms|retry_backoff_max_ms|lag_warn_ms|lag_critical_ms|split_brain_fence_enabled>
= <value>
EXPECT VERSION <mode_version_uint64>;
```

### 4. Start, Pause, Resume, Stop Helpers
```sql
START REPLICATION CHANNEL <channel_name> EXPECT VERSION <mode_version_uint64>;
PAUSE REPLICATION CHANNEL <channel_name> EXPECT VERSION <mode_version_uint64>;
RESUME REPLICATION CHANNEL <channel_name> EXPECT VERSION <mode_version_uint64>;
STOP REPLICATION CHANNEL <channel_name> EXPECT VERSION <mode_version_uint64>;
```

### 5. Reseed / Resync
```sql
RESEED REPLICATION CHANNEL <channel_name>
[TABLE <schema_path>]
EXPECT VERSION <mode_version_uint64>;
```

### 6. Resolve Conflict
```sql
RESOLVE REPLICATION CONFLICT <conflict_uuid>
ACTION <APPLY_SOURCE|APPLY_TARGET|MARK_IGNORED|MANUAL_PATCH>
[PATCH <json_payload>]
EXPECT VERSION <mode_version_uint64>;
```

### 7. Split-Brain Controls
```sql
FENCE REPLICATION CHANNEL <channel_name> EXPECT VERSION <mode_version_uint64> [REASON <text>];
CLEAR REPLICATION FENCE <channel_name> EXPECT VERSION <mode_version_uint64> APPROVAL TOKEN <approval_token_text>;
```

### 8. Status and Diagnostics
```sql
SHOW REPLICATION STATUS [<channel_name>];
SHOW REPLICATION LAG [<channel_name>];
SHOW REPLICATION CURSORS [<channel_name>];
SHOW REPLICATION CONFLICTS [<channel_name>] [STATE <OPEN|AUTO_RESOLVED|MANUAL_PENDING|MANUAL_RESOLVED|IGNORED>];
SHOW REPLICATION EVENTS [<channel_name>] [WINDOW MINUTES <uint32>];
```

## Deterministic State Transition Contract

### One-Way Allowed Transitions
- `INIT -> SNAPSHOT`
- `SNAPSHOT -> CATCHUP`
- `CATCHUP -> STREAMING`
- `STREAMING -> PAUSED|DEGRADED|STOPPED|FAILED`
- `PAUSED -> STREAMING|STOPPED`
- `DEGRADED -> STREAMING|PAUSED|FAILED`
- `FAILED -> INIT` (reseed path)

### Bi-Directional Allowed Transitions
- all one-way transitions plus:
- `STREAMING -> FENCED`
- `FENCED -> DEGRADED` (clear fence workflow only)
- `DEGRADED -> STREAMING` (recovery guards pass)

### Forbidden Transitions
- `INIT -> STREAMING` direct
- `FENCED -> STREAMING` direct
- any transition without matching `EXPECT VERSION`

## Parser-to-SBLR Control Mapping

| SQL Command | Canonical Control Op | Required Catalog Writes |
| --- | --- | --- |
| `CREATE REPLICATION CHANNEL` | `CTL_REPL_CHANNEL_CREATE` | `replication_channel`, `replication_channel_member` |
| `ALTER REPLICATION CHANNEL SET STATE` | `CTL_REPL_CHANNEL_SET_STATE` | `replication_channel`, event persistence |
| `ALTER REPLICATION CHANNEL SET POLICY` | `CTL_REPL_CHANNEL_SET_POLICY` | `replication_channel` |
| `START|PAUSE|RESUME|STOP REPLICATION CHANNEL` | `CTL_REPL_CHANNEL_LIFECYCLE` | `replication_channel` |
| `RESEED REPLICATION CHANNEL` | `CTL_REPL_CHANNEL_RESEED` | channel event + reseed task rows |
| `RESOLVE REPLICATION CONFLICT` | `CTL_REPL_CONFLICT_RESOLVE` | `replication_conflict`, optional patch evidence |
| `FENCE REPLICATION CHANNEL` | `CTL_REPL_CHANNEL_FENCE` | channel state + split-brain event |
| `CLEAR REPLICATION FENCE` | `CTL_REPL_CHANNEL_CLEAR_FENCE` | channel state + split-brain clear event |
| `SHOW REPLICATION STATUS` | `CTL_REPL_SHOW_STATUS` | none (read-only) |
| `SHOW REPLICATION LAG` | `CTL_REPL_SHOW_LAG` | none (read-only) |
| `SHOW REPLICATION CURSORS` | `CTL_REPL_SHOW_CURSORS` | none (read-only) |
| `SHOW REPLICATION CONFLICTS` | `CTL_REPL_SHOW_CONFLICTS` | none (read-only) |
| `SHOW REPLICATION EVENTS` | `CTL_REPL_SHOW_EVENTS` | none (read-only) |

## Result Set Contracts

### `SHOW REPLICATION STATUS`
1. `channel_name` TEXT
2. `replication_channel_uuid` UUID
3. `direction` TEXT
4. `channel_state` TEXT
5. `mode_version` UINT64
6. `open_conflict_count` UINT32
7. `open_error_count` UINT32
8. `last_event_time` TIMESTAMP_WITH_ZONE

### `SHOW REPLICATION LAG`
1. `channel_name` TEXT
2. `member_name` TEXT
3. `cursor_name` TEXT
4. `source_commit_seq` UINT64
5. `applied_commit_seq` UINT64
6. `lag_ms` UINT64
7. `cursor_state` TEXT

### `SHOW REPLICATION CURSORS`
1. `channel_name` TEXT
2. `member_name` TEXT
3. `cursor_name` TEXT
4. `cursor_state` TEXT
5. `heartbeat_time` TIMESTAMP_WITH_ZONE
6. `last_error_code` TEXT

### `SHOW REPLICATION CONFLICTS`
1. `replication_conflict_uuid` UUID
2. `channel_name` TEXT
3. `conflict_kind` TEXT
4. `source_origin` TEXT
5. `target_origin` TEXT
6. `resolution_state` TEXT
7. `source_commit_seq` UINT64
8. `target_commit_seq` UINT64
9. `resolved_time` TIMESTAMP_WITH_ZONE

### `SHOW REPLICATION EVENTS`
1. `event_time` TIMESTAMP_WITH_ZONE
2. `channel_name` TEXT
3. `event_kind` TEXT
4. `event_note` TEXT
5. `actor_name` TEXT

## Deterministic Error Contract

| Error Code | Trigger |
| --- | --- |
| `SB-REPL-0001` | replication channel not found |
| `SB-REPL-0002` | replication channel ambiguous |
| `SB-REPL-0003` | illegal channel state transition |
| `SB-REPL-0004` | expected version mismatch |
| `SB-REPL-0005` | direction-incompatible command |
| `SB-REPL-0006` | split-brain fence active |
| `SB-REPL-0007` | conflict action invalid for conflict kind |
| `SB-REPL-0008` | policy value invalid or out of range |
| `SB-REPL-0009` | required approval token missing or invalid |
| `SB-REPL-0010` | replication channel in failed state requires reseed |

## Tooling Control Contract
- `sb_admin replication create ...` maps to `CREATE REPLICATION CHANNEL`.
- `sb_admin replication set-state ...` maps to `ALTER REPLICATION CHANNEL ... SET STATE`.
- `sb_admin replication set-policy ...` maps to `ALTER REPLICATION CHANNEL ... SET POLICY`.
- `sb_admin replication start|pause|resume|stop ...` maps to lifecycle helper commands.
- `sb_admin replication conflicts resolve ...` maps to `RESOLVE REPLICATION CONFLICT`.
- `sb_admin replication fence|clear-fence ...` maps to fence commands.
- `sb_admin replication status|lag|cursors|conflicts|events ...` maps to `SHOW REPLICATION ...`.

## Normative Implementation Checklist
1. Parse replication commands with strict grammar and no fallback parsing.
2. Resolve channel and conflict identifiers using UUID binding.
3. Enforce privilege checks for all mutation commands.
4. Enforce `EXPECT VERSION` guards on all state/policy/conflict mutations.
5. Validate direction/state compatibility before dispatch.
6. Persist channel/event/conflict mutations transactionally before success responses.
7. Return fixed schema and column order for all `SHOW REPLICATION` commands.
8. Map all failures to deterministic `SB-REPL-XXXX` codes.
9. Block fence-clear unless approval token and convergence checks pass.
10. Reject emulated parser exposure unless explicit profile mapping exists.

## Cross-Section Links
- `24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_REPLICATION_RUNTIME_AND_CONFLICT_RESOLUTION.md`
- `28_Parser_Implementations/NORMATIVE_PARSER_REPLICATION_CONTROL_AND_ROUTING_CHECKLIST.md`
- `29_Listener_and_Server_Orchestration/NORMATIVE_LISTENER_ONE_WAY_AND_BIDIRECTIONAL_REPLICATION_CHECKLIST.md`

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
