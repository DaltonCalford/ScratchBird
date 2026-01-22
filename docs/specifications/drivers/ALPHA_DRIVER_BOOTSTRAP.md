# Alpha Driver Bootstrap Specification (Native ScratchBird)

Version: 1.0
Status: Draft (Alpha IP layer)
Last Updated: January 2026

## Purpose

Define the Alpha driver bootstrap requirements for native ScratchBird
connectivity. All client drivers listed in the drivers specifications
MUST be implemented using the native protocol stack.

## Scope

- ScratchBird native protocol only
- libscratchbird (network-only) usage
- Connection strings, DSN, TLS, and auth mapping
- Autocommit semantics
- Server-side prepare/bind (no client-side SQL substitution)

Out of scope:
- Emulation drivers (PostgreSQL/MySQL/Firebird clients) wiring
- Embedded engine mode (driver MUST use network path in Alpha)

## Required Drivers (Alpha)

The following drivers are required for Alpha:
- ODBC (ODBC 3.x)
- JDBC (Type 4)
-- All language drivers listed in docs/specifications/beta_requirements/drivers/
  (native ScratchBird protocol only), including:
  - C++
  - C#/.NET
  - Go
  - Java
  - Node.js/TypeScript
  - PHP
  - Python
  - Ruby
  - Rust
  - R
  - Pascal/Delphi

## Connection Model

- Driver connects to ScratchBird listener (default port 3092).
- Driver uses libscratchbird network-only client library.
- Embedded mode is not used by drivers in Alpha.

## Connection String

Canonical format:

```
scratchbird://user:password@host:port/database?sslmode=require&role=app
```

Required parameters:
- host
- port
- database
- user

Optional:
- role
- sslmode (disable|prefer|require)
- application_name

## Authentication

- Default: SCRAM-SHA-256 or password (per server allowlist)
- TLS client certificates supported when sslmode=require
- Driver must surface auth errors as SQLSTATE class 28

## Autocommit Semantics

ScratchBird is always in a transaction.
- Autocommit ON: each statement commits and starts a new transaction.
- Autocommit OFF: explicit COMMIT/ROLLBACK required.

Drivers must map their autocommit flags to this model.

## Type Mapping

- Use DATA_TYPE_PERSISTENCE_AND_CASTS.md for canonical formats.
- UUID string output must be canonical (hyphenated).
- Temporal types are normalized to UTC at rest.

## Error Mapping

- Engine returns SB error codes and SQLSTATE where available.
- Drivers must map to native exception types for each language.

## Packaging

- Each driver ships with minimal dependencies.
- TLS support required for production builds.

## Related Specs

- docs/specifications/drivers/ODBC_DRIVER_SPECIFICATION.md
- docs/specifications/drivers/JDBC_DRIVER_SPECIFICATION.md
- docs/specifications/drivers/NATIVE_DRIVER_CONFORMANCE.md
- docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md
- docs/specifications/types/DATA_TYPE_PERSISTENCE_AND_CASTS.md
