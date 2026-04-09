# Definitive Specset Index

## Assigned Section Entry Files

- docs/specifications/25_Runtime_Modes/README.md
- docs/specifications/25_Runtime_Modes/MANAGER_HEARTBEAT_AND_SERVER_AGENT_MODEL.md
- docs/specifications/26_Native_Wire_Protocol/README.md
- docs/specifications/27_Native_Handshake/README.md
- docs/specifications/29_Listener_and_Server_Orchestration/README.md
- docs/specifications/32_Architecture_and_Component_Boundaries/README.md

## Required Consumed Canon Within Assigned Scope

- docs/specifications/25_Runtime_Modes/EMBEDDED_DIRECT_ENGINE_AND_LOCAL_SHARED_SERVER_RUNTIME_SELECTION_MODEL.md
- docs/specifications/25_Runtime_Modes/MANAGER_DBBT_LPREFACE_AND_MCP_CONTROL_PLANE_MODEL.md
- docs/specifications/25_Runtime_Modes/MANAGER_HEARTBEAT_PUBLICATION_AND_REMOTE_DRIFT_RUNTIME_MODEL.md
- docs/specifications/25_Runtime_Modes/REMOTE_MANAGEMENT_INSTRUCTION_STATE_MACHINE.md
- docs/specifications/26_Native_Wire_Protocol/LOCAL_IPC_LIBRARY_AND_NON_IP_CONNECTION_MODEL.md
- docs/specifications/26_Native_Wire_Protocol/LOCAL_IPC_REQUEST_FRAMING_SESSION_LIFECYCLE_AND_NO_IP_GUARANTEE_MODEL.md
- docs/specifications/26_Native_Wire_Protocol/LOCAL_ONLY_IPC_STACK_SESSION_AND_ENDPOINT_IDENTITY_MODEL.md
- docs/specifications/26_Native_Wire_Protocol/MCP_CLUSTER_HEARTBEAT_AND_REMOTE_DRIFT_RESULT_MODEL.md
- docs/specifications/26_Native_Wire_Protocol/MCP_MANAGER_CONTROL_AND_DATABASE_BINDING_MODEL.md
- docs/specifications/26_Native_Wire_Protocol/REMOTE_MANAGEMENT_STATUS_AND_CONTROL_PAYLOADS.md
- docs/specifications/27_Native_Handshake/AUTH_NEGOTIATION_AND_POLICY.md
- docs/specifications/27_Native_Handshake/HANDSHAKE_MESSAGE_SCHEMAS.md
- docs/specifications/27_Native_Handshake/HANDSHAKE_STATE_MACHINE_AND_FAILURE_MATRIX.md
- docs/specifications/29_Listener_and_Server_Orchestration/ENGINE_ADMIN_LISTENER_CONTROL_AND_CONFIGURATION_PROPAGATION_MODEL.md
- docs/specifications/29_Listener_and_Server_Orchestration/FAILURE_RECOVERY_AND_FALLBACK.md
- docs/specifications/29_Listener_and_Server_Orchestration/LISTENER_MANAGEMENT_IPC_CHANNEL.md
- docs/specifications/29_Listener_and_Server_Orchestration/PARSER_POOL_ASSIGNMENT_AND_SCALING.md
- docs/specifications/32_Architecture_and_Component_Boundaries/EMBEDDED_DIRECT_ENGINE_PARSER_IPC_AND_STACK_DEPLOYMENT_MODEL.md
- docs/specifications/32_Architecture_and_Component_Boundaries/ENGINE_LIBRARY_SERVER_PROCESS_AND_LAYERED_DEPLOYMENT_MODEL.md
- docs/specifications/32_Architecture_and_Component_Boundaries/THREADED_IPC_SERVER_AND_ENGINE_LIBRARY_BOUNDARY_MODEL.md

## Required Consumed Cross-Section Canonical Specs

- docs/specifications/24_Catalog_Model_and_Virtual_Overlays/LISTENER_TOPOLOGY_PARSER_POOL_AND_EMULATION_BINDING_CATALOG_MODEL.md
- docs/specifications/24_Catalog_Model_and_Virtual_Overlays/REMOTE_MANAGEMENT_CATALOG_AND_DEPLOYMENT_RECORDS.md
- docs/specifications/24_Catalog_Model_and_Virtual_Overlays/REMOTE_MANAGEMENT_LOCAL_AND_CLUSTER_PERSISTENCE_CONSISTENCY_MODEL.md
- docs/specifications/30_Client_Tooling/REMOTE_ADMIN_AND_DEPLOYMENT_CONTROL_SURFACE.md
- docs/specifications/30_Client_Tooling/REMOTE_MANAGEMENT_ADMIN_SQL_COMMAND_SURFACE.md

## Scope Notes

- section `24` is consumed for catalog-backed listener-topology and
  remote-management persistence authority only
- section `30` is consumed to fix the operator-facing inspection and mutation
  command names; package `05` still owns the underlying service-stack control
  seam rather than the client-tool grammar
- Windows listener-management IPC remains an explicit current limitation rather
  than undefined behavior

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
