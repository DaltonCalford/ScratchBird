# Cache and Buffer Commercial-Grade MGA Alignment Model

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Source finding reconciled: `local_work/findings/ScratchBirdCacheandBufferSuggestions.md`
- Last reconciliation date: 2026-03-30

## Purpose

Reconcile the older cache and buffer research report with the current
ScratchBird canon so that commercial-grade buffer-manager improvements are
specified in ScratchBird terms rather than imported from WAL-oriented engines.

## Governing rule

ScratchBird may adopt commercial-grade cache and buffer improvements only when
they remain subordinate to:

- MGA visibility and reclaim truth
- forced-write and ordered-publication durability
- page-image and transaction-inventory recovery authority
- current shared-frame logical-domain architecture unless and until a later
  implementation wave proves physical segmentation

## Findings from the report that are compatible with ScratchBird

The report contains several strong ideas that align with ScratchBird goals and
should guide future hardening:

### 1. Domain-aware residency and budgeting

ScratchBird should continue to harden:

- critical-system protection
- hot OLTP protection
- read-mostly handling
- scan and bulk ring containment
- version-chain and MGA-heavy page handling
- temporary and scratch separation

This is already directionally present through logical policy domains and
residency tiers. The compatible improvement is to strengthen the governing
policy and observability, not to pretend physically separate frame pools
already exist.

### 2. Admission before replacement

The report is correct that not every read deserves durable residency in the
main protected working set.

ScratchBird-compatible next steps are:

- stronger probationary and protected promotion rules
- second-touch and direct-protect-root hardening
- real ghost-history evidence surfaces
- page-class-aware admission defaults

### 3. Page-type-specific policy

The report is correct that page roles matter.

ScratchBird should continue to use page-class-specific policy for:

- system and metadata pages
- allocation and inventory pages
- heap and ordered-index working sets
- chain-heavy MGA pages
- scan-probation pages
- temporary-work pages

This remains policy guidance only. It must not redefine transaction visibility
or recovery truth.

### 4. Dirty-generation and checkpoint-debt writeback

The report is directionally correct that elite systems avoid foreground stall
spikes by shaping dirtying and flushing continuously.

ScratchBird already has the right MGA-compatible primitives:

- dirty generations
- queue-state attribution
- checkpoint-bound drain
- background writer thresholds
- write-admission fencing on incident

Future hardening should deepen:

- debt visibility
- class-aware queue prioritization
- cleaner concurrency
- foreground stall reporting

without importing WAL-distance or redo-log authority.

### 5. Predictive prefetch with debt caps

The report is correct that prefetch is only valuable when speculative reads do
not damage residency quality.

ScratchBird-compatible hardening is:

- predictive and class-aware prefetch windows
- capped prefetch debt
- usefulness-floor-driven suppression
- cancellation when access patterns pivot
- fairness under pressure

### 6. Anti-thrashing and fairness controls

The report is correct that the buffer manager must defend itself from:

- scans larger than memory
- adversarial access
- rapid hot-set oscillation
- dirty storms
- chain-heavy MGA pressure

ScratchBird-compatible improvements include:

- per-workload and per-domain budget enforcement
- thrash-state-driven policy shifts
- debt and dirtying-rate controls
- fairness reporting and refusal behavior under pressure

### 7. Restart warmup and persistent heat summaries

The report is correct that restart behavior should become useful faster.

ScratchBird-compatible warmup means:

- prewarm hints are derivative runtime state only
- prewarm never overrides durable page truth
- prewarm priorities favor roots, system metadata, allocator pages, and known
  hot paths
- restart warmup remains optional and fail-closed

This is compatible with MGA because it is an optimization over durable page
truth, not an alternate recovery path.

### 8. Rich observability and policy traceability

The report is correct that commercial-grade cache behavior must explain itself.

ScratchBird should expose richer:

- domain hit and miss behavior
- ghost hits
- promotion and demotion events
- dirty-generation age and debt
- flush queue depth and foreground flush latency
- prefetch usefulness
- thrash-state transitions
- warmup effectiveness

## Findings from the report that are not canonical for ScratchBird

The report also imports assumptions that are not valid for ScratchBird and
must be rejected.

### 1. WAL distance awareness

`WAL` distance is not a canonical ScratchBird writeback or recovery concept.

ScratchBird may expose derivative `wal_after` metrics for replication or audit
lanes, but:

- `wal_after` is not recovery truth
- `wal_after` distance is not a flush-legality rule
- `wal_after` state must not decide commit truth or checkpoint legality

### 2. LSN-driven flush eligibility

`LSN`-driven flush eligibility is not canonical.

ScratchBird flush and checkpoint rules remain driven by:

- dirty-state lifecycle
- dirty-generation ordering
- checkpoint attribution
- forced-write fence completion
- MGA publication ordering

### 3. Redo-oriented checkpoint or recovery coupling

ScratchBird must not import language implying that restart reconstructs page
truth from redo replay or that the cache manager primarily exists to support
redo-friendly flushing.

ScratchBird restart remains:

- page-image and inventory reconciliation
- forced-write and ordered-publication based
- MGA-native

### 4. Physically segmented pools as an implied requirement

The report presents segmentation as if physical split pools are required for a
top-tier design.

ScratchBird canon does not accept that claim.

Current truth is:

- logical policy domains over a shared frame array are real
- physical split pools are optional future implementation work
- commercial-grade behavior may be reached through stronger logical policy and
  observability without pretending physical segmentation already exists

## Commercial-grade improvement program in ScratchBird terms

The next ScratchBird-compatible cache and buffer hardening wave should be
defined as:

### CG-BUF-1 Policy hardening

- stronger domain-budget enforcement
- stronger probation and protected interaction
- page-class-driven admission defaults
- direct operator-visible ghost-history evidence

### CG-BUF-2 Writeback hardening

- queue debt and generation-age reporting
- class-aware queue prioritization
- cleaner concurrency and predictive flush scheduling
- explicit foreground flush stall accounting

### CG-BUF-3 Prefetch and fairness hardening

- prefetch usefulness and cancellation controls
- debt caps and thrash-trigger suppression
- per-workload fairness controls

### CG-BUF-4 Warmup and observability hardening

- restart warmup hint model
- hot-set summary persistence as derivative state
- richer buffer-policy telemetry and top-offender reporting

## Audit rule

Any future implementation or audit work derived from the older report shall
translate its recommendations into ScratchBird-native terms before they enter
canon.

If a recommendation depends on:

- WAL truth
- redo replay
- LSN ordering
- undo-centric semantics

then it must be rewritten or rejected.

## Cross-section references

- `BUFFER_POOL_AND_FLUSH.md`
- `MGA_AWARE_BUFFER_POOL_POLICY.md`
- `MEMORY_POLICY_DOMAINS_AND_RESIDENCY_SEGMENTS.md`
- `PREFETCH_FAIRNESS_AND_THRASH_CONTROL.md`
- `../35_Durability_Crash_Recovery_and_Checkpoint_Model/DURABILITY_MODEL_AND_CORRECTNESS_BOUNDARY.md`
- `../20_Diagnostics_Audit_and_Observability/STORAGE_METRICS.md`
