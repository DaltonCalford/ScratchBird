# Testing Criteria

## Protocol tests
- TLS 1.3 handshake and certificate validation.
- STARTUP/AUTH flow for SCRAM and certificate auth.
- QUERY and prepared statement flows.
- CANCEL and timeout handling.

## Type round-trip tests
- Core scalars: bool, ints, floats, decimal, money.
- Temporal: date, time, timestamp, interval.
- UUID, JSON/XML, JSONB (`scratchbird::types::Jsonb`), RANGE (`scratchbird::types::Range<T>`), GEOMETRY (`scratchbird::types::Geometry`), binary, arrays, vector, network, spatial.

## API compliance tests
- Conformance to the language standard API (JDBC/ADO.NET/DB-API/database/sql/etc).
- Statement cache behavior and parameter binding semantics.

## SQLSTATE mapping tests
- SQLSTATE class 01 -> Error::Warning; class 02 -> Error::NoData.
- SQLSTATE class 08 -> Error::Connection.
- SQLSTATE class 0A -> Error::NotSupported.
- SQLSTATE class 22 -> Error::Data.
- SQLSTATE class 23 -> Error::Integrity.
- SQLSTATE class 28 -> Error::Auth.
- SQLSTATE class 40 -> Error::Transaction.
- SQLSTATE class 42 -> Error::Syntax.
- SQLSTATE class XX -> Error::Internal.

## Performance tests
- Connection establishment time.
- Simple query latency.
- Prepared statement throughput.
- Large result streaming without memory spikes.
