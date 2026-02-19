# ScratchBird Feature Catalog (0.1.0 Baseline)

This catalog separates implemented capabilities from partial/planned scope.

## Implemented Feature Families

### Engine Core

- Firebird-style MGA visibility and transaction core.
- Catalog/bootstrap and UUID-based object identity.
- Storage page management, index/storage integration, and GC/sweep.
- Security primitives and protocol/auth integration points.

### Parser and Execution

- Native parser (v3 path) compiling SQL into SBLR.
- SBLR container decode/dispatch execution path.
- Deterministic error contracts for parser and selected executor families.

### Network, Listener, and Parser Agents

- Server binary + dedicated listener and parser agent binaries by protocol.
- Listener startup orchestration with:
  - port bind collision checks
  - database-owned listener routing
  - parser/engine endpoint bootstrap metadata
- Native, PostgreSQL, MySQL, Firebird protocol listener/parser surfaces.

### Tooling and Packaging

- Clean build/test workflow with full ctest coverage.
- Beta artifact staging:
  - runtime package
  - QA package
  - full tarball archive

## Implemented Feature Evidence

- Build targets: `src/CMakeLists.txt`
- Listener orchestration: `src/server/service_controller.cpp`
- Listener runtime: `src/network/sb_listener_main.cpp`
- Parser runtime: `src/parser/parser_v3.cpp`
- Executor runtime: `src/sblr/executor.cpp`
- Listener bootstrap tests: `tests/unit/test_service_controller_listener_bootstrap.cpp`
- UDR SQL render endpoint tests:
  `tests/unit/test_language_udr_sblr_sql_render_endpoint_contract.cpp`

## Partial / Planned Feature Families

### 0.2.0 Required Tracks

- Full detailed specs and workplans for all remaining partial/planned items.
- Catalog refactor/optimization and validation.
- Emulation parser parity completion with source-engine suite gates.
- Native parser normalization for dialect-style consistency.
- Post-refactor driver compatibility confirmation.
- Cross-engine performance benchmarking on identical hardware/OS.
- Go/no-go/redesign gates based on measured benchmark results.
- Installer bundles alongside release packages.

## Scope Boundary

Archive material under legacy/alpha directories is retained for history, but is
not treated as active capability evidence unless revalidated in the 0.1.0+
baseline.
