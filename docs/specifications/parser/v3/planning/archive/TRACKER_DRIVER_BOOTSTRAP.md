# Tracker: Driver Bootstrap (Alpha)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Status: Complete
Last Updated: 2026-01-19

## Scope
Define and scaffold driver bootstrap for all required Alpha drivers.
This includes ODBC/JDBC/native drivers and the tooling network paths
needed to validate connections through parser agents.

## Specifications
- /docs/specifications/parser/v3/drivers/ALPHA_DRIVER_BOOTSTRAP.md
- /docs/specifications/parser/v3/tools/SB_TOOLING_NETWORK_SPEC.md
- /docs/specifications/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md
- /docs/specifications/parser/v3/admin/SB_SERVER_NETWORK_CLI_SPECIFICATION.md

## Deliverables
- Driver bootstrap checklist and build wiring (per ALPHA_DRIVER_BOOTSTRAP)
- Driver connection paths through parser agents
- CLI tools able to connect via network endpoints

## Work Items
1) Confirm per-driver build targets and visibility in CMake. (done)
2) Define driver connection parameters in config/catalog. (done)
3) Wire driver connection paths to parser agents (native protocol). (done)
4) Add smoke tests for driver connectivity (non-TCP test gating as needed). (done)

## Notes
- Detailed driver implementation work will follow once listener/parser
  binaries and control-plane socket are in place.
- CMake exposes `scratchbird_client`, `libscratchbird`, and `scratchbird_odbc`
  libraries plus `sb_isql` for native connectivity; JDBC targets not yet wired.
- Driver defaults now live in server config ([drivers] section) and are
  parsed by `ServiceConfig`.
- libscratchbird honors `SCRATCHBIRD_DRIVER_*` env defaults (host, port,
  sslmode, timeouts, application name) for native driver connections.
- Added driver connectivity smoke test (`DriverConnectivitySmokeTest`) and
  env-defaults coverage in unit tests.
