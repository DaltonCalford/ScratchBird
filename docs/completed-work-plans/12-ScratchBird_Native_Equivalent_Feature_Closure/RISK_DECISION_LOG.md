# Risk Decision Log

## Initial Recorded Risks

### R-12-001 Section Placement Drift

- Risk:
  - new native families such as eventing or jobs may straddle multiple numbered
    sections
- Current decision:
  - primary file path is frozen per ticket in
    `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`
  - cross-section updates are allowed, but only one primary owner file may be
    named per ticket

### R-12-002 MGA Drift In Distributed Coordination

- Risk:
  - distributed atomic coordination work may accidentally introduce
    WAL-authoritative or external-coordinator truth
- Current decision:
  - every related ticket must explicitly state which state remains derivative
    and what local MGA truth remains authoritative

### R-12-003 Managed Runtime Scope Explosion

- Risk:
  - managed extensibility may drift into “embed whole language runtime”
- Current decision:
  - the package targets safe managed runtime admission, capability policy,
    packaging, and executor binding, not unrestricted general-purpose runtime

### R-12-004 Donor-Only Scope Reintroduction

- Risk:
  - SQL Server catalog, protocol, or DMV work may leak back into the package
- Current decision:
  - donor-only rows are explicitly frozen out of scope in
    `CANONICAL_GAP_REGISTER.md`
