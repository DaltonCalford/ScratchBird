# Native Driver Conformance (ScratchBird)

Version: 1.0
Status: Draft
Last Updated: January 2026

## Purpose

Define the server-side requirements needed for native ScratchBird drivers to
operate correctly using SBWP v1.1. This is the canonical contract the server
must satisfy for driver parity.

## Authoritative Reference

- `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md`

## Required Server Capabilities

1. SBWP v1.1 only
   - Accept only the 40-byte header and SBWP v1.1 message set.
   - Reject legacy SBDB headers and non-native protocols.

2. TLS 1.3 required
   - Enforce TLS 1.3 at the listener; no plaintext fallback in production.

3. Always-in-transaction semantics
   - Provide attachment_id and txn_id in AUTH_OK.
   - Require attachment_id and txn_id on all subsequent messages.

4. Extended query protocol
   - Implement PARSE, DESCRIBE, BIND, EXECUTE, CLOSE, SYNC.
   - Support parameter formats (text/binary) and result formats per column.

5. Cancellation
   - Implement CANCEL with MSG_FLAG_URGENT behavior.

6. Type serialization
   - Provide binary encodings for all wire types listed in SBWP.
   - Maintain compatibility with the Type Serialization section in the wire spec.

7. Error mapping
   - Return SQLSTATE and ScratchBird error codes for driver use.

## Driver Expectations

- Drivers are native-only; emulated protocols are out of scope.
- Client-side SQL substitution for parameters is forbidden.

