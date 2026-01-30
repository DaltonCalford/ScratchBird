# Driver-Visible Server Features (Gaps + Extensions)

Status: Draft
Last Updated: 2026-01-30

## Purpose

Document server-side capabilities required to unlock the full driver feature
set and higher conformance levels. This spec is used by ScratchBird server
agents to prioritize protocol and catalog work that directly impacts drivers.

## Baseline Capabilities (Expected by Drivers)

These are required for "full SBWP v1.1 conformance" and already assumed by
every driver in this repo.

Protocol + transport:
- TLS required (no plaintext), SCRAM auth, message framing, STARTUP/READY.
- Binary-only results; text format is treated as an error by drivers.
- PARSE/BIND/EXECUTE flow for parameterized statements.
- DESCRIBE returns parameter metadata (count/type list).
- CANCEL returns SQLSTATE 57014 on cancellation.
- PORTAL paging: `MSG_PORTAL_SUSPENDED` + `EXECUTE max_rows` loops.

Startup parameters (accepted or ignored safely):
- `database`, `user`, `role`, `application_name`, `search_path`.

Catalog/metadata (minimum for BI tools):
- `sys.schemas`, `sys.tables`, `sys.columns`
- `sys.types` (type_id -> type_name)
- `sys.indexes`
- `sys.index_columns` (if absent, tools still work but index column detail is empty)
- `sys.foreign_keys` / `sys.primary_keys` (or compatible information_schema views)

## Known Gaps / Not Yet Supported in Server

These are intentionally disabled in drivers until server support exists.

1. Compression (zstd)
   - Drivers reject `compression=zstd` with SQLSTATE 0A000.
   - Requires: negotiated compression flag + per-message compress/decompress.

## Optional Extensions (Nice-to-Have)

Each item is optional but unlocks higher-level UX or tool compatibility.

1. COPY/Bulk streaming
   - Capability: high-throughput ingest/export (binary COPY).
   - Driver impact: Go/JDBC/.NET can stream without row-by-row overhead.

2. Large object streaming
   - Capability: chunked BLOB/CLOB transfer without full buffering.
   - Driver impact: avoids memory spikes for large payloads.

3. Server-side prepared statement cache + stats
   - Capability: reuse plans across executes; optional plan stats view.
   - Driver impact: JDBC `prepareThreshold` becomes meaningful.

4. Holdable/named portals (server cursors)
   - Capability: cursor survives transaction boundaries; scrollable cursors.
   - Driver impact: scrollable ResultSet support for JDBC/.NET.

5. Capability negotiation flags
   - Capability: server advertises optional features (compression, COPY, etc.).
   - Driver impact: drivers can auto-enable when supported.

6. Query progress + metrics frames
   - Capability: live progress frames or sys.* progress views.
   - Driver impact: UI progress bars + smarter cancel decisions.

7. Event/notification channel
   - Capability: LISTEN/NOTIFY style or SBWP push frames.
   - Driver impact: async notifications for UI tools.

8. Richer metadata views
   - Capability: domains/enums, check constraints, expression indexes,
     partitioning metadata, table stats.
   - Driver impact: BI tools show accurate schema + indexing details.

## Conformance Test Extensions (Optional)

Once a capability lands, add a gated conformance test:
- Compression: compressed query round-trip
- COPY: import/export with row count check
- Large objects: stream + checksum
- Portals: scroll/holdable cursor behaviors
- Eventing: LISTEN/NOTIFY round-trip

These should be gated by environment variables in the harness.

## Related Specs

- `docs/specifications/DRIVER_CONFORMANCE_TEST_HARNESS.md`
- `docs/specifications/DRIVER_STREAMING_AND_PAGING.md`
- ScratchBird server wire protocol spec (main repo)
