# Implementation Notes

- Source scope centered on `NATIVE_ADMIN_LANGUAGE_DEFINITION.md` with targeted rule imports from listener/storage/normalization specs.
- `ADMIN_FEATURE_KEY_MAP.csv` includes one row per unique admin feature key in canonical section-21 admin definition.
- `ADMIN_ACCEPT_REJECT_MATRIX.csv` includes deterministic positive, rewrite, and rejection cases to prevent inference in downstream implementation.
