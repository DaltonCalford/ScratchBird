# Connectivity Profiles and Tool Runtime (Alpha)

## Purpose
Define canonical connectivity families, front-door modes, and shared runtime
policy for ScratchBird client tooling and maintained drivers.

## Connectivity Families

## Profile `embedded_runtime`
- host process links the ScratchBird runtime directly.
- used for embedding and in-process orchestration scenarios.
- not every maintained driver lane is required to implement this profile.

## Profile `local_ipc_runtime`
- tool or runtime client connects to ScratchBird over a local IPC transport.
- valid IPC forms include unix socket, named pipe, or loopback TCP runtime
  bridge where explicitly configured.
- intended for local orchestration and embeddedable-engine bridge scenarios.

## Profile `inet_native_direct`
- client connects to the native listener over SBWP.
- front-door mode is `direct`.
- this is the mandatory connectivity floor for maintained driver lanes.

## Profile `inet_native_managed`
- client connects over SBWP using the manager-proxy front door.
- front-door mode is `manager_proxy`.
- maintained JDBC-baseline lanes must support this profile unless an approved
  lane-specific deviation exists.

## Lane Applicability Model
1. Maintained language drivers are evaluated primarily against
   `inet_native_direct` and `inet_native_managed`.
2. `embedded_runtime` and `local_ipc_runtime` are canonical runtime profiles,
   but they MAY be implemented outside an individual driver lane.
3. The current ScratchBird-driver C/C++ lane is intentionally IP-only; driver-
   side embedded/local IPC behavior is delegated to ScratchBird runtime/server
   layers.
4. CLI tooling may expose `embedded`, `local-ipc`, `inet`, and `managed` mode
   selectors on top of this shared profile model.

## Profile Selection Rules
1. Explicit mode/profile in CLI or API config wins.
2. If no explicit mode is supplied, lane defaults apply.
3. `front_door_mode=manager_proxy` is valid only for native INET transport.
4. `embedded_runtime` and `local_ipc_runtime` reject manager-proxy-only
   configuration deterministically.
5. Selecting an INET profile when no opened database or no active eligible
   listener/parser pool exists fails deterministically.
6. Invalid mode/front-door combinations fail before any auth or session work.

## Shared Connection Policy

### Transport and TLS Controls
- canonical `sslmode` values are:
  `disable|allow|prefer|require|verify-ca|verify-full`
- direct and managed INET profiles share the same `sslmode` vocabulary.
- stronger defaults or restrictions are lane-specific policy decisions and MUST
  be documented in the lane baseline spec.

### Compatibility Startup Controls
- compatibility startup settings include:
  - `binary_transfer`
  - `compression`
  - `currentSchema` / `schema`
  - timeout, fetch-size, pooling, and read-only controls
- maintained drivers SHOULD preserve the JDBC baseline defaults unless the lane
  spec defines an approved compatibility deviation.

### Auth-Policy Transport Controls
- caller auth-policy inputs include:
  - `client_flags` / `connect_client_flags`
  - `auth_method_id`
  - `auth_method_payload`
  - `auth_payload_json`
  - `auth_payload_b64`
  - `auth_provider_profile`
  - `auth_required_methods`
  - `auth_forbidden_methods`
  - `auth_require_channel_binding`
  - `workload_identity_token`
  - `proxy_principal_assertion`
- these fields are first-class startup inputs and MUST be transported without
  silent mutation.
- explicit `auth_method_id` selection and capability-based registry discovery
  are governed by section 27.

## Tool Runtime Components
Shared runtime layers:
1. config loader and mode normalizer
2. auth-policy assembler
3. transport adapter (`embedded`, `local-ipc`, `inet`)
4. request executor and protocol bridge
5. result renderer / shell UX
6. diagnostics, telemetry, and tracing

## Required Alpha Tools
- `sb_isql`
- `sb_fb_isql`
- `sb_pg_isql`
- `sb_my_isql`
- `sb_admin`
- `sb_backup`
- `sb_security`
- `sb_verify`
- `sbdriver-conformance`

## Tool Behavior Contracts

### `sb_isql`
- interactive and script mode.
- explicit transaction control commands.
- deterministic connection-mode normalization and output rendering.

### Emulated SQL shells
- `sb_fb_isql`, `sb_pg_isql`, and `sb_my_isql` are emulated-protocol script
  runners over the shared client runtime.
- protocol emulation selection MUST remain separate from native transport mode
  selection.

### `sb_admin`
- administration, diagnostics, and server-policy operations.
- direct and manager-proxy operational flows are first-class.
- derivative queue, shadow-group, restore-boundary, and failback-boundary
  inspection are first-class read operations in these flows.

### `sb_backup`
- backup and restore control path where supported by server/runtime.
- deterministic progress/error surfaces.

### `sb_security`
- user, role, and security-metadata control path.
- auth-policy-sensitive operations must preserve caller mode and selected auth
  context.

### `sb_verify`
- verification and integrity-scan control path.
- returns deterministic summary/report handles where supported.

## Transport and Retry Rules
- retries are disabled by default for non-idempotent commands.
- idempotent admin/status requests may retry on transport failure.
- retry budget and backoff are configurable.
- retries MUST NOT silently switch transport family or front-door mode.

For derivative and shadow inspection:
- read-only status and list requests are idempotent
- retry may be used for status and list requests only
- promote, failback, retry-release, or quarantine-changing operations are not
  idempotent by default and must not be auto-retried

## Security Rules
- tools must never log plaintext credentials.
- auth token and proxy assertion redaction is mandatory in diagnostics.
- admin/security operations require elevated privileges.
- insecure `sslmode=disable` is never implied; it must come from explicit caller
  choice and lane policy.

Operator diagnostics rendered by tooling must keep separate:
- local MGA durability state
- derivative queue and sink state
- shadow-group readiness state

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
