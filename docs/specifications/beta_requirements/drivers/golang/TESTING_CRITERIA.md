# Testing Criteria

## Protocol tests
- TLS 1.3 handshake and certificate validation.
- STARTUP/AUTH flow for SCRAM and certificate auth.
- QUERY and prepared statement flows.
- CANCEL and timeout handling.

## Type round-trip tests
- Core scalars: bool, ints, floats, decimal, money.
- Temporal: date, time, timestamp, interval.
- UUID, JSON/XML, JSONB (`scratchbird.JSONB`), RANGE (`scratchbird.Range[T]`), GEOMETRY (`scratchbird.Geometry`), binary, arrays, vector, network, spatial.

## API compliance tests
- Conformance to the language standard API (JDBC/ADO.NET/DB-API/database/sql/etc).
- Statement cache behavior and parameter binding semantics.

## SQLSTATE mapping tests
- SQLSTATE class 01 -> scratchbird.Error{Kind: ErrWarning}; class 02 -> scratchbird.Error{Kind: ErrNoData}.
- SQLSTATE class 08 -> scratchbird.Error{Kind: ErrConnection}.
- SQLSTATE class 0A -> scratchbird.Error{Kind: ErrNotSupported}.
- SQLSTATE class 22 -> scratchbird.Error{Kind: ErrData}.
- SQLSTATE class 23 -> scratchbird.Error{Kind: ErrIntegrity}.
- SQLSTATE class 28 -> scratchbird.Error{Kind: ErrAuth}.
- SQLSTATE class 40 -> scratchbird.Error{Kind: ErrTransaction}.
- SQLSTATE class 42 -> scratchbird.Error{Kind: ErrSyntax}.
- SQLSTATE class XX -> scratchbird.Error{Kind: ErrInternal}.

## Performance tests
- Connection establishment time.
- Simple query latency.
- Prepared statement throughput.
- Large result streaming without memory spikes.
