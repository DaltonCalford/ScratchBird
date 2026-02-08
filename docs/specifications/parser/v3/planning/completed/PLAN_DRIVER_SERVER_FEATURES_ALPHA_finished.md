# Plan: Driver-Visible Server Features (Alpha Required)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Status: In Progress
Owner: Server Core
Last Updated: 2026-01-31

## Goal

Implement every item in `docs/findings/DRIVER_SERVER_FEATURE_GAPS_AND_EXTENSIONS.md` so
all driver-visible server features are available for Alpha. This plan treats every
item in that document as **required**, even if previously marked optional.

## Specs to Follow

- `/docs/specifications/parser/v3/wire_protocols/scratchbird_native_wire_protocol.md`
- `/docs/specifications/parser/v3/DRIVER_STREAMING_AND_PAGING.md`
- `/docs/specifications/parser/v3/DRIVER_CONFORMANCE_TEST_HARNESS.md`
- `/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_STRUCTURE.md`
- `/docs/specifications/parser/v3/operations/PROMETHEUS_METRICS_REFERENCE.md`
- `/docs/specifications/parser/v3/operations/MONITORING_DIALECT_MAPPINGS.md`

## Workstreams (Ordered)

### WS-1: SBWP Compression (zstd)

Status: Complete

1. Add negotiated compression flags in STARTUP/AUTH_OK per SBWP v1.1.
2. Implement per-message compress/decompress with `MSG_FLAG_COMPRESSED`.
3. Enforce server policy on accepted compression algorithms.
4. Add a conformance test case gated by env var.

Deliverables:
- SBWP compression path in protocol layer.
- Conformance test: compressed round-trip (`compression_roundtrip` in `docs/fixtures/driver_conformance_manifest.json`).

### WS-2: COPY / Bulk Streaming

Status: Complete

1. Implement binary COPY IN/OUT in SBWP (`COPY_IN_RESPONSE`, `COPY_OUT_RESPONSE`,
   `COPY_DATA`, `COPY_DONE`, `COPY_FAIL`). (done)
2. Add portal-driven paging for COPY OUT where required. (done)
3. Wire server-side copy stats to monitoring (if specified). (done)
4. Add conformance tests for bulk COPY. (done)

Deliverables:
- Server-side COPY streaming for SBWP.
- Conformance test: COPY import/export + row count (`copy_in_basic`, `copy_out_basic`, `copy_in_binary`, `copy_out_binary` in `docs/fixtures/driver_conformance_manifest.json`).

### WS-3: Large Object Streaming (LOB)

Status: Complete

1. Implement chunked BLOB/CLOB streaming for SBWP.
2. Add flow control with `STREAM_CONTROL` / `STREAM_*` message types.
3. Ensure binary-only enforcement for streaming payloads.
4. Add conformance tests for LOB streaming + checksum.

Deliverables:
- LOB streaming path in protocol/executor.
- Conformance test: LOB stream + checksum (`lob_stream_basic` in `docs/fixtures/driver_conformance_manifest.json`).

### WS-4: Prepared Statement Cache + Stats

Status: Complete

1. Implement server-side prepare cache with eviction policy (LRU or size limit).
2. Add plan stats view (sys.*) for prepared statements.
3. Wire `DESCRIBE` parameter metadata consistently.
4. Add conformance tests for prepare reuse.

Deliverables:
- Prepared statement cache + sys view(s).
- Tests for prepare reuse and metadata (`prepare_reuse_basic` in `docs/fixtures/driver_conformance_manifest.json`).

### WS-5: Holdable / Named Portals (Cursors)

Status: Complete

1. Extend portal lifecycle to support holdable cursors across transactions.
2. Implement scrollable cursor semantics (forward/backward where applicable).
3. Ensure portal paging (`MSG_PORTAL_SUSPENDED`) is consistent with streaming.
4. Add conformance tests for portal pagination + holdable cursors.

Deliverables:
- Named/holdable portal support.
- Conformance tests for portal behavior.

### WS-6: Capability Negotiation Flags

Status: Complete

1. Define server capability set in STARTUP/READY (compression, COPY, LOB, portals,
   notifications, progress).
2. Add system catalog/parameter exposure (e.g., `sys.server_capabilities`).
3. Update drivers (or conformance harness) to read capabilities.

Deliverables:
- Capability flags in SBWP handshake.
- sys view for capability introspection.

### WS-7: Query Progress + Metrics Frames

Status: Complete

1. Implement progress frames (`STREAM_*` or dedicated message type) with
   rows processed + bytes read.
2. Add `sys.performance` fields for live query progress.
3. Add optional conformance test gated by env var.

Deliverables:
- Progress frames + catalog exposure.
- Test gated by env var (`progress_basic` in `docs/fixtures/driver_conformance_manifest.json`).

### WS-8: Event / Notification Channel

Status: Complete

1. Implement LISTEN/NOTIFY semantics over SBWP (`SUBSCRIBE`, `NOTIFICATION`).
2. Add permission checks for channel subscription.
3. Add conformance test for NOTIFY round-trip.

Deliverables:
- Notification channel in SBWP.
- Conformance test for eventing (`notify_basic` in `docs/fixtures/driver_conformance_manifest.json`).

### WS-9: Richer Metadata Views

Status: Complete

1. Expand sys.* catalog views to include:
   - Domains/enums
   - Check constraints
   - Expression indexes
   - Partitioning metadata
   - Table stats
2. Ensure compatibility with BI tooling expectations.
3. Add conformance tests validating metadata.

Deliverables:
- Expanded sys.* metadata views.
- Metadata conformance tests.

## Cross-Cutting Tasks

- Update `docs/findings/DRIVER_SERVER_FEATURE_GAPS_AND_EXTENSIONS.md` status per WS.
- Update `/docs/specifications/parser/v3/DRIVER_CONFORMANCE_TEST_HARNESS.md` with new tests.
- Add any required SBWP message definitions or clarifications to
  `/docs/specifications/parser/v3/wire_protocols/scratchbird_native_wire_protocol.md`.

## Validation

- Run conformance harness with feature gates enabled for each WS.
- Add unit/integration tests for SBWP protocol changes where available.

## Exit Criteria

- All WS deliverables completed.
- All conformance tests pass with required features enabled.
- Driver feature matrix shows full SBWP v1.1 conformance and all extensions available.
