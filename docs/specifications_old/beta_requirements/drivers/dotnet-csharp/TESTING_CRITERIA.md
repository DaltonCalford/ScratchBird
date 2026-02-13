# Testing Criteria

## Protocol tests
- TLS 1.3 handshake and certificate validation.
- STARTUP/AUTH flow for SCRAM and certificate auth.
- QUERY and prepared statement flows.
- CANCEL and timeout handling.

## Type round-trip tests
- Core scalars: bool, ints, floats, decimal, money.
- Temporal: date, time, timestamp, interval.
- UUID, JSON/XML, JSONB (`ScratchBirdJsonb`), RANGE (`ScratchBirdRange<T>`), GEOMETRY (`ScratchBirdGeometry`), binary, arrays, vector, network, spatial.

## API compliance tests
- Conformance to the language standard API (JDBC/ADO.NET/DB-API/database/sql/etc).
- Statement cache behavior and parameter binding semantics.

## SQLSTATE mapping tests
- SQLSTATE class 01 -> ScratchbirdWarning; class 02 -> ScratchbirdNoDataException.
- SQLSTATE class 08 -> ScratchbirdConnectionException.
- SQLSTATE class 0A -> ScratchbirdNotSupportedException.
- SQLSTATE class 22 -> ScratchbirdDataException.
- SQLSTATE class 23 -> ScratchbirdIntegrityException.
- SQLSTATE class 28 -> ScratchbirdAuthException.
- SQLSTATE class 40 -> ScratchbirdTransactionException.
- SQLSTATE class 42 -> ScratchbirdSyntaxException.
- SQLSTATE class XX -> ScratchbirdInternalException.

## Performance tests
- Connection establishment time.
- Simple query latency.
- Prepared statement throughput.
- Large result streaming without memory spikes.
