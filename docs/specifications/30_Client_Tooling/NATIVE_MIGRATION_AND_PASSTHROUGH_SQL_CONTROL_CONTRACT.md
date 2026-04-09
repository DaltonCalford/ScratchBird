# Native Migration and Passthrough SQL Control Contract

Status: current_authority_with_reconstructed_expansion

## Purpose

Define strict native-parser SQL and admin surfaces for passthrough and migration so implementation can be completed step-by-step without undocumented behavior.

## Scope

- native parser SQL and control commands only
- control of migration metadata in section `24` and orchestration behavior in sections `28` and `29`
- output contracts consumed by CLI and API tooling

## Hard Invariants

1. Commands in this file are native-parser only unless explicitly mapped by an emulated parser profile.
2. Engine remains SQL-agnostic; parser maps these commands to control SBLR operations.
3. Every mutation transition must provide `EXPECT VERSION` and must fail on version mismatch.
4. Command success requires persistence in section `24` catalog rows before success response.
5. Result-set column order and names are fixed.
6. Donor capability class and extraction mode must be queryable for every migration.
7. Weak donors must not be promoted into stronger guarantees by SQL surface wording.

## Canonical SQL Grammar

### 1. Create Migration
```sql
CREATE MIGRATION <migration_name>
SOURCE SERVER <fdw_server_name>
SOURCE ENGINE <engine_name>
SOURCE DATABASE <source_database_name>
SOURCE PATH <source_path>
TARGET SCHEMA <target_schema_path>
OPTIONS (
  compare_policy = <ROW_COUNT_ONLY|ROW_COUNT_AND_CHECKSUM|FULL_ROW_COMPARE>,
  write_policy = <STRICT|LENIENT>,
  mirror_policy = <STRICT|LENIENT>,
  return_source = <LEGACY|EMULATED>,
  audit_sample_rate = <float_0_to_1>
);
```

### 2. Assess Migration Source
```sql
ASSESS MIGRATION <migration_name>
EXPECT VERSION <mode_version_uint64>;
```
Semantics:
- records donor capability class
- records extraction and verification mode proposal
- refuses success if source validation cannot determine a safe capability class

### 3. Alter Migration Mode
```sql
ALTER MIGRATION <migration_name>
SET MODE <PROXY_ONLY|EMULATED_BUILD|DUAL_READ_AUDIT|CUTOVER_READY|PRIMARY_EMULATED|MIRROR_LEGACY|RETIRED>
EXPECT VERSION <mode_version_uint64>
[APPROVAL TOKEN <approval_token_text>];
```

### 4. Alter Migration Options
```sql
ALTER MIGRATION <migration_name>
SET OPTION <compare_policy|write_policy|mirror_policy|return_source|audit_sample_rate>
= <value>
EXPECT VERSION <mode_version_uint64>;
```

### 5. Verify Migration
```sql
VERIFY MIGRATION <migration_name>
EXPECT VERSION <mode_version_uint64>;
```
Semantics:
- performs deterministic reconciliation or verification pass
- records mismatch and drift summary
- may transition to `DUAL_READ_AUDIT` or `CUTOVER_READY` depending on donor capability and policy

### 6. Cutover Migration
```sql
CUTOVER MIGRATION <migration_name>
EXPECT VERSION <mode_version_uint64>
[APPROVAL TOKEN <approval_token_text>];
```
Semantics:
- transitions to `PRIMARY_EMULATED` only after committed readiness proof
- fails when unresolved drift or capability class does not satisfy policy

### 7. Retire Migration
```sql
RETIRE MIGRATION <migration_name>
EXPECT VERSION <mode_version_uint64>;
```

### 8. Drop Migration
```sql
DROP MIGRATION <migration_name> [FORCE];
```
Semantics:
- without `FORCE`, command is allowed only when mode is `RETIRED`
- with `FORCE`, command records forced event and closes active cursors before invalidation

### 9. Show Migration Status
```sql
SHOW MIGRATION STATUS [<migration_name>];
```

### 10. Show Migration Audit
```sql
SHOW MIGRATION AUDIT <migration_name>
[WINDOW MINUTES <uint32>]
[DETAIL <SUMMARY|MISMATCH|ERRORS>];
```

## Deterministic State Transition Contract

Allowed transitions:
- `PROXY_ONLY -> EMULATED_BUILD`
- `EMULATED_BUILD -> DUAL_READ_AUDIT`
- `DUAL_READ_AUDIT -> CUTOVER_READY`
- `CUTOVER_READY -> PRIMARY_EMULATED`
- `PRIMARY_EMULATED -> MIRROR_LEGACY`
- `MIRROR_LEGACY -> RETIRED`
- `PRIMARY_EMULATED -> RETIRED`

Rollback transitions:
- `CUTOVER_READY -> DUAL_READ_AUDIT`
- `PRIMARY_EMULATED -> RECONCILING_FAILBACK` through orchestration state
- `MIRROR_LEGACY -> PRIMARY_EMULATED`

Forbidden transitions:
- any transition when `EXPECT VERSION` does not match current `mode_version`
- any transition to `CUTOVER_READY` without committed verification evidence
- any transition that would imply dual-write or native replication on a weak donor capability class

## Parser-to-SBLR Control Mapping

