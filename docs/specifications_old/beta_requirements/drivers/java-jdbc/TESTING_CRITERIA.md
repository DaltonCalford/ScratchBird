# Testing Criteria

## Protocol tests
- TLS 1.3 handshake and certificate validation.
- STARTUP/AUTH flow for SCRAM and certificate auth.
- QUERY and prepared statement flows.
- CANCEL and timeout handling.

## Type round-trip tests
- Core scalars: bool, ints, floats, decimal, money.
- Temporal: date, time, timestamp, interval.
- UUID, JSON/XML, JSONB (`SBJsonb`), RANGE (`SBRange<T>`), GEOMETRY (`SBGeometry`), binary, arrays, vector, network, spatial.

## API compliance tests
- Conformance to the language standard API (JDBC/ADO.NET/DB-API/database/sql/etc).
- Statement cache behavior and parameter binding semantics.

## SQLSTATE mapping tests
- SQLSTATE class 01 -> SQLWarning; class 02 -> SQLNoDataException.
- SQLSTATE class 08 -> SQLNonTransientConnectionException (or SQLTransientConnectionException if retryable).
- SQLSTATE class 0A -> SQLFeatureNotSupportedException.
- SQLSTATE class 22 -> SQLDataException.
- SQLSTATE class 23 -> SQLIntegrityConstraintViolationException.
- SQLSTATE class 28 -> SQLInvalidAuthorizationSpecException.
- SQLSTATE class 40 -> SQLTransactionRollbackException.
- SQLSTATE class 42 -> SQLSyntaxErrorException.
- SQLSTATE class XX -> SQLNonTransientException.

## Performance tests
- Connection establishment time.
- Simple query latency.
- Prepared statement throughput.
- Large result streaming without memory spikes.
