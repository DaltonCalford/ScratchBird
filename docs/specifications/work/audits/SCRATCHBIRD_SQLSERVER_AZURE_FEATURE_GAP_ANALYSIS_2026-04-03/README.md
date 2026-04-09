# ScratchBird SQL Server Azure Feature Gap Analysis 2026-04-03

This packet compares the current ScratchBird implementation and canonical spec
tree against the current Microsoft SQL Database Engine and Azure SQL feature
surface.

Contents:

- `SQLSERVER_AZURE_FEATURE_GAP_ANALYSIS.md`: narrative report
- `SQLSERVER_AZURE_FEATURE_GAP_MATRIX.csv`: feature-by-feature gap matrix

Method:

1. Use current ScratchBird code and canonical specs as the local baseline.
2. Use current official Microsoft Learn pages as the donor feature authority.
3. Classify each feature as one of:
   - `FULL_GAP`
   - `PLACEHOLDER_ONLY`
   - `GENERIC_SPEC_ONLY_FAMILY_GAP`
   - `GENERIC_IMPLEMENTED_FAMILY_GAP`
   - `EXPLICIT_BOUNDARY_GAP`
   - `PARTIAL_CATALOG_AND_FAMILY_GAP`

Scope:

- SQL Server engine features called out by the Microsoft SQL Database Engine
  overview
- Azure SQL Database and Azure SQL Managed Instance feature deltas from the
  current Microsoft comparison matrix
- family-specific protocol, parser, catalog, admin, HA/DR, and service
  features required for ScratchBird-grade SQL Server / Azure SQL compatibility

Out of scope:

- SSMS-only tooling UX
- Microsoft Fabric-only database surfaces
- deprecated legacy surfaces where Microsoft itself already marks the feature
  deprecated or replaced, unless the feature still appears in the active Azure
  comparison matrix and materially affects compatibility
