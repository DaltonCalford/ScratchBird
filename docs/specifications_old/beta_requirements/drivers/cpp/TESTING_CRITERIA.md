# Testing Criteria

## Protocol tests
- TLS 1.3 handshake and certificate validation.
- STARTUP/AUTH flow for SCRAM and certificate auth.
- QUERY and prepared statement flows.
- CANCEL and timeout handling.

## Type round-trip tests
- Core scalars: bool, ints, floats, decimal, money.
- Temporal: date, time, timestamp, interval.
- UUID, JSON/XML, JSONB (`scratchbird::Jsonb`), RANGE (`scratchbird::Range<T>`), GEOMETRY (`scratchbird::Geometry`), binary, arrays, vector, network, spatial.

## API compliance tests
- Conformance to the language standard API (JDBC/ADO.NET/DB-API/database/sql/etc).
- Statement cache behavior and parameter binding semantics.

## SQLSTATE mapping tests
- SQLSTATE class 01 -> scratchbird::Warning; class 02 -> scratchbird::NoDataError.
- SQLSTATE class 08 -> scratchbird::ConnectionError.
- SQLSTATE class 0A -> scratchbird::NotSupportedError.
- SQLSTATE class 22 -> scratchbird::DataError.
- SQLSTATE class 23 -> scratchbird::IntegrityError.
- SQLSTATE class 28 -> scratchbird::AuthError.
- SQLSTATE class 40 -> scratchbird::TransactionError.
- SQLSTATE class 42 -> scratchbird::SyntaxError.
- SQLSTATE class XX -> scratchbird::InternalError.

## Performance tests
- Connection establishment time.
- Simple query latency.
- Prepared statement throughput.
- Large result streaming without memory spikes.
