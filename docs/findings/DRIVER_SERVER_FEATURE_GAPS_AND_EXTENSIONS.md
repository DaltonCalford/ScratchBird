# Driver-Visible Server Features (Gaps + Extensions)

Status: Draft
Last Updated: 2026-01-31

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

(None at this time.)

## Resolved Gaps

1. Compression (zstd)
   - Negotiated compression flag + per-message compress/decompress implemented.
   - Conformance test gated by `SB_CONFORMANCE_FEATURES=compression`.
2. COPY/Bulk streaming
   - Binary/text COPY with stream flow control.
   - Conformance test gated by `SB_CONFORMANCE_FEATURES=copy`.
3. Large object streaming
   - Streamed LOB values via STREAM_READY/DATA/END with ROW_DATA references.
   - Conformance test gated by `SB_CONFORMANCE_FEATURES=lob_stream`.
4. Prepared statement cache + stats
   - Per-connection LRU cache with execution tracking in sys.statement_cache.
5. Capability negotiation flags
   - Server capabilities advertised in CONNECT_RESPONSE and sys.server_capabilities.
6. Query progress + metrics frames
   - QUERY_PROGRESS frames with rows/bytes and sys.performance progress metrics.
7. Event/notification channel
   - SUBSCRIBE/UNSUBSCRIBE + NOTIFICATION over SBWP with conformance test.
8. Conformance adapter build
   - `sbdriver-conformance` builds cleanly against current link deps.
9. Richer sys.* metadata views
   - sys.schemas/tables/columns/indexes/index_columns/types/domains/constraints/foreign_keys/primary_keys wired.

## Optional Extensions (Nice-to-Have)

Each item is optional but unlocks higher-level UX or tool compatibility.

1. Holdable/named portals (server cursors)
   - Capability: cursor survives transaction boundaries; scrollable cursors.
   - Driver impact: scrollable ResultSet support for JDBC/.NET.

2. Richer metadata views
   - Capability: domains/enums, check constraints, expression indexes,
     partitioning metadata, table stats.
   - Driver impact: BI tools show accurate schema + indexing details.

## Conformance Test Extensions (Optional)

Once a capability lands, add a gated conformance test:
- Compression: compressed query round-trip
- COPY: import/export with row count check
- Large objects: stream + checksum
- Portals: scroll/holdable cursor behaviors
- Progress: progress frame validation
- Eventing: LISTEN/NOTIFY round-trip

These should be gated by environment variables in the harness.

## Related Specs

- `docs/specifications/DRIVER_CONFORMANCE_TEST_HARNESS.md`
- `docs/specifications/DRIVER_STREAMING_AND_PAGING.md`
- ScratchBird server wire protocol spec (main repo)
