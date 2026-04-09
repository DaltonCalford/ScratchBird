# Section 30 Decision Record

## Current decisions

1. The stable client boundary is expressed first through maintained linked APIs and documented tool contracts, not through internal listener or IPC headers.
2. JDBC is the broadest maintained baseline lane; other maintained drivers may be narrower and must state their own floor.
3. Standalone tools and linked APIs share transaction semantics inherited from the server model; they do not define an alternate client-side transaction model.
4. `AUTOCOMMIT` is a client/session control over post-success commit behavior, not a non-transactional mode.
5. Migration, passthrough, replication, and forensic control surfaces are bounded feature lanes; only explicitly documented shipped behavior is authoritative.
6. Installer profiles define supported packaging and artifact expectations; unsupported deployment shapes are fail-closed.

## Rejected interpretations

- Treating every shipped driver as feature-identical.
- Treating internal control-plane or parser IPC types as stable public client ABI.
- Treating native control surfaces as a blanket promise of live migration, replication, or replay parity.
- Treating client tools as able to bypass transaction, MGA, metadata, or schema-publication rules.
