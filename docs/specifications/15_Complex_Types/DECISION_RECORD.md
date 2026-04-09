# Section 15 Complex Types Decision Record

Status: current_authority

## Active decisions aligned to code

1. the live authority for complex values is shared between TypeSystem, TypedValue, DomainManager, and extract-element runtime
2. internal complex-value framing is ScratchBird-owned; exact donor-engine binary contracts are not inherited unless directly re-audited
3. domain kinds are real and catalog-backed, but the code-backed authority is the DomainManager API and its catalog persistence path, not older exact SQL or catalog-schema prose
4. deterministic system-domain UUID generation is real and authoritative; the old exhaustive registry remains historical until re-audited row by row
5. emulated complex-type mappings are real as TypeSystem rows and mutation-boundary hints; mapping presence is not semantic parity

## Consequences

- section 15 fails closed on exact binary payload compatibility outside the audited TypedValue and DomainManager surfaces
- future implementation work should consolidate remaining capability and state vocabularies before widening operator-facing claims
