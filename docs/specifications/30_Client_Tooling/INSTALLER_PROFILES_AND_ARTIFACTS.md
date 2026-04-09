# Installer Profiles and Artifacts

## Purpose
Define deterministic installer profiles, required artifacts, forbidden
combinations, and first-start behavior contracts.

## Hard Invariants
1. Installer profiles must not violate runtime layer boundaries.
2. INET client access requires server-opened database plus listener and parser readiness.
3. `client_libs_only` profile must not start server or listener services.
4. Profile resolution is explicit and deterministic.
5. The canonical Linux installer artifact is the single-file `.run` bundle
   defined by `LINUX_SINGLE_FILE_INSTALLER_WIZARD_AND_RELEASE_STAGE_MODEL.md`.
6. Full emulated-engine enablement requires the parser package, compiler UDR,
   and emulation UDR for that family; no half-installed family may rely on
   compiled core-engine emulation behavior.
7. Release-stage-disabled surfaces may be visible but must not be installable.
8. Section `17` analytical and domain UDR packages must be installer-visible
   according to release state and dependency state.

## Supported Installer Profiles

### `embedded_only`
Required artifacts:
- embedded engine library
- local API headers and runtime config template

Forbidden:
- server service registration
- listener service registration
- parser worker deployment as managed services

### `server_ipc`
Required artifacts:
- `sb_ipc_server` service binary
- embedded engine runtime for server
- IPC client libraries
- startup config file template

Optional artifacts:
- compiler UDR and emulation UDR packages for staged or family-only emulation
  support
- analytical and domain UDR packages selected by the operator

Forbidden:
- listener service enablement at install time

### `server_ipc_listener_parser`
Required artifacts:
- everything in `server_ipc`
- listener service binaries
- parser worker binaries for enabled parser families
- compiler UDR packages for enabled emulated engine families
- emulation UDR packages for enabled emulated engine families
- analytical and domain UDR packages selected by the operator
- listener/parser pool config templates

### `client_libs_only`
Required artifacts:
- IPC and INET client libraries
- API headers
- client tool binaries selected by installer

Forbidden:
- server engine binaries
- service registration
- local database creation side effects

### `full_stack`
Required artifacts:
- server, listener, parser families, compiler UDR packages, emulation UDR
  packages, analytical/domain UDR packages selected by the operator, embedded
  libraries, client libraries, tooling

Beta 1 clarification:
- `full_stack` on Linux is the primary operator profile.
- Cluster-only layers may appear as disabled release-stage surfaces but are not
  selectable until promoted.

## Install-Time Questions (Normative)
Installer must collect:
1. Desired profile.
2. Desired deployment mode.
3. Whether to configure server service.
4. Initial startup config file path and defaults.
5. Whether to create first database now.
6. Which parser families to enable by default.
7. Which compiler UDR packages to install for each emulated engine family.
8. Which emulation UDR packages to install for each emulated engine family.
9. Which emulated engine families to enable after package validation.
10. Default bind addresses, ports, and allowed networks for the engine and each
    enabled emulated front door.
11. `SysArch` bootstrap settings.
12. Initial named `sysadmin` bootstrap settings.
13. Authentication plugin enablement and validation state.
14. Which analytical and domain UDR packages to install, including all
    currently specified section `17` Beta 2 packages with release-state
    visibility.
15. Which UDR modules to auto-open for created database.

## First-Start Behavior
1. If first database is not created, INET listener startup must remain disabled.
2. Server start may succeed with zero opened databases but must report `NO_OPEN_DATABASE` degraded state.
3. Listener/parser startup is skipped when zero databases open successfully.
4. Startup is skipped when `SysArch` bootstrap is incomplete.
5. Startup is skipped when a selected startup-critical authentication plugin is
   not validated.

## Compatibility and Capability Gate
Before enabling listeners:
1. Validate parser family binaries match supported version list.
2. Validate matching compiler UDR and emulation UDR packages exist for each
   enabled emulated family.
3. Validate parser capability profile compatibility with server capability
   profile and the installed compiler/emulation package profiles.
4. Validate engine-wide and per-family allowed-network rules.
5. Validate bind-address and port conflict rules.
6. Validate analytical/domain UDR dependency closure and release-state
   admissibility.
7. Reject incompatible combinations with deterministic install diagnostics.

## Deterministic Install Errors
- `INSTALL_PROFILE_INVALID`
- `INSTALL_PROFILE_CONFLICT`
- `INSTALL_REQUIRED_ARTIFACT_MISSING`
- `INSTALL_FORBIDDEN_ARTIFACT_PRESENT`
- `INSTALL_VERSION_INCOMPATIBLE`
- `INSTALL_CAPABILITY_PROFILE_MISMATCH`
- `INSTALL_RELEASE_STATE_DISABLED`
- `INSTALL_CLUSTER_MODE_NOT_AVAILABLE`
- `INSTALL_DATABASE_CREATE_FAILED`
- `INSTALL_EMULATION_PACKAGE_INCOMPLETE`
- `INSTALL_EMULATION_SUPPORT_MISSING`
- `INSTALL_UDR_PACKAGE_DISABLED`
- `INSTALL_UDR_PACKAGE_DEPENDENCY_MISSING`
- `INSTALL_SYSARCH_BOOTSTRAP_REQUIRED`
- `INSTALL_AUTH_PLUGIN_NOT_VALIDATED`
- `INSTALL_PORT_CONFLICT`
- `INSTALL_ALLOWED_NETWORK_INVALID`

## Evidence Artifacts
- `docs/specifications/work/conformance/tooling/INSTALL_PROFILE_SELECTION_RESULTS.csv`
- `docs/specifications/work/conformance/tooling/INSTALL_ARTIFACT_AUDIT.csv`
- `docs/specifications/work/conformance/tooling/INSTALL_FIRST_START_RESULTS.md`
- `docs/specifications/work/conformance/tooling/INSTALL_COMPATIBILITY_GATE_RESULTS.csv`
- `docs/specifications/work/conformance/tooling/INSTALL_WIZARD_PAGE_FLOW_RESULTS.csv`
- `docs/specifications/work/conformance/tooling/INSTALL_RELEASE_STAGE_VISIBILITY_RESULTS.csv`

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
