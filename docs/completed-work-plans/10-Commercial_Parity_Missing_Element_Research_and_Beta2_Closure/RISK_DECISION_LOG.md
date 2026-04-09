# Risk Decision Log

## Initial Risks

### R1 Oracle-style or SQL Server-style terminology drift into core MGA truth

- Status: open
- Impact: high
- Response:
  - every design must restate MGA authority explicitly
  - donor log or redo semantics may only appear as optional derivative lanes

### R2 Uncontrolled scope expansion during research

- Status: open
- Impact: high
- Response:
  - use `CANONICAL_GAP_REGISTER.md` as the bounded scope controller
  - record prerequisites here before adding any new ticket

### R3 Web research downloads become incomplete or non-reproducible

- Status: open
- Impact: medium
- Response:
  - every downloaded source must be indexed in the workspace-library manifests
  - every research packet must cite local downloaded copies, not just URLs

### R4 Existing canon overclaims current reality

- Status: open
- Impact: medium
- Response:
  - if research proves drift, narrow or supersede the old file explicitly
  - do not stack new Beta 2 files on top of unresolved contradictions

### R5 Commercial-grade target grows beyond Beta 2 practical boundary

- Status: open
- Impact: medium
- Response:
  - keep all closure files Beta 2 as requested
  - if a subfeature should remain deferred, document it explicitly inside the
    owning Beta 2 file as an exclusion or later gate, not as silent omission
