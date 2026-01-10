# Beta Tasks (Summary & Priority)

**Reference:** `docs/archive/2026-01-04/planning/Beta_Phase_0_Implementation_Plan.md` for packaging/tooling specifics. This file consolidates Beta-focused work items.

## Priority P0
- Packaging & builds (from Beta Phase 0): cross-compilation (Linux/Windows), CPack targets (DEB/RPM/TGZ/NSIS/ZIP/AppImage/Docker), build profiles, toolchain files.  
- Client library & tools: finalize `libscratchbird_client`, refactor sb_* tools to use it, dual network/embedded modes.  
- Admin/GUI specs: `CLIENT_LIBRARY_API_SPECIFICATION.md`, `SB_ADMIN_CLI_SPECIFICATION.md`, FlameRobin rebrand/spec + packaging.  
- End-user docs: structured doc set and wiki automation per plan.

## Priority P1
- WAL for ETL/replication/logging (not recovery); define format, retention, and export hooks.  
- Monitoring/maintenance workers: background sweep/analyze/index maintenance/resource monitors; metrics surfaces; per-component controls.  
- Logging/telemetry: query/audit logging scope, redaction, metrics endpoints/access control, rotation/retention (tie to monitoring).  
- Scheduler: cluster-aware mode (leader/lease) building on Alpha scheduler; job visibility/history.  
- CDC/ETL integration: expose WAL/CDC streams for downstream ETL; table opt-in and security.

## Priority P2
- Replication/clustering/sharding/load balancing: research & full spec; topology, failover, routing.  
- Optimizer/statistics deep dive: use of per-index stats in cost model; adaptive/replan behavior.  
- Checkpoint policy refinement in Beta context (interaction with WAL-for-ETL).  
- Advanced telemetry/export: hardened metrics endpoints, dashboards.

## Notes
- Keep Beta work aligned with Alpha constraints (MGA semantics, emulated engine sandboxes).  
- Treat WAL strictly as ETL/replication/logging in Beta; recovery remains MGA-based.  
