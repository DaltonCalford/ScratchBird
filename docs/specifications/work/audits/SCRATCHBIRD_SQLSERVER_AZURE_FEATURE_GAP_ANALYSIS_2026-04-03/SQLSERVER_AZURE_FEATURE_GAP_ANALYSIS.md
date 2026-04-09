# ScratchBird SQL Server Azure Feature Gap Analysis

Date: `2026-04-03`

## Goal

Determine which major `Microsoft SQL Server`, `Azure SQL Database`, and
`Azure SQL Managed Instance` feature families are not covered by the current
ScratchBird implementation and canonical spec tree.

This report answers a narrower question than general commercial parity:

- which Microsoft feature families are still absent
- which are only represented by generic ScratchBird substrate
- which are only hinted at through SQL Server catalog placeholders
- which are already explicitly fail-closed today

## Scope And Method

Microsoft feature authority was taken from:

- the current Microsoft SQL Database Engine overview
- the current Azure SQL Database and Azure SQL Managed Instance feature
  comparison matrix
- dedicated Microsoft Learn pages for SQL Server feature families that do not
  fit cleanly into the comparison table, such as `Service Broker`, `CLR`,
  `FILESTREAM`, `FileTable`, `Query Store`, `Resource Governor`,
  `In-Memory OLTP`, `Database Mail`, `SQL Server Agent`, `linked servers`,
  and `Always On availability groups`

ScratchBird coverage was measured against:

- current canonical specs under `docs/specifications/`
- current runtime and catalog code under `src/` and `include/`
- the existing SQL Server family Beta 2 emulation placeholder at
  `docs/specifications/28_Parser_Implementations/BETA2_EMULATION_FAMILY_SQLSERVER_AZURESQL_MODEL.md`

## Current Baseline

The current ScratchBird tree already has meaningful generic substrate in a few
areas that matter to SQL Server and Azure SQL parity:

- generic `TDE` canon
- generic protected-query encryption and enclave canon
- generic row-level security canon and runtime
- generic data masking substrate
- generic columnstore substrate and OLAP canon
- generic distributed-query, external-table, text-file, and ODBC connector
  canon
- generic HA/DR, failover, and PITR canon
- generic workload isolation and classifier canon

However, that does not mean `SQL Server / Azure SQL` parity exists. The family
still lacks a real parser, a real `TDS` runtime, and donor-specific catalog,
syntax, DMV, admin, and compatibility semantics.

## Classification Counts

Across the `35` reviewed feature families:

- `11` are `FULL_GAP`
- `5` are `PLACEHOLDER_ONLY`
- `14` are `GENERIC_SPEC_ONLY_FAMILY_GAP`
- `3` are `GENERIC_IMPLEMENTED_FAMILY_GAP`
- `1` is `PARTIAL_CATALOG_AND_FAMILY_GAP`
- `1` is `EXPLICIT_BOUNDARY_GAP`

The machine-readable matrix is
`SQLSERVER_AZURE_FEATURE_GAP_MATRIX.csv`.

## Main Result

The biggest current problem is not one missing feature. It is the absence of a
real `SQL Server / Azure SQL` family implementation boundary:

1. no dedicated `TDS` adapter
2. no dedicated parser worker
3. no internal `TDS` bridge client
4. only a very small `sys.*` compatibility surface

That means even the Microsoft features for which ScratchBird already has a
generic substrate remain unavailable to SQL Server or Azure SQL clients.

## Full Gaps

These families currently have no meaningful ScratchBird canon or runtime
coverage for the Microsoft donor surface:

- `Service Broker`
- `CLR integration`
- `Change Tracking`
- `Ledger`
- `Distributed transactions / MS DTC / elastic transactions`
- `Database Mail`
- `SQL Server Agent / Azure Elastic Jobs family`
- `Azure elastic pools`
- `Azure serverless compute tier`
- `Azure Hyperscale tier`
- `Query Notifications / Event Notifications`

These are not family-mapping gaps. They are direct capability gaps.

## Placeholder-Only Gaps

These features are especially important because the current SQL Server catalog
layer already advertises columns that imply support, but there is no matching
runtime or canonical family model:

