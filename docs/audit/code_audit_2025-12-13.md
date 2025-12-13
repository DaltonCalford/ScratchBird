# ScratchBird Code Audit (2025-12-13)

Scope: validation of implemented functionality vs. Alpha 3 claims, with emphasis on newly-added protocol work and build/test reality.

## Findings
- **Alpha 3 claims vs. reality:** Public docs (README.md, PROJECT_CONTEXT.md, IMPLEMENTATION_STATUS_DASHBOARD.md) still advertise Alpha 3 completion, enterprise auth, pooling, and 1,337 passing tests; the codebase remains largely Alpha-1/2 with many stubs.
- **Connection pooling still stubbed:** `src/pool/connection_pool.cpp` methods (`connect/execute/validate/close`) are TODOs that only flip flags; no real pooling, health checks, or statement/result caching despite “complete” status.
- **Enterprise auth not implemented:** LDAP/AD and other providers in `src/core/auth_provider.cpp` return `NOT_IMPLEMENTED`; OAuth/OIDC flows in `src/security/oauth_auth.cpp` stub out JWKS/discovery/token introspection and always return `NOT_SUPPORTED`.
- **Dialect compilers still thin:** Adapters now always use their dialect compilers (fallback to `QueryCompilerV2` removed); MySQL/PostgreSQL cover only trivial syntax, and Firebird compilation now stubs in `RDB$DATABASE` to pass semantic checks but lacks broader syntax/DDL coverage.
- **Firebird adapter bridge partially wired:** Adapter uses an IPC client to the native server (Unix socket under `build/`), and transactions/exec/fetch route through the client instead of in-process execution. It now tracks tr_handle → native transaction mappings and parses BLR for basic SQLDA fields, enforces message-length/null-indicator layout, and pads row buffers to declared BLR lengths. Missing pieces: full SQLDA type/charset handling, RDB$ catalog emulation, richer status-vector mapping, and streaming cursors (rows are still fully buffered).
- **IPC paths cleaned to stay inside repo:** `getIPCPath`/`getPIDFilePath` now write sockets/PIDs under `build/`, not `/tmp`, but docs/comments still reference /tmp in several places; sb_server PID handling remains loosely coupled to client-side server-running checks.
- **JDBC/ODBC/FDW claims unverified:** No evidence of runnable integration for these components; tests never exercise them.

## Build/Test Status
- Built from `/home/dcalford/CliWork/ScratchBird/build` with `cmake -S . -B build && cmake --build build`.
- `ctest --output-on-failure` now passes **1341/1341**; Firebird dialect compilation succeeds after bootstrapping a stub `RDB$DATABASE` table in the catalog, but broader dialect fidelity remains unproven.

## Impact
- The project still falls short of Alpha 3: core services (pooling, enterprise auth, wire-protocol fidelity, catalog emulation) are stubbed or faked. Documentation overstates implementation completeness. The Firebird bridge is only a first-hop IPC reroute and needs full BLR/transaction/catalog support before it can meet Firebird client expectations.
