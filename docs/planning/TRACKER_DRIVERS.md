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
  - [ ] Streaming + COPY
  - [ ] Type encoding/decoding
  - [ ] SQLSTATE/error mapping
  - [ ] Tests: unit + integration (SCRAM handshake coverage added)

- [ ] ODBC driver
  - [ ] Handles + lifecycle
  - [ ] Connection strings + TLS/auth
  - [ ] Statements + params + results
  - [ ] Diagnostics + SQLSTATE
  - [ ] Packaging + install docs
  - [ ] Tests: unit + integration

- [ ] JDBC driver
  - [ ] Driver + URL parsing
  - [ ] Connection/Statement/ResultSet
  - [ ] TLS/auth + config
  - [ ] Types + batching
  - [ ] Packaging
  - [ ] Tests: unit + integration

- [ ] Python driver
  - [ ] PEP 249 API
  - [ ] TLS/auth + config
  - [ ] Types + arrays
  - [ ] Packaging
  - [ ] Tests: unit + integration

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
- Foundation: libscratchbird client core
