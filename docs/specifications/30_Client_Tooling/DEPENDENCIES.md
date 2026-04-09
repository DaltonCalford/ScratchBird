# Section 30 Dependencies

Section 30 depends on the following canonical sections:

- section `08` for transaction lifecycle and autocommit semantics
- section `09` for locking and conflict behavior surfaced to clients
- section `24` for metadata publication and DDL visibility boundaries
- section `26` for protocol and handshake contracts
- section `27` for security, authentication, and authorization behavior
- section `28` for parser and translation behavior exposed through SQL-facing clients
- section `29` for listener orchestration and session establishment behavior
- section `35` for MGA durability and recovery semantics that shape client-visible commit and restart behavior
- section `37` for schema and metadata invalidation behavior reflected in clients and tools

Section 30 is a consumer-facing boundary. It must not redefine server behavior established in those sections.
