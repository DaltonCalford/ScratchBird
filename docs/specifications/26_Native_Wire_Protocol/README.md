# Section 26 Native Wire Protocol

Status: current_authority

Section `26` is the canonical current-state authority for ScratchBird-native
wire framing and the current internal IPC transport contract used between parser
agents and the engine.

Current active authority in this section covers:
- native `SBDB` wire header and message-family contract
- internal `SBIP` IPC header and message-family contract
- current result, error, and completion frame mapping
- current native and IPC copy or stream message families where they are
  explicitly declared and implemented

This section is not the authority for:
- listener orchestration
- handshake policy details
- parser SQL ownership
- replay session transport
- parserless cluster-fabric transport
- distributed-read and telemetry transport expansion

Those deeper or adjacent surfaces are owned by sections `27`, `28`, `29`,
`19`, and `17`.

Unsupported or future-only transport lanes in this section are explicit
unsupported-boundary documents only. They are not planning surfaces, and they
must not be treated as current implementation authority.

<!-- AUTO-GENERATED:FILE-LIST:START -->
- [AUTH_CHALLENGE_REGISTRY_NEGOTIATION_AND_CHALLENGE_PAYLOAD_MODEL.md](AUTH_CHALLENGE_REGISTRY_NEGOTIATION_AND_CHALLENGE_PAYLOAD_MODEL.md)
- [AUTH_PLUGIN_REGISTRY_CHALLENGE_AND_METHOD_NEGOTIATION_MODEL.md](AUTH_PLUGIN_REGISTRY_CHALLENGE_AND_METHOD_NEGOTIATION_MODEL.md)
- [CLUSTER_UDR_FABRIC_CHANNEL_SPEC.md](CLUSTER_UDR_FABRIC_CHANNEL_SPEC.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [FORENSIC_REPLAY_SESSION_PROFILE.md](FORENSIC_REPLAY_SESSION_PROFILE.md)
- [IPC_SBWP_FRAME_SPEC.md](IPC_SBWP_FRAME_SPEC.md)
- [LOCAL_IPC_LIBRARY_AND_NON_IP_CONNECTION_MODEL.md](LOCAL_IPC_LIBRARY_AND_NON_IP_CONNECTION_MODEL.md)
- [LOCAL_IPC_REQUEST_FRAMING_SESSION_LIFECYCLE_AND_NO_IP_GUARANTEE_MODEL.md](LOCAL_IPC_REQUEST_FRAMING_SESSION_LIFECYCLE_AND_NO_IP_GUARANTEE_MODEL.md)
- [LOCAL_ONLY_IPC_STACK_SESSION_AND_ENDPOINT_IDENTITY_MODEL.md](LOCAL_ONLY_IPC_STACK_SESSION_AND_ENDPOINT_IDENTITY_MODEL.md)
- [MCP_CLUSTER_HEARTBEAT_AND_REMOTE_DRIFT_RESULT_MODEL.md](MCP_CLUSTER_HEARTBEAT_AND_REMOTE_DRIFT_RESULT_MODEL.md)
- [MCP_MANAGER_CONTROL_AND_DATABASE_BINDING_MODEL.md](MCP_MANAGER_CONTROL_AND_DATABASE_BINDING_MODEL.md)
- [MESSAGE_CATALOG_AND_SCHEMAS.md](MESSAGE_CATALOG_AND_SCHEMAS.md)
- [NORMATIVE_P1_WIRE_DISTRIBUTED_READ_AND_TELEMETRY_CHECKLIST.md](NORMATIVE_P1_WIRE_DISTRIBUTED_READ_AND_TELEMETRY_CHECKLIST.md)
- [OPTIMIZER_ACCELERATOR_AND_RUNTIME_STATUS_PAYLOADS.md](OPTIMIZER_ACCELERATOR_AND_RUNTIME_STATUS_PAYLOADS.md)
- [PROTOCOL_STATE_MACHINES.md](PROTOCOL_STATE_MACHINES.md)
- [REMOTE_MANAGEMENT_STATUS_AND_CONTROL_PAYLOADS.md](REMOTE_MANAGEMENT_STATUS_AND_CONTROL_PAYLOADS.md)
- [RESULT_SHAPES_AND_ERROR_FRAMES.md](RESULT_SHAPES_AND_ERROR_FRAMES.md)
- [SECURITY_QUORUM_CLUSTER_SECRET_AND_MFA_INSPECTION_RESULT_MODEL.md](SECURITY_QUORUM_CLUSTER_SECRET_AND_MFA_INSPECTION_RESULT_MODEL.md)
- [SERVICE_CHANNELS_AND_STREAMING.md](SERVICE_CHANNELS_AND_STREAMING.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
- [TRANSACTION_REATTACH_HANDLE_AND_SESSION_REBIND_MODEL.md](TRANSACTION_REATTACH_HANDLE_AND_SESSION_REBIND_MODEL.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->

## Audit lookup anchors

Representative section-26 audit anchors are:
- `IPCServer::setupListener(`
- `setSessionContext(`
- `MCP_DB_CONNECT`
- `startup_quarantine_active`
