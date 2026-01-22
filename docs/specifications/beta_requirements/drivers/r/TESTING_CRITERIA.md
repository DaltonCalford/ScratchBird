# Testing Criteria

## Protocol tests
- TLS 1.3 handshake and certificate validation.
- STARTUP/AUTH flow for SCRAM and certificate auth.
- QUERY and prepared statement flows.
- CANCEL and timeout handling.

## Type round-trip tests
- Core scalars: bool, ints, floats, decimal, money.
- Temporal: date, time, timestamp, interval.
- UUID, JSON/XML, JSONB (`sb_jsonb`), RANGE (`sb_range`), GEOMETRY (`sb_geometry`), binary, arrays, vector, network, spatial.

## API compliance tests
- Conformance to the language standard API (JDBC/ADO.NET/DB-API/database/sql/etc).
- Statement cache behavior and parameter binding semantics.

## SQLSTATE mapping tests
- SQLSTATE class 01 -> scratchbird_warning; class 02 -> scratchbird_no_data.
- SQLSTATE class 08 -> scratchbird_connection_error.
- SQLSTATE class 0A -> scratchbird_not_supported_error.
- SQLSTATE class 22 -> scratchbird_data_error.
- SQLSTATE class 23 -> scratchbird_integrity_error.
- SQLSTATE class 28 -> scratchbird_auth_error.
- SQLSTATE class 40 -> scratchbird_transaction_error.
- SQLSTATE class 42 -> scratchbird_syntax_error.
- SQLSTATE class XX -> scratchbird_internal_error.

## Performance tests
- Connection establishment time.
- Simple query latency.
- Prepared statement throughput.
- Large result streaming without memory spikes.
