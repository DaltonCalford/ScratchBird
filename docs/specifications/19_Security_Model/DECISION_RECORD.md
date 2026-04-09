# Section 19 Decision Record

Status: current_authority

## Decisions

1. Security bootstrap completes before remote exposure.
2. Security state is controlled by bootstrap configuration and local trusted material, not by catalog mutation.
3. Authentication methods terminate in a canonical authenticated-principal contract regardless of protocol family.
4. Loadable authentication extensions require explicit trust and signed-module policy.
5. Fail-open downgrade paths are forbidden.
6. Automated distributed PKI and supply-chain governance beyond current configured channels remain unsupported until explicitly implemented.

## Rejected alternatives

- late security bootstrap after listener exposure
- unsigned or ad hoc authentication modules
- catalog-stored secret authority for bootstrap keys
- silent downgrade from strong authentication or channel protection to weaker modes
