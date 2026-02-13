# Driver Plan: Node.js / TypeScript

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Status: Draft

Spec references:
- /docs/specifications/parser/v3/beta_requirements/00_DRIVERS_AND_INTEGRATIONS_INDEX.md
- /docs/specifications/parser/v3/drivers/ALPHA_DRIVER_BOOTSTRAP.md
- /docs/specifications/parser/v3/beta_requirements/drivers/DRIVER_BASELINE_SPEC.md

Goal
- Deliver async Node.js driver with full TypeScript definitions.

Scope
- Promise-based API, TLS by default, network-only.

Plan
1) API design
- Connection pool, client, and query APIs (async/await).
- Named and positional parameter support (rewrite '?').

2) Protocol integration
- Bind to libscratchbird (native addon) or implement JS SBWP client.
- Support SCRAM and TLS options.

3) Types + streaming
- Implement type conversions and row streaming.
- COPY in/out stream integration for Node streams.

4) Packaging
- NPM package, prebuilt binaries or fallback build.

Testing
- Unit tests for API and type mapping.
- Integration tests with listener.
- Smoke tests with Prisma/TypeORM optional adapters.

Dependencies
- libscratchbird client core and build tooling for native addon.
