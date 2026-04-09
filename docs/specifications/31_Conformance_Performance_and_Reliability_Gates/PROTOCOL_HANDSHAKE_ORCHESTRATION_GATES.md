# Protocol, Handshake, and Orchestration Gates

Status: current_authority_with_reconstructed_expansion

## Purpose
Define conformance and reliability gates for sections `26`, `27`, and `29`.

## Gate Suite T31-PRO-01 Frame and Message Conformance
- frame header field checks
- crc and chunking behavior
- message schema decode and encode coverage
- deterministic status payload fields for MGA health, derivative shipping health, shadow-group readiness, and management inspection rows

Pass criteria:
- 100 percent schema conformance pass on required message catalog

## Gate Suite T31-PRO-02 Handshake Transcript
- native inet transcript order
- parser to IPC transcript order
- auth method negotiation and MFA flows
- manager-fronted proxy negotiation and `LPREFACE` validation where the manager path is enabled
- dormant reattach admission, refusal, and restart replacement branches

Pass criteria:
- deterministic state transitions and error codes for all tested branches
- no privilege or routing branch bypasses engine authorization

## Gate Suite T31-PRO-03 Registry and Capability Visibility
- privilege filtering and deterministic ordering
- emulation filter behavior
- management server and instruction visibility filtering

Pass criteria:
- zero unauthorized registry leakage
- zero unauthorized management-state leakage

## Gate Suite T31-PRO-04 Listener, Parser, Engine, and Manager Layering
- listener startup is server-managed and requires at least one opened database
- parser requires listener plus IPC data path
- listener is not an engine-truth authority
- manager is optional, server-local, and fronts listeners without acquiring engine-truth authority
- listener-management IPC remains tight, local, and untrusted by the engine

Pass criteria:
- all boundary invariants validated
- no listener-local fabrication of engine-owned state

## Gate Suite T31-PRO-05 Pool Assignment, Scaling, and Failure Fallback
- deterministic assignment tie-break
- queue timeout and rejection behavior
- crash replacement and backoff
- localhost-only fallback enforcement
- split-brain prevention checks

Pass criteria:
- assignment determinism maintained under load
- no policy bypass and no non-localhost fallback accepted

## Gate Suite T31-PRO-06 Remote Management Inspection and Mutation Routing
- manager heartbeat inspection visibility
- instruction queue inspection visibility
- route from admin SQL or tooling to engine, controller, and listener boundaries
- refusal paths when capability, privilege, or target generation requirements are unsatisfied

Pass criteria:
- mutation authorization is distinct from inspection authorization
- queue and heartbeat rows preserve stable schema and deterministic terminal state

## Current Code-Backed Entry Points
Current maintained evidence exists through:
- `tests/unit/test_manager_proxy_mcp.cpp`
- `tests/unit/test_auth_policy_protocol_parity.cpp`
- `tests/integration/test_auth_plugin_enterprise_fail_closed.cpp`
- `tests/conformance/v3_native_inet/sql/12_security_show_visibility.sql`
- `tests/conformance/v3_native_inet/sql/13_security_row_level_security.sql`

## Reconstructed Required Expansion
The rebuilt canon additionally requires:
- dedicated manager-heartbeat transcript gates
- listener to engine aggregated status gates
- remote-management routing and refusal transcript gates
- dormant reattach handshake gates at the protocol transcript layer

## Cross-Section References
- `26_Native_Wire_Protocol/PROTOCOL_STATE_MACHINES.md`
- `27_Native_Handshake/HANDSHAKE_STATE_MACHINE_AND_FAILURE_MATRIX.md`
- `29_Listener_and_Server_Orchestration/LISTENER_MANAGEMENT_IPC_CHANNEL.md`
