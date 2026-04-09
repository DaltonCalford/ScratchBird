# ScratchBird Layered Runtime and Authentication Gap Report
Date: 2026-02-25
Status: Working draft (non-authoritative findings report)

## Purpose
Document the concrete implementation gaps identified during review of ScratchBird's layered architecture and planned plugin-based authentication model.

## Scope
1. ScratchBird engine/runtime/auth/listener/manager implementation.
2. Cross-project compatibility surface (`ScratchBird-driver`).
3. Canonical specification alignment in `docs/specifications`.
4. External baseline comparison for plugin auth patterns and MFA/auth policy patterns.

## Method
1. Read canonical specs for section 19 (Security), section 25 (Runtime Modes), section 28 (Parser Implementations), section 29 (Listener/Server Orchestration), and section 17 (UDR connectors).
2. Inspect implementation anchors in `ScratchBird/src` and `ScratchBird/include`.
3. Cross-check driver-side auth handling in `ScratchBird-driver`.
4. Compare architecture direction against external baseline models:
   - Firebird plugin manager and auth plugin types.
   - PostgreSQL OAuth validator model.
   - MySQL MFA/auth plugin chain model.

## Executive Summary
1. Security boundary hardening is incomplete in manager/listener front-door flows.
2. Signed auth-plugin architecture exists in specification but not in runtime implementation.
3. Wire-level auth method handling is fixed-enum and blocks future plugin method growth without core rewrites.
4. Runtime/parser-family inventory does not yet match the nine-family layered architecture contract.
5. UDR remote connector path remains bootstrap scaffold, not production-complete.

## Findings Matrix

### F-001 (Critical): Managed-mode DB binding gate bypass exists
- Requirement intent:
  - Managed mode must enforce LPREFACE/DBBT before auth/data forwarding.
- Observed behavior:
  - Manager routes directly to proxy path when no control socket dir is configured.
- Evidence:
  - `src/server/sb_manager_main.cpp:1081`
  - `docs/specifications/19_Security_Model/ENGINE_AUTHENTICATION_HARDENING_AND_MANAGER_OPTION_SPEC.md:196`
- Impact:
  - Trust boundary can be relaxed by configuration omission.

### F-002 (Critical): DBBT fallback key is hardcoded and shared
- Requirement intent:
  - Binding token key material should be explicit and controlled.
- Observed behavior:
  - Listener and manager both accept hardcoded fallback key (`builtin:default`) if no keyring/env key configured.
- Evidence:
  - `src/network/sb_listener_main.cpp:1759`
  - `src/server/sb_manager_main.cpp:733`
- Impact:
  - Predictable signing key undermines managed trust guarantees.

### F-003 (Critical): Native PASSWORD flow still transmits plaintext credential payload
- Requirement intent:
  - Native wire must not carry plaintext password bytes.
- Observed behavior:
  - PASSWORD auth branch consumes raw payload as password string.
- Evidence:
  - `src/server/server_session.cpp:2268`
  - `src/server/server_session.cpp:2301`
  - `docs/specifications/19_Security_Model/ENGINE_AUTHENTICATION_HARDENING_AND_MANAGER_OPTION_SPEC.md:137`
- Impact:
  - Violates required transport-hardening contract.

### F-004 (Critical): Signed auth plugin admission architecture is not implemented in runtime
- Requirement intent:
  - Only signed/authorized plugins load; unknown IDs/signers rejected.
- Observed behavior:
  - Runtime uses static method registration and optional in-process factory registration.
  - No runtime evidence of signed package admission/JWS trust policy enforcement.
- Evidence:
  - `src/security/auth_manager.cpp:815`
  - `src/security/auth_method.cpp:427`
  - `docs/specifications/19_Security_Model/AUTH_PLUGIN_ARCHITECTURE_AND_SIGNED_MODULE_ABI.md:71`
  - `docs/specifications/19_Security_Model/AUTH_PLUGIN_ARCHITECTURE_AND_SIGNED_MODULE_ABI.md:168`
- Impact:
  - Planned security model is not enforceable today.

