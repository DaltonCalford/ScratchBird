# Driver Plan: Foundation (libscratchbird client core)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Status: Draft

Spec references:
- /docs/specifications/parser/v3/drivers/ALPHA_DRIVER_BOOTSTRAP.md
- /docs/specifications/parser/v3/beta_requirements/drivers/DRIVER_BASELINE_SPEC.md
- /docs/specifications/parser/v3/wire_protocols/scratchbird_native_wire_protocol.md
- /docs/specifications/parser/v3/types/DATA_TYPE_PERSISTENCE_AND_CASTS.md
- /docs/specifications/parser/v3/security/*

Goal
- Deliver a stable, reusable client core that all drivers call into for SBWP v1.1, TLS/auth, autocommit, type encoding, and error mapping.

Scope
- Native SBWP over TCP and optional Unix sockets only.
- TLS required by default with SCRAM-SHA-256 auth.
- No emulation protocols.

Plan
1) Protocol compliance baseline
- Implement/verify STARTUP/AUTH/READY handshake, attachment_id/txn_id headers.
- Enforce TLS default and certificate validation; allow opt-out only if server permits.
- Implement CANCEL with urgent flag.

2) Connection config + parsing
- Standardize DSN/URL parsing and option normalization (sslmode, timeouts, application_name).
- Provide a per-driver adapter layer for string parsing but reuse the same core config struct.

3) Query + prepared statements
- Implement SIMPLE query and extended PARSE/BIND/EXECUTE APIs.
- Add placeholder rewrite from '?' to $1..N when needed.
- Add statement cache and batch execution support.

4) Streaming + COPY
- Implement STREAM_CONTROL backpressure.
- Implement COPY IN/OUT streaming primitives usable by drivers.

5) Type mapping + errors
- Binary encoding default; text fallback per DATA_TYPE_PERSISTENCE_AND_CASTS.
- SQLSTATE preservation and error field propagation.

6) Observability + testing hooks
- Connection metrics, trace logging hooks, wire debug toggles.

Testing
- Unit tests: parsing, auth, TLS validation, placeholder rewrite, error mapping.
- Integration tests: connect/auth/query/prepare/copy/streaming against listener.

Deliverables
- Stable libscratchbird client API with versioned ABI/semver.
- Driver-facing documentation and minimal sample client.

Dependencies
- SBWP v1.1 protocol implementation completeness.
- Listener/parser integration stable for integration tests.
