# ScratchBird Code Audit (2025-12-12)

Scope: reality check of implemented functionality vs documentation/roadmaps. Focus on unimplemented or stubbed areas that contradict the claimed Alpha 3 completion.

## Top-Level Gaps
- Project version is still `v0.1.0-alpha.1.01`, not Alpha 3 (`include/scratchbird/version.h:4-9`).
- Public docs claim 1,337 passing tests and Alpha 3 completion (`README.md`, `docs/IMPLEMENTATION_STATUS_DASHBOARD.md`, `PROJECT_CONTEXT.md`), but there is no runnable suite: `rg "TEST(" tests` returns 0 and the orchestrator has stubbed DB lifecycle methods (`src/testing/TestRunner.cpp:640-679`).
- CLI functionality is limited to creating/opening a file and a `.info` REPL; no SQL execution (`src/main.cpp`).

## Server & Wire Protocols
- All protocol adapters funnel queries through `ProtocolAdapter::executeQuery`, which is a canned responder that never touches the storage engine (returns hard-coded tags for SELECT/INSERT/UPDATE/DELETE/etc.). No planner/executor integration despite documentation claims of full wire compatibility (`src/protocol/adapters/protocol_adapter.cpp:128-200`).
- The native/MySQL/PostgreSQL/Firebird adapters therefore cannot serve real queries; wire-level behavior is limited to handshake/parsing with dummy results.

## Connection Pooling
- Pool connections do not actually connect anywhere. `PooledConnection::connect/execute/close/validate` are all TODO stubs that simply flip state and increment counters (`src/pool/connection_pool.cpp:154-227`), so the advertised pool/statement/result caching is non-functional.

## Authentication & Security
- LDAP and Active Directory providers are explicit stubs that log warnings and always fail with `NOT_IMPLEMENTED` (`src/core/auth_provider.cpp:248-330`). Documentation claims AD/LDAP support is complete.
- OAuth/OIDC code omits all required network/JWKS plumbing: introspection, JWKS fetch, OIDC discovery, and userinfo calls are stubbed and return `NOT_SUPPORTED` (`src/security/oauth_auth.cpp:221-243`, `394-399`, `427-435`, `480-485`, `720-725`). Tokens are effectively unchecked beyond local parsing, contradicting the “Enterprise Security Suite” claims.

## Testing Infrastructure
- TestRunner database setup/teardown is stubbed (always returns true) and generates reports regardless of execution (`src/testing/TestRunner.cpp:640-679`).
- ProtocolTester marks large swaths of coverage as “SKIPPED” stubs (prepared statements, parameter binding, COPY, etc.) (`src/testing/ProtocolTester.cpp:684-709`, `1153-1168`), so the supposed protocol compliance coverage is absent.

## Additional Observations
- Many NOT_IMPLEMENTED paths remain across core subsystems (catalog, domain manager, storage engine, timezone loader, GIN, etc. surfaced by `rg "NOT_IMPLEMENTED"`), indicating large unfinished areas despite dashboard claims of 100% completion.
- Service/systemd claims are not reflected in versioning or testing; the server entry point still targets Alpha 1-era features and lacks evidence of integration tests or install scripts being exercised.

## Impact
The codebase is closer to an Alpha 1 prototype: network handlers, pooling, authentication, and tests are largely scaffolding. Documentation overstates progress (Alpha 3 complete, enterprise security, ODBC/JDBC maturity, thousands of tests) and should be corrected or the missing functionality implemented before relying on these features.

## Build & Test Execution (2025-12-12)
- Built from `/home/dcalford/CliWork/ScratchBird/build` with `cmake --build .` (succeeds).
- `ctest --output-on-failure` ran 1,337 registered tests; 1,335 passed, 2 failed:
  - `ProtocolSessionTest.SendReceiveMessage` (`tests/unit/test_wire_protocol.cpp`): listen socket creation fails with `Operation not permitted` (environment restriction), status != `Status::OK`.
  - `ProtocolSessionTest.FullHandshake` (`tests/unit/test_wire_protocol.cpp`): subsequent failures/segfault because no socket/session established.
- The pass count aligns with the dashboard’s “1337 tests” claim numerically, but these tests do not exercise server↔engine query execution or protocol→engine integration. Most protocol-related coverage is codec-level; the adapter logic still returns canned responses (see above), so passing tests do not contradict the stubbed functionality noted in this audit.