- system-versioned temporal tables
- SQL Graph tables
- `FILESTREAM`
- `FileTable`
- `In-Memory OLTP`

The key evidence is
`include/scratchbird/catalog/mssql_catalog.h`, which currently exposes fields
such as:

- `temporal_type`
- `is_node`
- `is_edge`
- `filestream_data_space_id`
- `is_filetable`
- `is_memory_optimized`
- `durability`

Those columns are useful as emulation targets, but today they are placeholders,
not proof of feature support.

## Generic ScratchBird Substrate Exists, But Microsoft Family Coverage Is Still Missing

The following Microsoft feature families are not zero-substrate gaps. In these
cases ScratchBird already has a relevant generic capability or approved Beta 2
canon, but the `SQL Server / Azure SQL` family still lacks donor-compatible
syntax, catalog, DMV, auth, or operational semantics:

- native `TDS` endpoint and parser family
- broad SQL Server and Azure catalog, DMV, and system procedure surface
- CDC donor feature surface
- PolyBase / Azure data virtualization
- Query Store
- Resource Governor
- linked servers / `OPENQUERY` / `OPENDATASOURCE`
- Always On / failover groups / active geo-replication
- `TDE`
- Always Encrypted and secure enclaves
- Microsoft Entra authentication and login semantics
- Elastic Query and cross-database remote naming
- Azure automatic tuning
- Machine Learning Services compatibility
- Windows integrated authentication and SQL login token semantics

This is the largest category in the report. It means ScratchBird already has
enough architectural base to avoid reinventing these subsystems from scratch,
but it still does not have Microsoft family compatibility.

## Generic Runtime Exists, But The Microsoft Family Is Still Unmapped

Three notable families already have live or near-live generic ScratchBird
behavior:

- dynamic data masking
- row-level security
- columnstore

For these, the gap is narrower:

- the donor parser surface is missing
- the donor catalog and DMV surface is missing
- donor DDL and metadata rules are missing
- donor error and plan behavior are missing

These are still real gaps, but they are lower-risk than the `FULL_GAP` and
`PLACEHOLDER_ONLY` classes.

## Explicit Boundary Gap

`Replication` is a special case.

ScratchBird does not merely omit it accidentally. The current listener canon
explicitly says one-way and bidirectional replication runtime is not implemented
today. That is a useful truth boundary because it prevents the SQL Server and
Azure family from being overstated.

So replication remains open in two ways:

- core runtime is still outside the current shipped boundary
- SQL Server and Azure replication-family semantics are also missing

## Features Not Counted As Current Major Gaps

The report does not count these as primary uncovered Microsoft families because
current ScratchBird canon already covers them well enough at the generic engine
level that the remaining work is mostly donor mapping, not subsystem design:

- XML core type support
- spatial core type support
- sequence support
- generic security policy layering
- generic HA/DR and PITR base contracts

That does not mean Microsoft parity is complete for those areas. It only means
they are no longer the highest-signal uncovered families.

## Priority Order

If the goal is credible SQL Server / Azure SQL parity planning, the work order
should be:

1. `TDS` endpoint, parser worker, internal bridge client, and broad catalog or
   DMV surface
2. temporal tables, linked servers, distributed transactions, and query-store
   family mapping
3. Service Broker, CLR, Agent, Database Mail, replication, and In-Memory OLTP
4. FILESTREAM, FileTable, SQL Graph, and Ledger
5. Azure-specific service tiers and control-plane-compatible behavior:
   elastic pools, serverless, Hyperscale, failover groups, active
   geo-replication, and automatic tuning

## Bottom Line

The current ScratchBird tree is not missing only a `TDS` protocol adapter. It
is missing a large amount of the Microsoft family surface above the core
storage engine.

The most important uncovered areas are:

- admin and messaging subsystems
- temporal, graph, and memory-optimized relational features
- remote-query and distributed-transaction surfaces
- SQL Server and Azure operational catalogs and DMVs
- Azure SQL service-tier behaviors

The strongest positive conclusion is that ScratchBird already has enough
generic Beta 2 canon in security, HA/DR, external-data access, distributed
query, and analytical storage to avoid a blank-sheet design in many of these
areas. The missing work is now sharply identifiable as Microsoft-family closure
work rather than general architecture invention.
