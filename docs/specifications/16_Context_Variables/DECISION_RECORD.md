# Decision Record

## DR-16-01: ConnectionContext is the primary current-state authority

ConnectionContext currently owns the session and identity state that section 16 documents. That includes current user, session user, current schema, search path, dialect tag, and generic session variables.

## DR-16-02: SHOW is a bounded exposure layer, not a universal registry

The executor exposes a real SHOW surface, but it is bounded to directly implemented names and generic session-variable fallback. Section 16 treats SHOW as a bounded view over runtime state, not as proof of a universal typed variable catalog.

## DR-16-03: unknown SHOW names are part of the canonical fail-closed contract

Unknown SHOW names are rejected explicitly. That negative path is part of the canonical section contract.

## DR-16-04: transaction and statement scope stay bounded here

Section 08 owns transaction semantics. Section 16 only owns the current exposure boundary for transaction and statement state, such as SHOW transaction_isolation and SHOW statement_timeout.

## DR-16-05: row context is internal until a public front-door contract exists

Executor-local row and trigger context are real. Public row-variable syntax remains out of contract until there is direct front-door proof.
