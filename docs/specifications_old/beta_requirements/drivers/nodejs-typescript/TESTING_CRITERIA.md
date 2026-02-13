# Testing Criteria

## Protocol tests
- TLS 1.3 handshake and certificate validation.
- STARTUP/AUTH flow for SCRAM and certificate auth.
- QUERY and prepared statement flows.
- CANCEL and timeout handling.

## Type round-trip tests
- Core scalars: bool, ints, floats, decimal, money.
- Temporal: date, time, timestamp, interval.
- UUID, JSON/XML, JSONB (`ScratchbirdJsonb`), RANGE (`ScratchbirdRange<T>`), GEOMETRY (`ScratchbirdGeometry`), binary, arrays, vector, network, spatial.

## API compliance tests
- Conformance to the language standard API (JDBC/ADO.NET/DB-API/database/sql/etc).
- Statement cache behavior and parameter binding semantics.

## SQLSTATE mapping tests
- SQLSTATE class 01 -> ScratchbirdWarning; class 02 -> ScratchbirdNoDataError.
- SQLSTATE class 08 -> ScratchbirdConnectionError.
- SQLSTATE class 0A -> ScratchbirdNotSupportedError.
- SQLSTATE class 22 -> ScratchbirdDataError.
- SQLSTATE class 23 -> ScratchbirdIntegrityError.
- SQLSTATE class 28 -> ScratchbirdAuthError.
- SQLSTATE class 40 -> ScratchbirdTransactionError.
- SQLSTATE class 42 -> ScratchbirdSyntaxError.
- SQLSTATE class XX -> ScratchbirdInternalError.

## Performance tests
- Connection establishment time.
- Simple query latency.
- Prepared statement throughput.
- Large result streaming without memory spikes.
