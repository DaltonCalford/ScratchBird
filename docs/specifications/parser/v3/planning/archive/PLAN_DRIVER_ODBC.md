# Driver Plan: ODBC

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Status: Draft

Spec references:
- /docs/specifications/parser/v3/drivers/ODBC_DRIVER_SPECIFICATION.md
- /docs/specifications/parser/v3/drivers/ALPHA_DRIVER_BOOTSTRAP.md
- /docs/specifications/parser/v3/beta_requirements/drivers/DRIVER_BASELINE_SPEC.md

Goal
- Deliver ODBC 3.8-compliant driver using native SBWP via libscratchbird.

Scope
- Core/Basic ODBC for Alpha (per spec limitations).
- Network-only connections (listener on port 3092), TLS by default.

Plan
1) Driver manager integration
- Implement handle types (ENV/DBC/STMT/DESC) and lifecycle.
- Support Unicode and ANSI entry points.

2) Connection handling
- Parse DSN/DSN-less connection strings; map to common config.
- Implement TLS and SCRAM auth using libscratchbird.

3) Statement execution
- Map ODBC calls to SIMPLE/EXTENDED protocol.
- Implement parameter binding and result set iteration.
- Add autocommit and explicit transaction APIs.

4) Error and SQLSTATE mapping
- Map server SQLSTATE to ODBC diagnostics and SQLGetDiagRec.

5) Packaging
- Build artifacts for Linux/macOS/Windows naming conventions.
- Provide odbcinst.ini/odbc.ini examples.

Testing
- Unit tests for handle lifecycle and error mapping.
- Integration tests for connect/query/prepare/copy, DSN parsing.
- Compatibility tests with unixODBC/iODBC and Excel/BI tools smoke.

Dependencies
- libscratchbird client core stabilization.
- SBWP v1.1 auth + TLS complete.
