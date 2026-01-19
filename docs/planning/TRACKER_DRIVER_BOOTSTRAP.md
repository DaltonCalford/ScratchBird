# Tracker: Driver Bootstrap (Alpha)

Status: Not started
Last Updated: 2026-01-18

## Scope
Define and scaffold driver bootstrap for all required Alpha drivers.
This includes ODBC/JDBC/native drivers and the tooling network paths
needed to validate connections through parser agents.

## Specifications
- docs/specifications/drivers/ALPHA_DRIVER_BOOTSTRAP.md
- docs/specifications/tools/SB_TOOLING_NETWORK_SPEC.md
- docs/specifications/network/ENGINE_PARSER_IPC_CONTRACT.md
- docs/specifications/admin/SB_SERVER_NETWORK_CLI_SPECIFICATION.md

## Deliverables
- Driver bootstrap checklist and build wiring (per ALPHA_DRIVER_BOOTSTRAP)
- Driver connection paths through parser agents
- CLI tools able to connect via network endpoints

## Work Items
1) Confirm per-driver build targets and visibility in CMake.
2) Define driver connection parameters in config/catalog.
3) Wire driver connection paths to parser agents (native protocol).
4) Add smoke tests for driver connectivity (non-TCP test gating as needed).

## Notes
- Detailed driver implementation work will follow once listener/parser
  binaries and control-plane socket are in place.
