# Data Proxy and Migration Runtime Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines how ScratchBird proxies remote data paths and executes migration work, especially when the donor system does not provide natural replication, durable change streams, or forceable transaction semantics.

## Core Invariants

1. ScratchBird target truth remains MGA-based at all times.
2. Donor state is never treated as ScratchBird truth until it is committed into ScratchBird under ScratchBird transaction rules.
3. Passthrough execution never upgrades a weak donor into a stronger consistency class than the donor actually provides.
4. A donor that cannot provide natural replication or transaction forcing must use staged extraction, verification, and explicit cutover assessment.
5. Listener, manager, and client surfaces may route or expose migration state, but committed engine state remains authoritative.

## Runtime Classes

1. `passthrough_query`
   - remote read execution with local policy gating
   - no implied local MGA visibility guarantee

2. `passthrough_dml`
   - remote write execution only when explicitly permitted by policy
   - never treated as equivalent to local MGA commit publication

3. `staged_copy_migration`
   - bounded extraction from donor into ScratchBird staging or target tables
   - no automatic cutover

4. `verified_cutover_migration`
   - staged copy followed by deterministic reconciliation and explicit cutover

5. `shadow_or_archive_assisted_migration`
   - migration aided by derivative archive, shadow, audit capture, or donor-export artifacts when available

6. `dual_read_audit_migration`
   - request fanout or compare mode across donor and ScratchBird target
   - mismatch capture without donor-truth reclassification

## Donor Capability Classes

- `native_transactional_stream`
  - donor supplies a stable ordered change stream with transaction identity

- `transactional_but_no_stream`
  - donor supplies transactional reads and writes but no durable ordered stream

- `statement_consistent_only`
  - donor supplies statement-level consistency but not reliable multi-statement transaction forcing for migration

- `weak_or_non_transactional`
  - donor supplies neither reliable transaction forcing nor durable ordered change streams

The runtime must choose migration procedure by donor capability class. It must not advertise natural replication, dual-write safety, or verified cutover guarantees where the donor cannot support the underlying proof.

## Extraction Mode Classes

- `STREAM_APPLY`
- `SNAPSHOT_COPY`
- `WATERMARK_CHUNK_SCAN`
- `WINDOWED_RESCAN_VERIFY`
- `EXPORT_IMPORT_WITH_RECONCILIATION`
- `ARCHIVE_ASSISTED_VERIFY`

Each migration run must bind exactly one active extraction mode plus one verification mode.

## Required Procedure by Donor Class

### 1. `native_transactional_stream`

Required procedure:
1. capture connector capability snapshot
2. bind stream identity and ordering scope
3. start staged apply into ScratchBird under MGA transactions
4. record per-batch reconciliation evidence
5. require cutover readiness assessment before routing change

### 2. `transactional_but_no_stream`

Required procedure:
1. take bounded donor snapshot
2. record snapshot boundary identity
3. copy in chunks with deterministic cursor identity
4. perform at least one verification rescan before cutover assessment
5. require donor-side write freeze, routed write fence, or equivalent admin gate before final cutover

### 3. `statement_consistent_only`

Required procedure:
1. declare statement-consistent-only capability explicitly
2. choose watermark key or extraction cut line
3. run ordered chunk extraction with overlap window
4. rescan overlap ranges until mismatch count reaches zero or policy refuses cutover
5. require explicit operator assessment before cutover

### 4. `weak_or_non_transactional`

Required procedure:
1. declare weak capability class explicitly
2. choose chunking or watermark rules
3. record extraction window identity
4. run copy under explicit verification passes
5. require cutover assessment before promotion
6. preserve reconciliation evidence and unresolved-drift counts
7. refuse final promotion when policy requires stronger guarantees than the donor can provide

## Passthrough Rules

### `passthrough_query`

- allowed only when remote passthrough policy admits the query class
- result classification must state that visibility is donor-defined, not local MGA-defined
- local joins, local write adjacency, and local transaction claims require explicit policy support and must fail closed otherwise

### `passthrough_dml`

- allowed only when remote passthrough policy admits remote write behavior
- success reflects remote system acceptance only
- no local commit truth, no local rollback substitution, and no local savepoint guarantee may be implied
- routing must preserve the source connector identity and policy version used

## Verified Cutover Procedure

Required ordered procedure:
1. resolve migration definition and committed mode version
2. resolve donor capability class
3. verify target schema mapping and connector configuration
4. verify prior copy state and mismatch inventory
5. run final verification pass for the chosen extraction mode
6. classify unresolved drift
7. refuse cutover if unresolved drift exceeds policy or donor class cannot support the requested guarantee
8. record cutover readiness assessment
9. apply committed routing change inside a ScratchBird transaction
10. emit committed event, audit evidence, and post-cutover continuity markers

## Weak-Donor Reconciliation Algorithm

For `statement_consistent_only` and `weak_or_non_transactional` donors:

1. choose stable chunk key, watermark key, or exported-order key
2. copy chunk `N`
3. compute chunk identity and verification checksum set
4. advance watermark only after chunk evidence is persisted
5. rescan overlap window covering chunk `N - 1`, `N`, and current tail
6. classify drift as:
   - `NONE`
   - `RETRYABLE`
   - `SOURCE_MUTATING`
   - `UNRESOLVABLE_WITH_CURRENT_POLICY`
7. repeat rescan until:
   - drift resolves
   - retry budget is exhausted
   - operator policy refuses promotion

## Failure and Refusal Rules

The runtime must refuse migration or proxy modes that would require pretending a non-transactional donor has stronger guarantees than it actually has.

Mandatory refusal classes:
- donor capability overstated
- stream ordering cannot be proven
- extraction window identity missing
- unresolved drift above policy
- remote passthrough policy absent or stale
- cutover requested without committed readiness assessment

## Current Code-Backed Authority

Current code-backed substrate exists for:
- migration catalog families and root-page backfill
- replication channel, retry, cursor, and apply-log catalog families
- secret-redacted replication error capture
- generic migration lock and migration-discovery manager substrate

These code-backed surfaces prove catalog and bookkeeping substrate. They do not by themselves prove a fully shipped listener-driven cutover runtime.

## Reconstructed Required Expansion

The rebuilt canon additionally requires:
- explicit donor capability assessment and recording
- exact weak-donor reconciliation loops
- committed cutover assessment and refusal logic
- deterministic passthrough visibility disclaimers
- audit, mismatch, and drift evidence preserved across cutover and retirement
