# Section 22 Dependencies

Status: current_authority

## Upstream dependencies

- Section 08 for transaction and publication boundaries that govern catalog visibility.
- Section 13 for operator and coercion semantics reflected in emitted SBLR.
- Section 14 and 15 for domain and type payload semantics.
- Section 24 for committed catalog truth and schema epoch publication.
- Section 28 for dialect parser ownership.

## Downstream dependents

- Section 23 compiler, planner, and execution front door.
- Section 26 native protocol result rendering paths that depend on stable logical identity.
- Section 29 listener and parser orchestration for parser worker handoff.
- client-facing renderers that convert canonical execution artifacts back to supported client expectations.
