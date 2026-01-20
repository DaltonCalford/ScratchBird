# Driver Plan: R (DBI)

Status: Draft

Spec references:
- docs/specifications/beta_requirements/00_DRIVERS_AND_INTEGRATIONS_INDEX.md
- docs/specifications/drivers/ALPHA_DRIVER_BOOTSTRAP.md
- docs/specifications/beta_requirements/drivers/DRIVER_BASELINE_SPEC.md

Goal
- Deliver an R DBI-compatible driver.

Scope
- Native SBWP via C extension or Rcpp.

Plan
1) API surface
- Implement DBI methods (dbConnect, dbGetQuery, dbSendQuery).
- DSN/URL parsing and config mapping.

2) Protocol integration
- Use libscratchbird via native extension.
- TLS/auth handling and autocommit mapping.

3) Types + data frames
- Map types to R vectors/data frames.

4) Packaging
- CRAN package with native extension.

Testing
- DBI compliance tests and integration tests with listener.

Dependencies
- libscratchbird client core.
