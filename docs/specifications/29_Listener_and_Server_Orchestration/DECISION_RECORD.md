# Section 29 Decision Record

## Decision 1: Listener runtime is process-based

Current ScratchBird listener orchestration is process-based:
- one `ServiceController`
- one or more listener processes
- one parser worker pool per listener
- one engine IPC session bridge behind the parser workers

Thread-only or in-engine listener ownership is not the section `29` model.

## Decision 2: File-backed config is current authority

Listener configuration is currently sourced from file-backed config and launch
arguments.

Catalog-authoritative listener configuration is not current runtime authority.

## Decision 3: Control plane is bounded internal runtime surface

The listener/parser control plane is a bounded internal contract. It is not a
public protocol family contract and is not a stable extension ABI.

## Decision 4: Managed mode is the DBBT/LPREFACE path

Managed mode is the current manager-proxy path and requires:
- loopback bind
- `DBBT`
- `LPREFACE`
- replay and listener-id validation

## Decision 5: Current shipped listener families are finite

Current shipped section `29` listener family proof is limited to:
- `native`
- `postgresql`
- `mysql`
- `firebird`

## Decision 6: SQL text stays out of the engine IPC bridge

SQL text ownership stays with parser workers. The engine IPC session handler is
the execution bridge for prepared engine payloads and session state.

## Decision 7: Future migration and fabric documents are unsupported boundaries

Dual execution, migration, replication runtime, and parserless cluster fabric
documents remain explicit unsupported-boundary documents in the current spec
tree. They are not shipped section `29` authority.
