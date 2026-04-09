# Section 29 Specification Outline

## Scope

Section `29` specifies the listener and server orchestration surfaces that are
currently shipped in ScratchBird:
- listener profile ingestion from server configuration
- listener and manager-proxy process launch
- listener startup validation
- local control-plane and management IPC
- parser worker pool lifecycle
- client socket handoff to parser workers
- listener observability and operational control

## File map

- `PROCESS_MODEL_AND_DEPENDENCY_GRAPH.md`
  - process ownership, startup topology, and runtime seams
- `CONNECTION_AND_SESSION_LIFECYCLE.md`
  - client accept, parser attach, engine session attach, and detach flow
- `LISTENER_PARSER_HANDOFF_PROTOCOL.md`
  - current control-plane message families and managed-mode preface rules
- `LISTENER_MANAGEMENT_IPC_CHANNEL.md`
  - local admin command set and response contract
- `PARSER_POOL_ASSIGNMENT_AND_SCALING.md`
  - worker states, warm-pool semantics, and runtime resizing
- `LISTENER_CONFIGURATION_PROFILE_SCHEMA.md`
  - current file-backed config keys and launch arguments
- `FAILURE_RECOVERY_AND_FALLBACK.md`
  - startup refusal, drain, reload, and failure response
- `LISTENER_OBSERVABILITY_AND_AUDIT_CONTRACT.md`
  - metrics, audit event emission, and status visibility
- `NORMATIVE_LISTENER_IMPLEMENTATION_CHECKLIST.md`
  - shipped runtime certification checklist
- unsupported-boundary documents:
  - `DUAL_EXECUTION_MIRROR_AND_AUDIT_RUNTIME.md`
  - `MIGRATION_STATE_MACHINE_AND_CUTOVER_GUARANTEE.md`
  - `NORMATIVE_LISTENER_ONE_WAY_AND_BIDIRECTIONAL_REPLICATION_CHECKLIST.md`
  - `NORMATIVE_LISTENER_PASSTHROUGH_AND_LIVE_MIGRATION_CHECKLIST.md`
  - `NORMATIVE_SERVER_CLUSTER_UDR_FABRIC_CHECKLIST.md`

## Non-goals

Section `29` does not define:
- SQL grammar
- SBLR payload semantics
- engine transaction or MGA visibility rules
- schema publication rules
- client driver public API stability

## Current implementation boundary

Shipped current-state authority is limited to the listener and parser-worker
edge backed by:
- `service_controller.cpp`
- `sb_listener_main.cpp`
- `control_plane.h/.cpp`
- `listener_ipc_adapter.cpp`
- `parser_agent.cpp`
- `engine_ipc_session_handler.cpp`

Future-only docs in this section are explicit unsupported-boundary documents and
must not be used as implementation authority for the shipped runtime.