### F-005 (High): Auth method negotiation is hardcoded to known enum values
- Requirement intent:
  - Support future methods without hardcoded enum churn.
- Observed behavior:
  - Protocol codec rejects unknown auth method bytes in request/challenge parsing.
- Evidence:
  - `include/scratchbird/protocol/wire_protocol.h:71`
  - `src/protocol/wire_protocol.cpp:507`
  - `src/protocol/wire_protocol.cpp:545`
- Impact:
  - Plugin-defined methods cannot be negotiated without core protocol edits.

### F-006 (High): Declared auth method surface exceeds implemented method set
- Requirement intent:
  - Declared auth methods should map to concrete implementations or deterministic policy-disabled states.
- Observed behavior:
  - `AuthType` includes LDAP/Kerberos/Ident/Radius/PAM while factory returns `nullptr` for these.
- Evidence:
  - `include/scratchbird/security/auth_method.h:55`
  - `src/security/auth_method.cpp:389`
  - `src/security/auth_method.cpp:414`
- Impact:
  - API/behavior mismatch; higher operational ambiguity.

### F-007 (High): OAuth/JWT implementation remains partial/stubbed
- Requirement intent:
  - JWT/OIDC method requires robust JOSE/JWKS/introspection handling.
- Observed behavior:
  - OAuth type reuses LDAP slot.
  - JWKS fetch/parse and introspection are stubs.
  - JWT parsing includes simplified JSON scanning.
- Evidence:
  - `include/scratchbird/security/oauth_auth.h:247`
  - `src/security/oauth_auth.cpp:245`
  - `src/security/oauth_auth.cpp:403`
  - `src/security/oauth_auth.cpp:524`
  - `src/security/oauth_auth.cpp:729`
- Impact:
  - Not production-ready for centralized token-based auth.

### F-008 (High): Listener management IPC does not show explicit admin auth gate
- Requirement intent:
  - Management commands require admin authentication.
- Observed behavior:
  - Management handler receives command and executes control actions without explicit command-level auth contract in handler flow.
- Evidence:
  - `src/network/sb_listener_main.cpp:1809`
  - `docs/specifications/29_Listener_and_Server_Orchestration/NORMATIVE_LISTENER_IMPLEMENTATION_CHECKLIST.md:167`
  - `docs/specifications/29_Listener_and_Server_Orchestration/LISTENER_MANAGEMENT_IPC_CHANNEL.md:68`
- Impact:
  - Control-plane command surface is underprotected.

### F-009 (High): Lockout/rate-limit state is process-local only
- Requirement intent:
  - Lockout/rate-limit must be consistent across parser pools/processes.
- Observed behavior:
  - Rate limiter uses in-memory map state.
- Evidence:
  - `include/scratchbird/security/auth_manager.h:249`
  - `src/security/auth_manager.cpp:523`
  - `docs/specifications/19_Security_Model/TEST_CONTRACT.md:50`
- Impact:
  - Enforcement drift across processes.

### F-010 (High): Server/driver auth constant handling diverges
- Requirement intent:
  - Auth negotiation constants and handling must be consistent across client libraries.
- Observed behavior:
  - Elixir driver auth constant mapping/handlers do not align with server method set.
