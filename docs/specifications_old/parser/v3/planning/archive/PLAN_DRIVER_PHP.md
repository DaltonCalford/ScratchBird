# Driver Plan: PHP (PDO / mysqli compatibility)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Status: Draft

Spec references:
- /docs/specifications/parser/v3/beta_requirements/00_DRIVERS_AND_INTEGRATIONS_INDEX.md
- /docs/specifications/parser/v3/drivers/ALPHA_DRIVER_BOOTSTRAP.md
- /docs/specifications/parser/v3/beta_requirements/drivers/DRIVER_BASELINE_SPEC.md

Goal
- Deliver PDO driver (and optional mysqli compatibility layer) using SBWP.

Scope
- Network-only, TLS by default.

Plan
1) PDO driver skeleton
- Implement PDO, PDOStatement, and error handling.
- DSN parsing and config normalization.

2) Protocol integration
- Use libscratchbird via PHP extension.
- TLS/auth handling and autocommit mapping.

3) Types + streaming
- Implement fetch modes and streaming large results.

4) Packaging
- PECL distribution and distro packages.

Testing
- Unit tests with PDO test suite.
- Integration tests with listener.
- Smoke with Laravel and WordPress.

Dependencies
- libscratchbird client core.
