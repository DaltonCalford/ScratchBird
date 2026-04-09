# Section 29 Dependencies

## Direct code dependencies

Current section `29` runtime depends directly on:
- `service_controller.cpp`
- `sb_listener_main.cpp`
- `control_plane.h`
- `control_plane.cpp`
- `listener_ipc_adapter.cpp`
- `parser_agent.cpp`
- `engine_ipc_session_handler.cpp`

## Canonical spec dependencies

Section `29` depends on:
- section `08`
  - transaction lifecycle and autocommit meaning
- section `09`
  - lock and conflict consequences during active sessions
- section `24`
  - catalog session creation and metadata helper contracts
- section `28`
  - parser ownership and SQL-to-engine translation
- section `30`
  - protocol family behavior at the client edge
- section `31`
  - gate and conformance expectations
- section `32`
  - architecture and ownership boundaries
- section `35`
  - startup recovery and durability posture
- section `37`
  - schema publication and metadata visibility

## Parser metadata helper dependency

Parser families depend on the catalog helper contract owned outside section
`29`:
- `sb_catalog_resolve_name_to_uuid`
- `sb_catalog_resolve_uuid_to_path_name`
- `sb_catalog_snapshot_begin`
- `sb_catalog_delta_since_anchor`

The listener runtime does not implement those helpers. It only hosts parser
workers that depend on them.

## Explicit non-dependencies

Section `29` does not currently depend on:
- live migration runtime
- replication apply runtime
- parserless cluster fabric runtime
- catalog-owned listener profile publication