- Evidence:
  - `include/scratchbird/protocol/wire_protocol.h:71`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/p3/drivers/elixir/lib/scratchbird/protocol.ex:92`
  - `/home/dcalford/CliWork/ScratchBird-driver/tracks/p3/drivers/elixir/lib/scratchbird/connection.ex:675`
- Impact:
  - Auth handshake failure risk in cross-version deployments.

### F-011 (High): Runtime parser/listener inventory is incomplete versus nine-family contract
- Requirement intent:
  - Alpha layered architecture requires nine parser families.
- Observed behavior:
  - Parser/listener runtime and binary targets currently cover native/pg/mysql/firebird.
- Evidence:
  - `docs/specifications/25_Runtime_Modes/NORMATIVE_LAYERED_RUNTIME_STACK.md:45`
  - `docs/specifications/29_Listener_and_Server_Orchestration/PROCESS_MODEL_AND_DEPENDENCY_GRAPH.md:21`
  - `src/parser/sb_parser_main.cpp:497`
  - `src/protocol/adapters/protocol_adapter.cpp:783`
  - `src/server/service_controller.cpp:50`
  - `src/CMakeLists.txt:855`
- Impact:
  - Architectural completeness gap for intended scaling model.

### F-012 (High): UDR remote connector runtime is scaffold-grade
- Requirement intent:
  - Remote connector UDR contract requires full ABI/lifecycle operations.
- Observed behavior:
  - Cursor/COPY operations return not implemented.
  - Version string indicates bootstrap scaffold.
- Evidence:
  - `src/udr/udr_connector.cpp:461`
  - `src/udr/udr_connector.cpp:668`
  - `src/udr/udr_connector.cpp:701`
  - `docs/specifications/17_Functions_and_Procedures/NORMATIVE_UDR_REMOTE_ENGINE_CONNECTOR_CHECKLIST.md:27`
- Impact:
  - Remote database fabric/connectivity target is not fully realized.

### F-013 (Medium): DB UUID binding uses name-derived hash path
- Requirement intent:
  - DB identity binding should be canonical/UUID-driven at runtime boundaries.
- Observed behavior:
  - Manager/listener derive DB UUID via FNV-derived function from name.
- Evidence:
  - `src/server/sb_manager_main.cpp:252`
  - `src/network/sb_listener_main.cpp:251`
  - `docs/specifications/19_Security_Model/ENGINE_AUTHENTICATION_HARDENING_AND_MANAGER_OPTION_SPEC.md:199`
- Impact:
  - Potential identity drift vs catalog UUID authority.

### F-014 (Medium): Windows parity is incomplete for control-plane/runtime features
- Requirement intent:
  - Core runtime features should have clear parity/compensating controls per platform.
- Observed behavior:
  - Control-plane sockets return not implemented on Windows.
  - Several runtime libraries disabled on Windows toolchains.
- Evidence:
  - `src/network/control_plane.cpp:1080`
  - `src/CMakeLists.txt:555`
- Impact:
  - Operational parity and supportability gap.

### F-015 (Medium): Password hash fallback is placeholder-grade in password policy module
- Requirement intent:
  - Password verifier storage should use strong KDF.
- Observed behavior:
  - Hashing path uses `std::hash` and comments identify placeholder nature.
- Evidence:
  - `src/security/password_policy.cpp:262`
  - `src/security/password_policy.cpp:270`
- Impact:
  - Security posture risk if used in production path.

### F-016 (Medium): HBA CIDR prefix parsing lacks explicit range validation
- Requirement intent:
  - CIDR parsing should enforce strict prefix bounds for IPv4/IPv6.
- Observed behavior:
  - Prefix is parsed via `stoi` to `uint8_t` without explicit bounds checks prior to mask operations.
- Evidence:
  - `src/security/auth_manager.cpp:340`
  - `src/security/auth_manager.cpp:70`
- Impact:
  - Potential undefined/misclassified match behavior.

## Cross-Project Risk Notes
1. Authentication behavior changes in `ScratchBird` require synchronized updates in `ScratchBird-driver` before rollout.
2. Security contract changes around manager/listener modes will also affect operational tooling and likely `ScratchRobin` admin paths.

## External Baseline References
1. Firebird plugin architecture (plugin manager and typed plugin registration):
   - https://www.firebirdsql.org/en/plugins/
   - https://github.com/FirebirdSQL/firebird/blob/master/src/include/firebird/FirebirdInterface.idl
2. PostgreSQL OAuth validator model (server validator allowlist/module loading):
   - https://www.postgresql.org/docs/current/auth-oauth.html
3. MySQL MFA/auth plugin chain model:
   - https://dev.mysql.com/doc/refman/8.4/en/multifactor-authentication.html
4. JOSE/JWT/OAuth standards:
   - RFC 7515, RFC 7517, RFC 7518, RFC 7519, RFC 7662, RFC 7009, RFC 8725.

## Recommended Next Artifact
Generate a normative implementation-requirements document that maps each finding to atomic `MUST` requirements, acceptance tests, and release gates.
