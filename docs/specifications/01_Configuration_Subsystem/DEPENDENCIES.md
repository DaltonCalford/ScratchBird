# Dependencies

## Primary code authorities

- include/scratchbird/core/config.h
- src/core/config.cpp
- include/scratchbird/server/config_parser.h
- src/server/config_parser.cpp
- include/scratchbird/server/service_controller.h
- src/server/service_controller.cpp
- src/network/sb_listener_main.cpp
- src/core/cluster_write_safety.cpp
- src/core/database.cpp

## Upstream specification dependencies

- section 00 for governance and section ownership
- section 08 for always-in-transaction rules and transaction-setting boundaries
- section 24 for scalar configuration rows, dedicated listener-topology rows,
  and remote-management deployment records
- section 37 for metadata publication rules where configuration-admin surfaces
  mutate catalog-managed state
- section 29 for listener and service orchestration surfaces that consume configuration

## Downstream consumers

Current configuration claims materially affect:
- service bootstrap and listener orchestration
- runtime modes
- transaction monitoring and sweep or gc consumers
- observability and logging consumers
- conformance and reliability gate surfaces

## Explicit dependency boundary

This section depends on file-backed bootstrap code plus the catalog-backed
configuration row families that become authoritative after mount.

It does not permit manager or listener transport to become the durable source
of configuration truth.
