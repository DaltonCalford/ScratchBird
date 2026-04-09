# ScratchBird SQL Server Azure Native Equivalent Analysis

Date: `2026-04-03`

## Goal

Reframe the existing `SQL Server / Azure SQL` donor gap set into
`ScratchBird-native` capability work.

This report does **not** ask how to emulate Microsoft surfaces.

It asks:

- which Microsoft feature families imply a useful native ScratchBird subsystem
- which of those subsystems are already generically specified
- which are still absent or underspecified
- which Microsoft rows are donor-only compatibility work and should not drive
  native product design

## Method

The starting point was the existing donor-oriented packet at
`SCRATCHBIRD_SQLSERVER_AZURE_FEATURE_GAP_ANALYSIS_2026-04-03/`.

Those `35` donor rows were then regrouped into native capability families using:

- current ScratchBird canonical specs and runtime code
- open-source or open-standard references already captured locally
- whitepapers and third-party implementation anchors for areas where the
  current ScratchBird tree is still thin

The native-equivalent direction is constrained by current ScratchBird design:

- MGA truth remains authoritative
- parser and protocol surfaces stay outside core engine semantics
- UUID identity remains the internal authority model
- new families should extend current Beta 2 canon instead of inventing a
  separate Microsoft-shaped subsystem

## Reclassification Rule

Three donor rows are **not** native feature gaps:

- `TDS` endpoint, parser worker, and internal bridge client
- SQL Server `sys.*`, `DMV`, and system-procedure compatibility surface
- donor login or token semantics such as SQL logins, Windows-integrated login
  tokens, and Microsoft Entra object exposure

Those matter only if ScratchBird chooses SQL Server family emulation. They do
not define native product capability.

Everything else was collapsed into native families.

## Classification Counts

Across `23` native-equivalent rows:

- `6` are `NEW_NATIVE_SPEC_REQUIRED`
- `8` are `EXPAND_EXISTING_GENERIC_SPEC`
- `6` are `IMPLEMENT_EXISTING_GENERIC_SPEC`
- `3` are `DONOR_ONLY_NOT_REQUIRED`

The machine-readable matrix is
`SQLSERVER_AZURE_NATIVE_EQUIVALENT_MATRIX.csv`.

## Donor-Only Rows That Should Not Drive Native Design

These should remain outside the native roadmap unless full SQL Server or Azure
emulation is explicitly chosen:

- Microsoft wire protocol and parser family
- Microsoft catalog, DMV, and system-procedure compatibility overlays
- Microsoft login-token semantics and donor identity object shapes

ScratchBird should instead keep its own:

- listener and parser family model
- UUID-based internal object identity
- native auth-plugin and role model
- native admin and observability surfaces

## New Native Families Required

These are real product gaps, not donor-only compatibility gaps.

### 1. Transactional Eventing, Durable Queues, And Notifications

Microsoft source families:

- `Service Broker`
- `Query Notifications`
- `Event Notifications`

ScratchBird does not yet have a first-class transactional event bus or durable
queue subsystem. The best open-source references in the current local packet are:

- PostgreSQL `LISTEN/NOTIFY`
- `pgmq`
- `CloudEvents`

The ScratchBird-native equivalent should be:

- durable queue tables
- activation workers
- event publication envelopes
- replay-safe delivery state
- tenant-aware notification routing

### 2. Scheduled Jobs, Alerting, And Operator Messaging

Microsoft source families:

- `SQL Server Agent`
- `Azure Elastic Jobs`
- `Database Mail`

ScratchBird does not yet have a first-class database scheduler or notification
sink family. `pg_cron` is the strongest local open implementation anchor for
database-resident scheduling. `CloudEvents` should shape emitted events and
alerts.

### 3. Native Changefeed And Consumer Offset Model

Microsoft source families:

- `CDC`
- `Change Tracking`

ScratchBird has migration and replay-oriented change capture canon, but not a
native changefeed with stable consumer offsets, commit envelopes, or projection
policies. The best local open references are:

- PostgreSQL logical decoding
- Debezium
- CloudEvents

### 4. Tamper-Evident Ledger And Attestation

Microsoft source family:

- `Ledger`

ScratchBird has temporal lineage and audit canon, but not a real tamper-evident
digest chain or attestation export. The best local open references are:

- `Trillian`
- `immudb` auditor guidance

### 5. Transactional Blob/File Namespace Tables

Microsoft source families:

- `FILESTREAM`
- `FileTable`

ScratchBird already has object-store and open-table canon, but it does not yet
define a native table-bound blob namespace with path metadata, transactional
catalog truth, and governed file exposure. This needs a dedicated native spec
instead of a donor-shaped clone.

### 6. Distributed Atomic Coordination And Cross-Resource Commit

Microsoft source families:

- `MS DTC`
- `elastic transactions`

ScratchBird does not yet have a canonical distributed-transaction family.
Useful local open references are:

- PostgreSQL two-phase commit documentation
- FoundationDB transaction-processing documentation

## Existing Generic Canon That Must Be Expanded

These families already have meaningful native substrate, but the current spec
tree is not yet complete enough to claim a full native equivalent.

### 1. Managed Safe Extensibility Runtime

Microsoft source families:

- `CLR integration`
- `Machine Learning Services`

ScratchBird already has large Beta 2 UDR and analytical package canon, but it
lacks a safe managed runtime host. The best native direction is a
`WASM/WASI`-based extensibility model rather than any CLR clone.

### 2. Temporal Relational Versioning And Time-Travel Queries

Microsoft source family:

- system-versioned temporal tables and `FOR SYSTEM_TIME`

ScratchBird already has temporal clauses, MGA lineage, archive, and replay
canon. What is still missing is the full relational object model:

