# Low-Capability Implementability Criteria (2026-02-11)

## Purpose
Define hard acceptance criteria for each canonical specification file so a low-capability, non-reasoning AI can implement without inference.

## File-Level Pass Criteria
A file is `ready` only if all checks pass:
1. Scope is explicit and bounded.
2. Data structures are explicit (fields, types, constraints, defaults).
3. Algorithms are explicit (ordered steps, deterministic branching).
4. Error behavior is explicit (codes/conditions/outcomes).
5. Security behavior is explicit (authz/authn/policy gates if applicable).
6. Concurrency or transaction behavior is explicit where relevant.
7. Persistence behavior is explicit where relevant.
8. Test gates are explicit and verifiable.
9. No unresolved placeholders (`TBD`, `TODO`, `FIXME`, `XXX`).
10. No unresolved open questions.
11. No dead file references.
12. No ambiguous requirement language requiring interpretation.

## Ambiguity Heuristics (Flag for Review)
The following terms are treated as risk markers unless qualified with deterministic rules:
- `may`, `might`, `could`, `typically`, `generally`, `as needed`, `etc`.

## Status Values
- `pending`: not reviewed yet.
- `in_review`: currently being reviewed/patched.
- `blocked`: requires design clarification.
- `ready`: passes all criteria.

## Notes
- This rubric is an audit gate for specification quality, not an implementation status marker.
- Canonical scope is `docs/specifications/[00-31]_*` only.
