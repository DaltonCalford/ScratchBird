# Security, Authorization, and Remote Management Gates

Status: current_authority_with_reconstructed_expansion

## Purpose
Define the certification families for security enforcement, authorization,
authentication plugin behavior, masking, shared rights, manager-fronted control,
and remote-management instruction lifecycle correctness.

## Gate Suite T31-SEC-01 Authorization and Object Visibility
- default object visibility and public access rules
- ownership transfer and owner-scoped privileges
- select, update, delete, references, execute, and visible privilege contracts
- view, procedure, function, and package sandboxing where the invoker lacks direct rights to underlying objects

Current code-backed entrypoints:
- `tests/conformance/v3_native_inet/sql/09_security_default_access_public.sql`
- `tests/conformance/v3_native_inet/sql/10_security_ownership_alter_owner.sql`
- `tests/conformance/v3_native_inet/sql/11_security_grants_dml_execute_view.sql`

## Gate Suite T31-SEC-02 Row, Column, and Domain Security
- row-level security allow and deny paths
- column-level grant allow and deny paths
- domain masking privileged and unprivileged visibility
- domain encryption allow and deny visibility
- domain-level permission mask behavior

Current code-backed entrypoints:
- `tests/conformance/security/run_security_parity_matrix.sh`
- `tests/conformance/v3_native_inet/sql/13_security_row_level_security.sql`
- `tests/conformance/v3_native_inet/sql/14_security_column_level.sql`
- `tests/conformance/v3_native_inet/sql/15_security_domain_masking.sql`
- `tests/integration/test_domain_security.cpp`
- `tests/integration/test_domain_encryption.cpp`
- `tests/integration/test_domain_e2e_scenarios.cpp`
- `tests/integration/test_security_phase3_3.cpp`
- `tests/integration/test_security_phase3_4_rls.cpp`
- `tests/integration/test_security_phase3_5_rls_dml.cpp`

## Gate Suite T31-SEC-03 Authentication Provider and MFA Behavior
- provider chain ordering and refusal behavior
- fail-closed plugin admission
- method negotiation parity
- MFA challenge, recovery, and enrollment result contract
- external group sync and negotiated capability visibility

Current code-backed entrypoints:
- `tests/unit/test_auth_plugin_admission.cpp`
- `tests/unit/test_auth_plugin_registry_negotiation.cpp`
- `tests/unit/test_auth_policy_protocol_parity.cpp`
- `tests/unit/test_auth_mfa_challenge_flow.cpp`
- `tests/integration/test_auth_plugin_enterprise_matrix.cpp`
- `tests/integration/test_auth_plugin_enterprise_fail_closed.cpp`
- `tests/stress/test_auth_provider_fail_closed_stress.cpp`
- `tests/stress/test_auth_rate_limit_stress.cpp`
- `tests/benchmark/test_auth_plugin_enterprise_perf.cpp`
- `tests/stress/test_auth_plugin_enterprise_soak.cpp`

## Gate Suite T31-SEC-04 Shared Rights, Roles, Groups, and Cluster Secret Partitioning
- user, role, and group resolution
- shared-rights projection and cache invalidation
- cluster secret partitioning, reference, and recovery boundaries
- security-policy epoch and permission-cache epoch publication

Current code-backed entrypoints:
- `tests/unit/test_catalog_manager.cpp`
- `tests/unit/test_domain_control_plane_replication.cpp`
- `tests/integration/test_domain_security.cpp`

Reconstructed required expansion:
- dedicated role, group, shared-rights, and cluster-secret conformance bundles with deterministic output rows

## Gate Suite T31-SEC-05 Manager Proxy and Listener Control
- manager-fronted proxy negotiation
- listener binding and emulation policy control through the engine-admin path
- parser pool sizing and listener status inspection
- inspection privilege distinct from mutation privilege

Current code-backed entrypoints:
- `tests/unit/test_manager_proxy_mcp.cpp`
- `tests/unit/test_connection_manager.cpp`

Reconstructed required expansion:
- deterministic control and status transcript bundles for manager to listener, listener to engine, and engine-admin SQL control

## Gate Suite T31-SEC-06 Remote Management Instruction Lifecycle
- instruction queue insertion
- assessment
- apply
- refuse
- quarantine
- cancel
- acknowledge
- drift and target-generation inspection

Reconstructed required implementation-grade requirement:
- gate bundles must exist for both native admin-SQL and management inspection surfaces
- instruction state transitions must remain transaction-bound and auditable
- inspection results must match the canonical result schema from sections `24`, `26`, `28`, and `30`

## Pass Criteria
1. every current code-backed entrypoint passes with deterministic output identity
2. unsupported surfaces emit explicit refusal or `NA` identity rather than silent omission
3. security-policy epoch and permission-cache invalidation remain commit-bound
4. management inspection never implies management mutation authority
5. reconstructed remote-management gates remain open implementation work only until executable evidence exists, but their required semantics are fixed by canon now

## Cross-Section References
- `19_Security_Model/ROW_COLUMN_DOMAIN_MASKING_AND_SANDBOX_SECURITY_MODEL.md`
- `19_Security_Model/USER_ROLE_GROUP_AND_SHARED_RIGHTS_AUTHORIZATION_MODEL.md`
- `24_Catalog_Model_and_Virtual_Overlays/REMOTE_MANAGEMENT_CATALOG_AND_DEPLOYMENT_RECORDS.md`
- `28_Parser_Implementations/ADMIN_SQL_REMOTE_MANAGEMENT_BINDING_AND_RESULT_CONTRACT.md`
- `30_Client_Tooling/REMOTE_MANAGEMENT_ADMIN_SQL_COMMAND_SURFACE.md`
