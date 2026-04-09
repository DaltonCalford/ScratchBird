# Embedded Field Accessor Results

- Temporal TZ offset extract normalizes `kNoDisplayOffsetSeconds` to `0`.
- UUID timestamp extract returns `NULL` for non-time UUID versions.
- Accessor behavior is deterministic and test-validated.
