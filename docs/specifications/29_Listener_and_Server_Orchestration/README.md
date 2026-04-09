# Section 29 - Listener and Server Orchestration

Section `29` is the canonical authority for the shipped ScratchBird listener and
server orchestration edge.

This section is implementation authority for:
- `ServiceController` launch and ownership of listener processes
- file-backed listener profile loading and manager-proxy bootstrap
- listener process startup, front-door bind, warm-pool gating, and drain policy
- local control-plane framing between listener and parser workers
- local management socket commands and response format
- parser worker pool ownership at the listener edge
- listener-side metrics and managed-mode audit events

This section is not the authority for:
- SQL parsing rules or SBLR emission
- engine execution semantics
- transaction semantics
- catalog publication semantics
- protocol compatibility truth

Those surfaces are owned by sections `08`, `22`, `23`, `24`, `28`, `30`, and
`31`.

Current shipped listener families are:
- `native`
- `postgresql`
- `mysql`
- `firebird`

Current runtime modes are:
- `direct`
- `managed`

`managed` mode is the current manager-proxy path. It requires loopback bind and
uses `DBBT` plus `LPREFACE` validation for the listener front door. Current
`ServiceController` launch wiring applies this path to the native listener lane.

Unsupported in current section `29` runtime:
- live migration
- dual execution mirror
- one-way or bidirectional replication runtime
- parserless cluster fabric
- universal public plugin or extension ABI for the control plane

The documents in this section are written as current shipped authority or as
explicit unsupported-boundary documents only. They are not planning surfaces.

## File Index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [CLOUD_FRONT_DOOR_SERVICE_TOPOLOGY_AND_ROLLING_RESTART_MODEL.md](CLOUD_FRONT_DOOR_SERVICE_TOPOLOGY_AND_ROLLING_RESTART_MODEL.md)
- [CONNECTION_AND_SESSION_LIFECYCLE.md](CONNECTION_AND_SESSION_LIFECYCLE.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [DUAL_EXECUTION_MIRROR_AND_AUDIT_RUNTIME.md](DUAL_EXECUTION_MIRROR_AND_AUDIT_RUNTIME.md)
- [ENGINE_ADMIN_LISTENER_CONTROL_AND_CONFIGURATION_PROPAGATION_MODEL.md](ENGINE_ADMIN_LISTENER_CONTROL_AND_CONFIGURATION_PROPAGATION_MODEL.md)
- [FAILURE_RECOVERY_AND_FALLBACK.md](FAILURE_RECOVERY_AND_FALLBACK.md)
- [LISTENER_CONFIGURATION_PROFILE_SCHEMA.md](LISTENER_CONFIGURATION_PROFILE_SCHEMA.md)
- [LISTENER_MANAGEMENT_IPC_CHANNEL.md](LISTENER_MANAGEMENT_IPC_CHANNEL.md)
- [LISTENER_OBSERVABILITY_AND_AUDIT_CONTRACT.md](LISTENER_OBSERVABILITY_AND_AUDIT_CONTRACT.md)
- [LISTENER_PARSER_AGENT_POOL_AND_IP_HANDOFF_MODEL.md](LISTENER_PARSER_AGENT_POOL_AND_IP_HANDOFF_MODEL.md)
- [LISTENER_PARSER_HANDOFF_PROTOCOL.md](LISTENER_PARSER_HANDOFF_PROTOCOL.md)
- [MANAGER_PROXY_MCP_DBBT_AND_LPREFACE_CONNECT_MODEL.md](MANAGER_PROXY_MCP_DBBT_AND_LPREFACE_CONNECT_MODEL.md)
- [MIGRATION_ORCHESTRATION_INSPECTION_AND_HEARTBEAT_BOUNDARY.md](MIGRATION_ORCHESTRATION_INSPECTION_AND_HEARTBEAT_BOUNDARY.md)
- [MIGRATION_STATE_MACHINE_AND_CUTOVER_GUARANTEE.md](MIGRATION_STATE_MACHINE_AND_CUTOVER_GUARANTEE.md)
- [NORMATIVE_LISTENER_IMPLEMENTATION_CHECKLIST.md](NORMATIVE_LISTENER_IMPLEMENTATION_CHECKLIST.md)
- [NORMATIVE_LISTENER_ONE_WAY_AND_BIDIRECTIONAL_REPLICATION_CHECKLIST.md](NORMATIVE_LISTENER_ONE_WAY_AND_BIDIRECTIONAL_REPLICATION_CHECKLIST.md)
- [NORMATIVE_LISTENER_PASSTHROUGH_AND_LIVE_MIGRATION_CHECKLIST.md](NORMATIVE_LISTENER_PASSTHROUGH_AND_LIVE_MIGRATION_CHECKLIST.md)
- [NORMATIVE_SERVER_CLUSTER_UDR_FABRIC_CHECKLIST.md](NORMATIVE_SERVER_CLUSTER_UDR_FABRIC_CHECKLIST.md)
- [PARSER_AGENT_EXECUTABLE_COMPOSITION_AND_RUNTIME_STACK_MODEL.md](PARSER_AGENT_EXECUTABLE_COMPOSITION_AND_RUNTIME_STACK_MODEL.md)
- [PARSER_POOL_ASSIGNMENT_AND_SCALING.md](PARSER_POOL_ASSIGNMENT_AND_SCALING.md)
- [PROCESS_MODEL_AND_DEPENDENCY_GRAPH.md](PROCESS_MODEL_AND_DEPENDENCY_GRAPH.md)
- [PROXY_CAPTURE_COVERAGE_GAP_AND_SESSION_FENCE_MODEL.md](PROXY_CAPTURE_COVERAGE_GAP_AND_SESSION_FENCE_MODEL.md)
- [PROXY_MIGRATION_FOR_NON_REPLICATING_DONORS_MODEL.md](PROXY_MIGRATION_FOR_NON_REPLICATING_DONORS_MODEL.md)
- [PROXY_MIGRATION_SESSION_CLASSIFICATION_AND_CUTOVER_GATING_MODEL.md](PROXY_MIGRATION_SESSION_CLASSIFICATION_AND_CUTOVER_GATING_MODEL.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->

## Maintenance
- Update file list with `../skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`.

## Audit lookup anchors

Representative section-29 audit anchors are:
- `buildManagementResponsePayload(`
- `waitForWarm(`
- `sendListenerManagementCommand(`
