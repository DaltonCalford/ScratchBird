# Driver Plan: C# / .NET (ADO.NET)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Status: Draft

Spec references:
- /docs/specifications/parser/v3/beta_requirements/00_DRIVERS_AND_INTEGRATIONS_INDEX.md
- /docs/specifications/parser/v3/drivers/ALPHA_DRIVER_BOOTSTRAP.md
- /docs/specifications/parser/v3/beta_requirements/drivers/DRIVER_BASELINE_SPEC.md

Goal
- Deliver an ADO.NET provider for .NET 6+ using SBWP.

Scope
- IDbConnection, IDbCommand, IDataReader implementations.

Plan
1) Provider skeleton
- Implement DbProviderFactory, DbConnection, DbCommand, DbDataReader.
- Connection string builder and pooling.

2) Protocol integration
- Use libscratchbird via P/Invoke or .NET native SBWP client.
- TLS/auth handling and autocommit mapping.

3) Parameter binding + types
- Implement DbParameter and type conversions.

4) Async + cancellation
- Provide async APIs and cancellation tokens.

5) Packaging
- NuGet package, native dependencies if needed.

Testing
- Unit tests for provider behavior.
- Integration tests with listener.
- Smoke with Entity Framework Core provider (optional).

Dependencies
- libscratchbird client core or managed SBWP client.
