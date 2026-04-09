# Native Diagnostics SQL

## Current code-backed truth
- Diagnostics-facing parser entry points are partially anchored by the real utility and show or set parser families.
- Runtime observability truth is section-owned by section `20` and its `ObservabilityContract` and audit export surfaces.

## Boundary
- Parser-side diagnostics language is `partial`.
- Do not use this file to overclaim btree observability, page-walker repair, or broad privileged diagnostics parity.
- Defer runtime observability and redaction claims to section `20`.
