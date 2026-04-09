# Client API and Tooling Gates

Status: current_authority_with_reconstructed_expansion

## Purpose
Define certification gates for section `30` embedded API, linked library, CLI,
admin-SQL surfaces, and operator-visible management inspection or mutation
contracts.

## Gate Suite T31-CLI-01 C ABI and Handle Contract
- struct-size guards
- handle lifecycle and ownership
- thread-safety contract checks
- deterministic dormant-reattach handle remapping refusal or acceptance

Pass criteria:
- ABI compatibility checks pass for declared major and minor versions
- handle and reconnect error codes remain stable

## Gate Suite T31-CLI-02 Connectivity Profiles
- embedded profile behavior
- shared-local IPC profile behavior
- network native profile behavior
- manager-fronted proxy profile behavior when enabled

Pass criteria:
- all profile-specific smoke and conformance suites pass
- manager-fronted paths never bypass database or engine authorization

## Gate Suite T31-CLI-03 Service Channel and Status Surfaces
- subscribe, poll, and unsubscribe across channel kinds
- stream completion and error handling
- status row schemas for MGA health, derivative shipping health, and shadow-group readiness
- management inspection row schemas for instruction, server, drift, and target generation state

Pass criteria:
- no leaked streams
- deterministic close behavior
- status and inspection row schemas match canonical field sets exactly

## Gate Suite T31-CLI-04 Security and Secret Handling
- no plaintext credential logging
- redaction policy checks
- MFA challenge and recovery visibility
- privilege separation between inspection and mutation surfaces

Pass criteria:
- 100 percent contract match against reference snapshots
- unauthorized inspection and unauthorized mutation both fail with deterministic error identity

## Gate Suite T31-CLI-05 Remote Management and Admin-SQL Contracts
- `SHOW MANAGEMENT SERVERS`
- `SHOW MANAGEMENT INSTRUCTIONS`
- `SHOW MANAGEMENT DRIFT`
- `ALTER SYSTEM ASSESS REMOTE SET ... ON SERVER ...`
- `ALTER SYSTEM APPLY/CANCEL/QUARANTINE/ACKNOWLEDGE INSTRUCTION ...`

Pass criteria:
- accepted commands produce deterministic result rows
- refused commands preserve prior committed state
- inspection results remain stable across native client and CLI surfaces

## Current Code-Backed Entry Points
Current maintained evidence exists through:
- `tests/unit/test_auth_policy_protocol_parity.cpp`
- `tests/unit/test_auth_mfa_challenge_flow.cpp`
- `tests/unit/test_manager_proxy_mcp.cpp`
- `tests/integration/test_auth_plugin_enterprise_matrix.cpp`
- `tests/conformance/security/run_security_parity_matrix.sh`
- `ScratchBird-driver/tracks/p3/drivers/python`
- `ScratchBird-driver/tracks/p3/drivers/cli`
- `ScratchBird-Benchmarks/acid-tests/runners/acid_test_runner.py`
- `ScratchBird-Benchmarks/engine-differential-tests/runners/differential_test_runner.py`

## Reconstructed Required Expansion
The rebuilt canon additionally requires:
- dedicated admin-SQL result-schema snapshots for remote management
- manager-fronted tooling conformance lanes
- explicit dormant-reattach client contract gates after server restart replacement
- release-facing driver and benchmark harness verification against the maintained
  `ScratchBird-driver` p3 lanes

## Cross-Section References
- `19_Security_Model/ENGINE_AUTHENTICATION_HARDENING_AND_MANAGER_OPTION_SPEC.md`
- `26_Native_Wire_Protocol/REMOTE_MANAGEMENT_STATUS_AND_CONTROL_PAYLOADS.md`
- `30_Client_Tooling/REMOTE_ADMIN_AND_DEPLOYMENT_CONTROL_SURFACE.md`
