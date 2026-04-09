# Section 25 Dependencies

## Primary dependencies

- section `17` for cluster fabric and UDR runtime semantics where current section `25` only carries a boundary note
- section `19` for startup, bootstrap, encryption, and security-gate semantics
- section `20` for observability, audit, and operator diagnostics
- section `23` for planner, optimizer, and parallel execution semantics
- section `24` for catalog families, role vocabulary, and governance metadata families
- section `29` for listener lifecycle, startup control, and network-full-stack orchestration

## Downstream dependents

- section `30` for client-tooling interaction with runtime-mode controls
- section `31` for runtime, maintenance, and reliability gate coverage

## Non-ownership

Section `25` does not own protocol framing, handshake, parser behavior, cluster consensus, or MGA recovery semantics.