| SQL Command | Canonical Control Op | Required Catalog Writes |
| --- | --- | --- |
| `CREATE MIGRATION` | `CTL_MIGRATION_CREATE` | `migration_job`, `migration_event` |
| `ASSESS MIGRATION` | `CTL_MIGRATION_ASSESS_SOURCE` | `migration_job`, `migration_event`, capability rows |
| `ALTER MIGRATION ... SET MODE` | `CTL_MIGRATION_SET_MODE` | `migration_job`, `migration_event` |
| `ALTER MIGRATION ... SET OPTION` | `CTL_MIGRATION_SET_OPTION` | `migration_job`, `migration_event` |
| `VERIFY MIGRATION` | `CTL_MIGRATION_VERIFY` | `migration_job`, `migration_event`, verification rows |
| `CUTOVER MIGRATION` | `CTL_MIGRATION_CUTOVER` | `migration_job`, `migration_event`, readiness rows |
| `RETIRE MIGRATION` | `CTL_MIGRATION_RETIRE` | `migration_job`, `migration_event` |
| `DROP MIGRATION` | `CTL_MIGRATION_DROP` | invalidation of migration rows and close event |
| `SHOW MIGRATION STATUS` | `CTL_MIGRATION_SHOW_STATUS` | none (read-only) |
| `SHOW MIGRATION AUDIT` | `CTL_MIGRATION_SHOW_AUDIT` | none (read-only) |

## Result Set Contracts

### `SHOW MIGRATION STATUS`
Fixed columns in order:
1. `migration_name` TEXT
2. `migration_uuid` UUID
3. `source_engine` TEXT
4. `runtime_mode` TEXT
5. `mode_version` UINT64
6. `donor_capability_class` TEXT
7. `extraction_mode` TEXT
8. `verification_state` TEXT
9. `unresolved_drift_count` UINT64
10. `lag_ms` UINT64
11. `open_error_count` UINT32
12. `last_event_time` TIMESTAMP_WITH_ZONE
13. `target_schema_path` TEXT

### `SHOW MIGRATION AUDIT ... DETAIL SUMMARY`
1. `migration_name` TEXT
2. `window_minutes` UINT32
3. `read_compare_total` UINT64
4. `read_compare_mismatch` UINT64
5. `write_apply_total` UINT64
6. `write_apply_failed` UINT64
7. `mismatch_rate` FLOAT64
8. `error_rate` FLOAT64
9. `donor_capability_class` TEXT

### `SHOW MIGRATION AUDIT ... DETAIL MISMATCH`
1. `event_time` TIMESTAMP_WITH_ZONE
2. `statement_fingerprint` BINARY(32)
3. `legacy_row_count` UINT64
4. `emulated_row_count` UINT64
5. `mismatch_count` UINT32
6. `compare_policy` TEXT
7. `drift_class` TEXT

### `SHOW MIGRATION AUDIT ... DETAIL ERRORS`
1. `last_seen_time` TIMESTAMP_WITH_ZONE
2. `error_class` TEXT
3. `severity` TEXT
4. `source_component` TEXT
5. `source_code` TEXT
6. `occurrence_count` UINT32
7. `is_open` BOOLEAN

## Deterministic Error Contract

| Error Code | Trigger |
| --- | --- |
| `SB-MIG-0001` | migration name not found |
| `SB-MIG-0002` | migration name ambiguous |
| `SB-MIG-0003` | illegal mode transition |
| `SB-MIG-0004` | expected mode version mismatch |
| `SB-MIG-0005` | cutover guard failed |
| `SB-MIG-0006` | retire guard failed |
| `SB-MIG-0007` | option value out of range |
| `SB-MIG-0008` | permission denied |
| `SB-MIG-0009` | forced drop rejected by policy |
| `SB-MIG-0010` | source engine not enabled for migration |
| `SB-MIG-0011` | donor capability assessment missing or stale |
| `SB-MIG-0012` | unresolved drift exceeds policy |

## CLI Control Contract

- `sb_admin migration create ...` maps to `CREATE MIGRATION`
- `sb_admin migration assess ...` maps to `ASSESS MIGRATION`
- `sb_admin migration set-mode ...` maps to `ALTER MIGRATION ... SET MODE`
- `sb_admin migration verify ...` maps to `VERIFY MIGRATION`
- `sb_admin migration cutover ...` maps to `CUTOVER MIGRATION`
- `sb_admin migration retire ...` maps to `RETIRE MIGRATION`
- `sb_admin migration status` maps to `SHOW MIGRATION STATUS`
- `sb_admin migration audit` maps to `SHOW MIGRATION AUDIT`

## Normative Implementation Checklist

1. Parse and validate command grammar before any control-op dispatch.
2. Resolve migration and server names to UUIDs using discoverability-safe binding.
3. Enforce role and privilege checks for all mutation commands.
4. Require and validate `EXPECT VERSION` for every mode or option mutation.
5. Record donor capability class before permitting cutover.
6. Persist catalog updates and event rows in one transaction.
7. Return fixed result schemas in fixed column order.
8. Map every failure to a deterministic `SB-MIG-XXXX` code.
9. Block emulated parser exposure unless explicit profile mapping exists.
10. Log command correlation id for all migration control operations.

## Cross-Section Links

- `24_Catalog_Model_and_Virtual_Overlays/REMOTE_CONNECTOR_AND_PROXY_MIGRATION_CATALOG_MODEL.md`
- `28_Parser_Implementations/NORMATIVE_PARSER_PASSTHROUGH_AND_LIVE_MIGRATION_CHECKLIST.md`
- `29_Listener_and_Server_Orchestration/MIGRATION_STATE_MACHINE_AND_CUTOVER_GUARANTEE.md`
