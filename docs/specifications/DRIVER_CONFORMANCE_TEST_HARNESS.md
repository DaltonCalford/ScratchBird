# Driver Conformance Test Harness (Shared)

Status: Draft
Last Updated: 2026-01-09

## Purpose

Provide a shared, language-agnostic conformance test harness so all drivers
can verify SBWP v1.1 protocol compliance and full wire type coverage.

## Scope

- SBWP v1.1 handshake, auth, and message framing
- PARSE/BIND/EXECUTE behavior
- Type encoding/decoding for all wire types
- Cancellation behavior
- Metadata queries (sys.*)

## Harness Shape

### 1. Test Manifest

A JSON manifest defines the test suite and required capabilities:

```json
{
  "schema_version": "1.0",
  "protocol_version": "sbwp-1.1",
  "suite": "sbwp-v1.1",
  "requires": ["tls", "auth", "prepare_bind", "types"],
  "tests": [
    {"id": "handshake", "kind": "query", "sql": "SELECT 1", "expect_rows": 1},
    {"id": "auth", "kind": "auth"},
    {"id": "prepare_bind", "kind": "prepare_bind", "sql": "SELECT $1::int32", "params": [42]},
    {"id": "describe_mismatch", "kind": "prepare_bind", "sql": "SELECT $1, $2", "params": [1], "expect_sqlstate": "07001"},
    {"id": "paging_basic", "kind": "query", "sql": "SELECT id FROM sb_conformance.basic_table", "dsn_append": "fetch_size=1"},
    {"id": "cancel_stream", "kind": "cancel", "sql": "SELECT id FROM sb_conformance.basic_table", "cancel_after_rows": 1, "expect_sqlstate": "57014", "requires": ["cancel"]},
    {"id": "compression_roundtrip", "kind": "query", "sql": "SELECT 1", "dsn_append": "compression=zstd", "requires": ["compression"]},
    {"id": "prepare_reuse_basic", "kind": "prepare_reuse", "sql": "SELECT $1::int32 + 1", "params": [41], "reuse_count": 5, "requires": ["prepare_reuse"]},
    {"id": "portal_paging_basic", "kind": "portal_paging", "sql": "SELECT id FROM sb_conformance.basic_table ORDER BY id", "fetch_size": 2, "expect_rows": 3, "requires": ["portal_paging"]}
  ]
}
```

Supported test fields:
- `kind`: auth | query | prepare_bind | cancel | copy | lob_stream | prepare_reuse | portal_paging | progress | notify
- `sql`: SQL to execute (query/prepare_bind/cancel)
- `params`: bound parameters for prepare_bind/prepare_reuse
- `expect_rows`: optional row count assertion
- `expect_sqlstate`: expected SQLSTATE on failure
- `timeout_ms`: per-test timeout in milliseconds (query/prepare_bind/portal_paging)
- `dsn_append`: query-string or key-value suffix appended to base DSN
- `requires`: optional list of env-gated requirements
- `cancel_after_rows`: rows to read before issuing CANCEL (cancel kind)
- `copy_direction`: in | out (copy kind)
- `copy_format`: text | csv | binary (copy kind)
- `copy_data_file`: path to payload file for COPY FROM STDIN (copy kind)
- `copy_expect_file`: path to expected payload for COPY TO STDOUT (copy kind)
  - For `copy_format=binary`, these payloads are DATA_ROW frames (row framing
    without the DATA_ROW message header) as described in SBWP.
- `lob_payload_file`: expected payload for LOB streaming (lob_stream kind)
- `lob_expect_sha256`: expected SHA-256 of the assembled LOB payload (lob_stream kind)
- `reuse_count`: number of execute cycles to validate reuse (prepare_reuse kind)
- `fetch_size`: max rows per portal fetch (portal_paging kind)
- `expect_progress`: require progress frames (progress kind)
- `notify_channel`: channel to subscribe/notify (notify kind)
- `notify_payload`: payload bytes as string (notify kind)

### 2.2 Feature Gates

Tests with a `requires` list only run when the environment variable
`SB_CONFORMANCE_FEATURES` includes the matching tokens (comma-separated).

Adapters may also check server-advertised capabilities to ensure a feature is
supported. ScratchBird native connections expose capabilities via
`CONNECT_RESPONSE` flags and the `sys.server_capabilities` catalog view.

### 2.3 Reference Runner

`scripts/run_driver_conformance.py` loads a manifest, filters tests via
`SB_CONFORMANCE_FEATURES`, and invokes the `sbdriver-conformance` adapter with
the filtered manifest on stdin. Provide `SB_CONFORMANCE_DSN` (or `--dsn`) and
ensure the adapter is on PATH (or pass `--adapter`).

### 2. Driver Adapter Contract

Each driver exposes a thin adapter that can:

- Connect using a DSN
- Execute a simple QUERY
- Execute PARSE/BIND/EXECUTE with parameters
- Stream results and return row data
- Issue CANCEL
- Run metadata queries

#### Prepare Reuse Contract

For `prepare_reuse` tests, adapters must:
- Prepare the SQL once using the server-side prepare path.
- Execute the same prepared handle `reuse_count` times with the provided params.
- Treat any prepare/execute failure as a test failure.

#### Portal Paging Contract

For `portal_paging` tests, adapters must:
- Execute using a portal with `fetch_size` rows per EXECUTE call.
- If the server returns `PORTAL_SUSPENDED`, re-issue EXECUTE until completion.
- Count total rows and compare against `expect_rows` when provided.

#### Progress Contract

For `progress` tests, adapters must:
- Execute the provided query.
- Confirm at least one progress frame was observed when `expect_progress=true`.

#### Notify Contract

For `notify` tests, adapters must:
- SUBSCRIBE to `notify_channel`.
- Execute the NOTIFY SQL provided in the test.
- Confirm a NOTIFICATION for the channel/payload was received.

CLI adapters must use the executable name `sbdriver-conformance` to keep
invocation consistent across languages.

### 2.1 Adapter Strategy (Hybrid)

Chosen approach:

1. Implement an in-language test helper for the reference driver (Go) to
   validate the contract quickly.
2. Extract a CLI adapter contract from the reference helper (stdin manifest,
   stdout JSON).
3. Implement the CLI adapter for each remaining driver to standardize behavior.

### 3. Result Format

Adapters must emit a normalized JSON result:

```json
{
  "test_id": "prepare_bind",
  "rows": [[42]],
  "columns": ["int32"],
  "status": "ok",
  "errors": []
}
```

### 4. Fixture SQL

A shared SQL fixture file defines schema and seed data used by all tests.
Fixtures live under `docs/fixtures/` (e.g., `core_fixture.sql`,
`types_fixture.sql`).

## Required Tests

1. Handshake (TLS required)
2. Authentication (valid credentials)
3. Prepare/bind with nulls and binary formats
4. One-way decode coverage for every wire type (server -> driver)

## Execution Model

- Phase A: Go in-language helper + harness runner for rapid validation
- Phase B: Standardized CLI adapters for all drivers
- CI: run harness per driver with DSN environment variables

## Output and Reporting

- JSON summary with pass/fail per test
- Per-driver report artifacts
- Failures include expected vs actual payloads
