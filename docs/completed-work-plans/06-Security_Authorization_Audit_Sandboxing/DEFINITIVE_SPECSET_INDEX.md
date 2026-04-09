# Definitive Specset Index

## Assigned Sections

- docs/specifications/19_Security_Model/README.md
- docs/specifications/19_Security_Model/AUDIT_AND_FORENSIC_ACCESS_POLICY.md
- docs/specifications/19_Security_Model/AUTH_PLUGIN_METHOD_SPECIFICATIONS.md
- docs/specifications/19_Security_Model/AUTH_PROVIDER_CHAIN_AND_MFA_POLICY_RUNTIME_MODEL.md
- docs/specifications/19_Security_Model/CLOUD_SECRET_CERT_ROTATION_AND_SIGNED_ARTIFACT_DELIVERY_MODEL.md
- docs/specifications/20_Diagnostics_Audit_and_Observability/README.md
- docs/specifications/20_Diagnostics_Audit_and_Observability/MGA_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md

## Bounded Beta 1 subset

- local-engine and single-node service security only
- admitted auth families are password or compatibility password, SCRAM, token
  or authkey, peer, certificate mTLS, manager-control token flow, and MFA
  overlays on top of those families
- cluster-shared identity propagation, quorum secret runtime, and distributed
  trust rollout remain explicit Beta 2 or fail-closed boundaries for package
  `06`
- observability vocabulary is normalized on `OIT`, `OAT`, and `OST`

## Required consumed canonical specs

- docs/specifications/19_Security_Model/AUTH_PLUGIN_ADMISSION_HBA_AND_RUNTIME_AUTH_COORDINATION_MODEL.md
- docs/specifications/19_Security_Model/AUTH_PLUGIN_ARCHITECTURE_AND_SIGNED_MODULE_ABI.md
- docs/specifications/19_Security_Model/ENGINE_AUTHENTICATION_HARDENING_AND_MANAGER_OPTION_SPEC.md
- docs/specifications/19_Security_Model/ENCRYPTION_AND_KEY_MANAGEMENT.md
- docs/specifications/19_Security_Model/KEY_STORAGE_AND_CERTIFICATES.md
- docs/specifications/19_Security_Model/PKI_LIFECYCLE_CLUSTER_CHANNELS.md
- docs/specifications/19_Security_Model/PRIVILEGE_GRAPH_ROW_COLUMN_DOMAIN_SECURITY_AND_MASKING_MODEL.md
- docs/specifications/19_Security_Model/ROW_COLUMN_DOMAIN_MASKING_AND_SANDBOX_SECURITY_MODEL.md
- docs/specifications/19_Security_Model/SECURITY_DEFINER_INVOKER_AND_EMULATED_SCHEMA_SANDBOX_MODEL.md
- docs/specifications/19_Security_Model/TEST_CONTRACT.md
- docs/specifications/19_Security_Model/USER_ROLE_GROUP_AND_SHARED_RIGHTS_AUTHORIZATION_MODEL.md
- docs/specifications/20_Diagnostics_Audit_and_Observability/AUDIT_EXPORT_SINKS_RETENTION_AND_IMMUTABILITY.md
- docs/specifications/20_Diagnostics_Audit_and_Observability/SECURE_DIAGNOSTIC_REDACTION_FIELD_AND_TEXT_MODEL.md
- docs/specifications/20_Diagnostics_Audit_and_Observability/STORAGE_METRICS.md
- docs/specifications/20_Diagnostics_Audit_and_Observability/SUPPORT_BUNDLE_READINESS_REDACTION_AND_OPERATIONAL_EVIDENCE_MODEL.md
- docs/specifications/20_Diagnostics_Audit_and_Observability/TEST_CONTRACT.md
- docs/specifications/24_Catalog_Model_and_Virtual_Overlays/IDENTITY_AUTH_PROVIDER_AND_MFA_SECURITY_CATALOG_MODEL.md
- docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SECURITY_POLICY_PRIVILEGE_MASKING_AND_SHARED_RIGHT_ROW_FAMILY_MODEL.md
- docs/specifications/24_Catalog_Model_and_Virtual_Overlays/WRAPPER_OBJECT_SECURITY_DEFINER_AND_SANDBOX_CATALOG_MODEL.md
- docs/specifications/25_Runtime_Modes/CLOUD_SUPPORT_SCOPE_AND_BETA1_BETA2_PROGRAM_MODEL.md
- docs/specifications/26_Native_Wire_Protocol/LOCAL_ONLY_IPC_STACK_SESSION_AND_ENDPOINT_IDENTITY_MODEL.md
- docs/specifications/27_Native_Handshake/AUTH_NEGOTIATION_AND_POLICY.md
- docs/specifications/29_Listener_and_Server_Orchestration/ENGINE_ADMIN_LISTENER_CONTROL_AND_CONFIGURATION_PROPAGATION_MODEL.md
- docs/specifications/31_Conformance_Performance_and_Reliability_Gates/AUTH_PLUGIN_ENTERPRISE_PERFORMANCE_AND_SOAK_GATE_MODEL.md

## Global Governance Inputs

- docs/specifications/00_Governance_and_Invarients/WORK_PLAN_MANAGEMENT_STANDARD_AND_LIFECYCLE.md
- docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md
- docs/completed-work-plans/00-Beta1_Tasks/README.md
- docs/completed-work-plans/00-Beta1_Tasks/WORKPLAN_GENERATION_INPUT.md

## Required Research Order

1. assigned canonical specs
2. consumed cross-section canonical specs
3. docs/reference local authority tree
4. web research when local authority is insufficient
