# Implementation Plan

## Milestones
1. Transport + TLS + AUTH (SCRAM) + STARTUP/READY handshake.
2. Simple QUERY + result decoding (text and binary).
3. Prepared statements (PARSE/BIND/EXECUTE) + parameter binding.
4. Transactions + cancellation + timeouts.
5. Full type mapping (JSON, JSONB: `scratchbird.JSONB`, RANGE: `scratchbird.Range[T]`, GEOMETRY: `scratchbird.Geometry`, arrays, UUID, vector, network, spatial).
6. Connection pooling (if the language standard supports it).
7. Performance tuning: binary by default, compression, statement cache.
8. Framework integration (ORM/driver API compliance).
