# Testing Criteria

## Protocol tests
- TLS 1.3 handshake and certificate validation.
- STARTUP/AUTH flow for SCRAM and certificate auth.
- QUERY and prepared statement flows.
- CANCEL and timeout handling.

## Type round-trip tests
- Core scalars: bool, ints, floats, decimal, money.
- Temporal: date, time, timestamp, interval.
- UUID, JSON/XML, JSONB (`TScratchBirdJsonb`), RANGE (`TScratchBirdRange`), GEOMETRY (`TScratchBirdGeometry`), binary, arrays, vector, network, spatial.

## API compliance tests
- Conformance to the language standard API (JDBC/ADO.NET/DB-API/database/sql/etc).
- Statement cache behavior and parameter binding semantics.

## SQLSTATE mapping tests
- SQLSTATE class 01 -> EScratchbirdWarning; class 02 -> EScratchbirdNoData.
- SQLSTATE class 08 -> EScratchbirdConnectionError.
- SQLSTATE class 0A -> EScratchbirdNotSupported.
- SQLSTATE class 22 -> EScratchbirdDataError.
- SQLSTATE class 23 -> EScratchbirdIntegrityError.
- SQLSTATE class 28 -> EScratchbirdAuthError.
- SQLSTATE class 40 -> EScratchbirdTransactionError.
- SQLSTATE class 42 -> EScratchbirdSyntaxError.
- SQLSTATE class XX -> EScratchbirdInternalError.

## Performance tests
- Connection establishment time.
- Simple query latency.
- Prepared statement throughput.
- Large result streaming without memory spikes.
