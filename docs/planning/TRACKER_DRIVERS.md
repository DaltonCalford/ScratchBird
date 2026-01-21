# Driver Implementation Tracker

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
  - [ ] TLS/auth + config
  - [ ] Types + batching
  - [x] Packaging (Gradle build + wrapper + jar)
  - [ ] Tests: unit + integration

- [ ] Python driver
  - [x] PEP 249 API (query execution + cursor wiring)
  - [x] TLS/auth + config
  - [x] Types + arrays
  - [x] Packaging
  - [x] Tests: unit + integration

- [ ] Node.js/TypeScript driver
  - [ ] Async API + pooling
  - [ ] TLS/auth + config
  - [ ] Types + streaming
  - [ ] Packaging
  - [ ] Tests: unit + integration

- [ ] .NET driver
  - [ ] Provider skeleton
  - [ ] TLS/auth + config
  - [ ] Params + types
  - [ ] Async + cancellation
  - [ ] Packaging
  - [ ] Tests: unit + integration

- [ ] Go driver
  - [ ] database/sql interfaces
  - [ ] TLS/auth + config
  - [ ] Types + scanning
  - [ ] Packaging
  - [ ] Tests: unit + integration

- [ ] PHP driver
  - [ ] PDO driver
  - [ ] TLS/auth + config
  - [ ] Types + streaming
  - [ ] Packaging
  - [ ] Tests: unit + integration

- [ ] Pascal/Delphi driver
  - [ ] Core library
  - [ ] FireDAC/IBX/Zeos/SQLdb adapters
  - [ ] Types + metadata
  - [ ] Packaging
  - [ ] Tests: unit + integration

- [ ] C/C++ driver
  - [ ] C API
  - [ ] C++ wrapper
  - [ ] Types + streaming
  - [ ] Packaging
  - [ ] Tests: unit + integration

- [ ] Ruby driver
  - [ ] Driver API + adapters
  - [ ] TLS/auth + config
  - [ ] Types + streaming
  - [ ] Packaging
  - [ ] Tests: unit + integration

- [ ] Rust driver
  - [ ] Crate structure
  - [ ] TLS/auth + config
  - [ ] Types + streaming
  - [ ] Packaging
  - [ ] Tests: unit + integration

- [ ] R driver
  - [ ] DBI API
  - [ ] TLS/auth + config
  - [ ] Types + data frames
  - [ ] Packaging
  - [ ] Tests: unit + integration

Next up
- JDBC driver: TLS/auth + config, types/batching, tests
- Python driver: PEP 249 core (transport + basic queries)
