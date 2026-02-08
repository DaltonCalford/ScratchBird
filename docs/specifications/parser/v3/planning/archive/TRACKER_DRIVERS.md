# Driver Implementation Tracker

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Status: In Progress

Purpose
- Ensure all driver deliverables are tracked end-to-end (core library, per-language drivers, tests, packaging).

Phases
1) Foundation: libscratchbird client core (SBWP + TLS + auth + config + types)
2) Connectivity standards: ODBC, JDBC
3) Language drivers: Python, Node.js/TS, .NET, Go, PHP, Pascal/Delphi, C/C++, Ruby, Rust, R

Checklist
- [ ] Foundation (libscratchbird client core)
  - [x] SBWP handshake, auth, TLS defaults (SCRAM client handshake in NetworkClient)
  - [x] Config/DSN parsing normalization (shared driver config helpers)
  - [x] Simple + extended query APIs (client-side prepared fallback + placeholder rewrite)
  - [x] Streaming + COPY
  - [x] Type encoding/decoding
  - [x] SQLSTATE/error mapping
  - [x] Tests: unit + integration (SCRAM handshake coverage added)

- [ ] ODBC driver
  - [x] Handles + lifecycle
  - [x] Connection strings + TLS/auth
  - [x] Statements + params + results (parameter type coverage expanded; ODBC integration test added)
  - [x] Diagnostics + SQLSTATE
  - [x] Packaging + install docs
  - [x] Tests: unit + integration (type info unit + ODBC integration test added)

- [ ] JDBC driver
  - [x] Driver + URL parsing (skeleton)
  - [x] Connection/Statement/ResultSet (skeleton)
  - [x] TLS/auth + config
  - [x] Types + batching
  - [x] Packaging (Gradle build + wrapper + jar)
  - [x] Tests: unit + integration

- [ ] Python driver
  - [x] PEP 249 API (query execution + cursor wiring)
  - [x] TLS/auth + config
  - [x] Types + arrays
  - [x] Packaging
  - [x] Tests: unit + integration

- [ ] Node.js/TypeScript driver
  - [x] Async API + pooling
  - [x] TLS/auth + config
  - [x] Types + streaming
  - [x] Packaging
  - [x] Tests: unit + integration

- [ ] .NET driver
  - [x] Provider skeleton
  - [x] TLS/auth + config
  - [x] Params + types
  - [x] Async + cancellation
  - [x] Packaging
  - [x] Tests: unit + integration

- [ ] Go driver
  - [x] database/sql interfaces
  - [x] TLS/auth + config
  - [x] Types + scanning
  - [x] Packaging
  - [x] Tests: unit + integration

- [ ] PHP driver
  - [x] PDO driver
  - [x] TLS/auth + config
  - [x] Types + streaming
  - [x] Packaging
  - [x] Tests: unit + integration

- [ ] Pascal/Delphi driver
  - [x] Core library
  - [x] FireDAC/IBX/Zeos/SQLdb adapters
  - [x] Types + metadata
  - [x] Packaging
  - [x] Tests: unit + integration

- [ ] C/C++ driver
  - [x] C API
  - [x] C++ wrapper
  - [x] Types + streaming
  - [x] Packaging
  - [x] Tests: unit + integration

- [ ] Ruby driver
  - [x] Driver API + adapters
  - [x] TLS/auth + config
  - [x] Types + streaming
  - [x] Packaging
  - [x] Tests: unit + integration

- [ ] Rust driver
  - [x] Crate structure
  - [x] TLS/auth + config
  - [x] Types + streaming
  - [x] Packaging
  - [x] Tests: unit + integration

- [ ] R driver
  - [x] DBI API
  - [x] TLS/auth + config
  - [x] Types + data frames
  - [x] Packaging
  - [x] Tests: unit + integration

Next up
- Driver work complete
