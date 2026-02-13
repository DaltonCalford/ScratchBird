# Testing Criteria

## Protocol tests
- TLS 1.3 handshake and certificate validation.
- STARTUP/AUTH flow for SCRAM and certificate auth.
- QUERY and prepared statement flows.
- CANCEL and timeout handling.

## Type round-trip tests
- Core scalars: bool, ints, floats, decimal, money.
- Temporal: date, time, timestamp, interval.
- UUID, JSON/XML, JSONB (`scratchbird.types.Jsonb`), RANGE (`scratchbird.types.Range`), GEOMETRY (`scratchbird.types.Geometry`), binary, arrays, vector, network, spatial.

## API compliance tests
- Conformance to the language standard API (JDBC/ADO.NET/DB-API/database/sql/etc).
- Statement cache behavior and parameter binding semantics.

## SQLSTATE mapping tests
- SQLSTATE class 01 -> Warning; class 02 -> Warning.
- SQLSTATE class 08 -> OperationalError.
- SQLSTATE class 0A -> NotSupportedError.
- SQLSTATE class 22 -> DataError.
- SQLSTATE class 23 -> IntegrityError.
- SQLSTATE class 28 -> OperationalError.
- SQLSTATE class 40 -> OperationalError.
- SQLSTATE class 42 -> ProgrammingError.
- SQLSTATE class XX -> InternalError.

## Performance tests
- Connection establishment time.
- Simple query latency.
- Prepared statement throughput.
- Large result streaming without memory spikes.
