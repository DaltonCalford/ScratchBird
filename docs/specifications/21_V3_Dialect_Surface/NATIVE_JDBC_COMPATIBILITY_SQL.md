# Native JDBC Compatibility SQL

## Current code-backed truth
- A real native parser stack exists that can serve as the canonical target for driver or compatibility normalization.
- Native listener-path conformance exists and is stronger evidence than driver-only rewrite claims.

## Capability-state matrix
- `supported`:
  - parser normalization authority
- `partial`:
  - listener-path JDBC execution proof
  - current driver-promotion matrix authority
- `fail_closed`:
  - full JDBC parity

## Boundary
- Exact JDBC escape and promotion coverage is still partial.
- This file does not prove full driver-side parity for every JDBC form.
- Treat JDBC compatibility as parser normalization authority first, with deeper driver-promotion proof still open.
