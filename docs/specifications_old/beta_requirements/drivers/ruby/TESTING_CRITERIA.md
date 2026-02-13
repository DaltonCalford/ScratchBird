# Testing Criteria

## Protocol tests
- TLS 1.3 handshake and certificate validation.
- STARTUP/AUTH flow for SCRAM and certificate auth.
- QUERY and prepared statement flows.
- CANCEL and timeout handling.

## Type round-trip tests
- Core scalars: bool, ints, floats, decimal, money.
- Temporal: date, time, timestamp, interval.
- UUID, JSON/XML, JSONB (`Scratchbird::JSONB`), RANGE (`Scratchbird::RangeValue`), GEOMETRY (`Scratchbird::Geometry`), binary, arrays, vector, network, spatial.

## API compliance tests
- Conformance to the language standard API (JDBC/ADO.NET/DB-API/database/sql/etc).
- Statement cache behavior and parameter binding semantics.

## SQLSTATE mapping tests
- SQLSTATE class 01 -> Scratchbird::Warning; class 02 -> Scratchbird::NoDataError.
- SQLSTATE class 08 -> Scratchbird::ConnectionError.
- SQLSTATE class 0A -> Scratchbird::NotSupportedError.
- SQLSTATE class 22 -> Scratchbird::DataError.
- SQLSTATE class 23 -> Scratchbird::IntegrityError.
- SQLSTATE class 28 -> Scratchbird::AuthError.
- SQLSTATE class 40 -> Scratchbird::TransactionError.
- SQLSTATE class 42 -> Scratchbird::SyntaxError.
- SQLSTATE class XX -> Scratchbird::InternalError.

## Performance tests
- Connection establishment time.
- Simple query latency.
- Prepared statement throughput.
- Large result streaming without memory spikes.
