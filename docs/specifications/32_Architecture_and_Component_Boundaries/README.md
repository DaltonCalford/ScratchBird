# Section 32 Architecture and Component Boundaries

Status: current_authority_with_reconstructed_expansion
Current implementation state: partial, fail-closed where proof is not yet section-local.

This section owns the top-level ScratchBird architecture map: process model, engine/listener/client boundaries, subsystem ownership, dependency graph, IPC/internal messaging seams, and bounded extensibility surfaces.

## Section scope

- process model and execution surfaces
- subsystem boundary and ownership matrix
- engine/listener/client relationship
- IPC and internal message surfaces
- extensibility and plugin boundary
- dependency graph and shared ownership

<!-- AUTO-GENERATED:FILE-LIST:START -->
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [DEPENDENCY_GRAPH_AND_SHARED_OWNERSHIP.md](DEPENDENCY_GRAPH_AND_SHARED_OWNERSHIP.md)
- [EMBEDDED_DIRECT_ENGINE_PARSER_IPC_AND_STACK_DEPLOYMENT_MODEL.md](EMBEDDED_DIRECT_ENGINE_PARSER_IPC_AND_STACK_DEPLOYMENT_MODEL.md)
- [ENGINE_LIBRARY_SERVER_PROCESS_AND_LAYERED_DEPLOYMENT_MODEL.md](ENGINE_LIBRARY_SERVER_PROCESS_AND_LAYERED_DEPLOYMENT_MODEL.md)
- [ENGINE_LISTENER_CLIENT_RELATIONSHIP.md](ENGINE_LISTENER_CLIENT_RELATIONSHIP.md)
- [EXTENSIBILITY_AND_PLUGIN_BOUNDARY.md](EXTENSIBILITY_AND_PLUGIN_BOUNDARY.md)
- [IPC_AND_INTERNAL_MESSAGE_SURFACES.md](IPC_AND_INTERNAL_MESSAGE_SURFACES.md)
- [LIBRARY_COMPOSITION_EXECUTABLE_VARIANTS_AND_PACKAGING_MODEL.md](LIBRARY_COMPOSITION_EXECUTABLE_VARIANTS_AND_PACKAGING_MODEL.md)
- [PROCESS_MODEL_AND_EXECUTION_SURFACES.md](PROCESS_MODEL_AND_EXECUTION_SURFACES.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [SUBSYSTEM_BOUNDARY_AND_OWNERSHIP_MATRIX.md](SUBSYSTEM_BOUNDARY_AND_OWNERSHIP_MATRIX.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
- [THREADED_IPC_SERVER_AND_ENGINE_LIBRARY_BOUNDARY_MODEL.md](THREADED_IPC_SERVER_AND_ENGINE_LIBRARY_BOUNDARY_MODEL.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->

## Audit lookup anchors

Representative section-32 audit anchors are:
- `IPCServer::acceptLoop(`
- `manager_proxy.internal_native_port`
- `seedBootstrapListenerTopology(`
