# Decision Record

## Current decisions

1. Persisted catalog families and virtual overlay families are separate authorities and must not be conflated.
2. `emulation_profile` is a real runtime gate for engine-specific virtual overlay registration.
3. System object visibility is handler-driven and profile-gated, not donor-parity-driven.
4. Schema publication and invalidation are commit-bound under the shared MGA transaction model.
5. Branch and changeset catalog narratives are not current implementation authority.
6. Resource loading is authoritative only where current loader-backed startup paths prove it.

## Rejected interpretations

- Treating every documented catalog family as equally implemented.
- Treating donor-engine overlay narratives as proof of current runtime parity.
- Treating startup anchors as proof of a universal bootstrap checksum or installation-order framework.