- history-table binding rules
- system-time row version publication
- deterministic temporal query lowering

The strongest local open reference is `XTDB` time semantics.

### 3. Property Graph Storage And Pattern Matching

Microsoft source family:

- `SQL Graph`

ScratchBird already has graph-science UDR canon, but not a graph storage or
pattern-matching surface. The right native model is a property-graph overlay
with explicit graph catalogs and a graph query surface informed by:

- `Apache AGE`
- `openCypher`

### 4. Workload Governance, Service Classes, And Tenant Budgets

Microsoft source families:

- `Resource Governor`
- `elastic pools`

ScratchBird already has Beta 2 canon for hard tenant isolation, quotas, QoS,
service classes, and OLTP-specialized runtime lanes. The missing work is the
operator-facing control plane and policy model that turns those pieces into a
coherent service product.

### 5. Autosuspend And Autoscale Compute Control Plane

Microsoft source family:

- Azure SQL `serverless`

ScratchBird has cloud-scope and workload-governance canon, but not a complete
autosuspend, resume, warm-start, and cost-policy model for a serverless tier.
This needs a native control-plane spec, not Azure compatibility semantics.

### 6. Replicated Topology, Read Scale-Out, And Geo Failover

Microsoft source families:

- `Always On`
- `failover groups`
- `active geo-replication`
- `replication`
- `Hyperscale`

ScratchBird now has Beta 2 HA/DR, PITR, failover, distributed query, sharding,
and cross-machine planner canon, but the combined read-scale-out and
geo-topology family is still incomplete. The missing work is:

- replication runtime and role publication
- managed read routing
- geo placement and failover policy
- scale-out storage topology where required

### 7. Hot-Row Memory-Optimized OLTP Lane

Microsoft source family:

- `In-Memory OLTP`

ScratchBird already has strong Beta 2 OLTP canon, but not a clearly admitted
memory-optimized row family or compiled OLTP kernel contract. The strongest
local references are the `H-Store` and `Silo` papers.

### 8. Enterprise Identity Federation And Token Auth

Microsoft source families:

- Windows integrated authentication
- Microsoft Entra authentication

ScratchBird already has auth-plugin canon and some Kerberos substrate. The
remaining native work is broader identity federation:

- token-based auth
- group and claim mapping
- external principal binding
- recovery and operator override rules

## Existing Native Canon That Mostly Needs Implementation

These are no longer major native design gaps. They are primarily implementation
or final-closure work.

### 1. External Data Virtualization And Remote Federation

Microsoft source families:

- `PolyBase`
- `linked servers`
- `OPENQUERY`
- `OPENDATASOURCE`
- `Elastic Query`

ScratchBird already has strong native canon here through:

- distributed query decomposition
- ODBC datasource CRUD and remote SQL UDR
- external-table manifest and snapshot catalog
- open table format and object-store table canon

The remaining work is implementation and product integration, not blank-sheet
architecture.

### 2. Plan Store, Baseline Forcing, And Managed Tuning

Microsoft source families:

- `Query Store`
- Azure automatic tuning

ScratchBird already has native plan-store and adaptive-planning canon. The main
remaining work is runtime delivery and operator workflow.

### 3. Transparent At-Rest Encryption And Rekey

Microsoft source family:

- `TDE`

The native family already exists in canonical form.

### 4. Protected Query Encryption And Enclave Execution

Microsoft source family:

- `Always Encrypted` with secure enclaves

The native family already exists in canonical form.

### 5. Row Security And Dynamic Masking

Microsoft source families:

- `Row-Level Security`
- `Dynamic Data Masking`

The native families already exist and have partial runtime substrate.

### 6. Analytical Columnstore And OLAP Acceleration

Microsoft source family:

- `Columnstore`

ScratchBird already has columnstore and OLAP Beta 2 canon. The remaining gap
is delivery quality, not feature invention.

## Native Priority Order

If the goal is “match or exceed the useful service behind the Microsoft
features” without trying to become Microsoft-shaped, the work order should be:

1. transactional eventing, durable queues, scheduled jobs, and notification
   sinks
2. native changefeed, temporal relational versioning, and tamper-evident
   ledger
3. hot-row OLTP lane, distributed atomic coordination, and replicated topology
4. managed safe extensibility runtime
5. property graph storage and pattern matching
6. file/blob namespace tables
7. service-tier control plane: tenant budgets, autoscale, autosuspend, and
   read-scale routing
8. implementation closure for federation, plan store, encryption, masking, RLS,
   and columnstore

## Bottom Line

The donor report was correct for emulation, but it overstates what should drive
the native ScratchBird roadmap.

For native ScratchBird design:

- Microsoft-only protocol and catalog surfaces are mostly irrelevant
- the real missing product families are eventing, jobs, changefeeds, ledger,
  distributed commit, file/blob namespace tables, and a stronger OLTP and
  service-tier control plane
- several large “Microsoft gaps” are already covered by current ScratchBird
  Beta 2 canon and should now be treated as implementation work rather than new
  design work

The strongest open-source design anchors in the current local reference packet
are:

- PostgreSQL `LISTEN/NOTIFY`
- `pgmq`
- `pg_cron`
- PostgreSQL logical decoding
- `Debezium`
- `CloudEvents`
- `XTDB`
- `Trillian`
- `immudb`
- `Apache AGE`
- `openCypher`
- `WebAssembly`
- `WASI`
- PostgreSQL foreign-data-wrapper documentation
- PostgreSQL two-phase commit
- FoundationDB transaction-processing documentation
- `H-Store`
- `Silo`

That is enough to drive a native ScratchBird-equivalent design program without
copying Microsoft-specific protocol or catalog behavior.
