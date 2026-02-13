# Driver Plan: JDBC

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Status: Draft

Spec references:
- /docs/specifications/parser/v3/drivers/JDBC_DRIVER_SPECIFICATION.md
- /docs/specifications/parser/v3/drivers/ALPHA_DRIVER_BOOTSTRAP.md
- /docs/specifications/parser/v3/beta_requirements/drivers/DRIVER_BASELINE_SPEC.md

Goal
- Deliver a JDBC 4.2/4.3 Type 4 driver using SBWP over TLS.

Scope
- Pure Java driver, SPI auto-loading, network-only.

Plan
1) Driver skeleton
- Implement Driver SPI, URL parsing, property validation.
- Provide basic DriverPropertyInfo output.

2) Connection + session
- Implement Connection, Statement, PreparedStatement, ResultSet.
- Map autocommit and transaction isolation.
- Support schema switching and metadata APIs.

3) Protocol layer
- Integrate with libscratchbird (or Java-native SBWP client if required).
- Support SCRAM, TLS, and connection properties.

4) Data types + batching
- Implement type conversions per baseline spec.
- Add batch execution and fetch-size streaming.

5) Packaging + docs
- Maven/Gradle artifacts, shaded jar option.

Testing
- Unit tests for URL parsing, metadata, type conversions.
- Integration tests: connect, query, prepared, batch, copy/streaming.
- Smoke with HikariCP and DBeaver.

Dependencies
- libscratchbird client core or Java SBWP client implementation.
- SBWP v1.1 full support.
