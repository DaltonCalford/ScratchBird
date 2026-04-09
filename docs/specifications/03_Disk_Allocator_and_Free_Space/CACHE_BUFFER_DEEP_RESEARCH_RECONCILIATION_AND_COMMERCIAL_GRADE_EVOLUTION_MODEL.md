# Cache Buffer Deep Research Reconciliation and Commercial-Grade Evolution Model

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Source finding reconciled: `local_work/findings/buffers and cache.md`
- Last reconciliation date: 2026-03-30

## Purpose

Reconcile the older deep research report on ScratchBird cache and buffer design
against the current canonical ScratchBird specifications.

This file exists because the report mixes:

- accurate observations about the current buffer-pool substrate
- commercially useful improvement ideas
- non-canonical donor assumptions about WAL, redo, LSN, and torn-page defense

ScratchBird may adopt the useful ideas only in MGA-native terms.

## Current ScratchBird truth the report correctly recognized

The report is directionally correct that ScratchBird already has a real buffer
manager rather than a placeholder cache:

- Clock-Sweep-style usage and eviction behavior
- per-frame content locking
- partitioned page-table lookup
- prefetch and scan-oriented access-strategy hooks
- adaptive background writer behavior
- higher-layer logical caches above the page cache
- telemetry surfaces that are real but still incomplete

Those observations are compatible with current section `03`, section `20`, and
section `33` canon.

## Suggestions from the report that have merit for ScratchBird

The following ideas are good and should continue to shape commercial-grade
ScratchBird evolution.

### 1. Stronger scan resistance and admission discipline

The report is correct that ScratchBird should continue evolving from plain
Clock-Sweep behavior toward stronger admission and anti-pollution controls.

ScratchBird-compatible direction:

- probationary versus protected interaction hardening
- ring-only and scan-probation behavior for bulk paths
- promotion based on demonstrated reuse rather than every first touch
- page-class-aware protection for roots, metadata, and MGA-heavy structures

This remains compatible with the current shared-frame, logical-domain design.

### 2. Better I/O parallelism and prefetch shaping

The report is correct that current prefetch should evolve from simple
best-effort hooks toward stronger policy-driven prefetch behavior.

ScratchBird-compatible direction:

- asynchronous prefetch as an implementation option
- extent-aware and pattern-aware prefetch
- debt caps and usefulness tracking
- cancellation or suppression when access patterns pivot
- strict protection against speculative reads polluting hot working sets

### 3. Deeper operator observability

The report is correct that operator-grade cache diagnostics need to be richer.

ScratchBird-compatible direction:

- per-tablespace and per-domain residency reporting
- eviction reason reporting
- pin-wait and content-lock wait visibility
- page-table partition contention visibility
- background-writer effectiveness
- flush queue debt and stall visibility
- prefetch usefulness and wasted-prefetch reporting

### 4. Stronger configuration UX

The report is correct that the current configuration surface is richer than the
most minimal example configs disclose.

ScratchBird-compatible direction:

- one coherent operator-facing config story for buffer and writeback knobs
- explicit validation and effective-config reporting
- profile examples that match the actual accepted config surface

### 5. Safer torn-write containment

The report is correct that page checksums alone are only part of the story.

ScratchBird-compatible direction is not "become a WAL engine."
The compatible direction is:

- stronger page-image partial-write detection
- stronger quarantine and repair classification
- optional page-image torn-write defense that stays subordinate to MGA durable
  truth and forced writes

If a doublewrite-like page-image mirror is ever implemented, it must be
specified as page-image durability hardening, not as redo-log authority.

### 6. Sharding and locality as future scale paths

The report is directionally correct that sharded or multi-instance buffer
layouts may help on larger systems.

ScratchBird canon accepts this only as future hardening direction.
It does not reclassify physical multi-instance pools as current truth.

### 7. Warmup and hot-path recovery usability

The report is correct that restart usefulness can improve if the engine warms
critical structures intentionally.

ScratchBird-compatible direction:

- derivative hot-set summaries
- root and metadata prewarm
- optional background warmup after restart

This must remain derivative optimization only and must never redefine recovery
truth.

## Suggestions from the report that are not canonical for ScratchBird

### 1. WAL, redo, and LSN-centered reasoning

The report repeatedly explains durability and checkpoint sophistication in
terms borrowed from PostgreSQL and InnoDB:

- WAL-driven checkpoints
- redo-driven dirty-page strategy
- LSN-based flush eligibility

Those are not ScratchBird Alpha truth.

ScratchBird correctness remains:

- MGA page-image truth
- transaction-inventory truth
- forced-write and ordered-publication truth

### 2. Optional WAL as a normal long-term cache or checkpoint answer

The report floats "optional WAL layer" as one long-term route.
That is not canonical guidance for ScratchBird Alpha or Beta cache evolution.

If derivative logging exists, it remains:

- replication or archival support
- audit support
- non-authoritative derivative state

It does not become the canonical answer to buffer-manager correctness.

### 3. Redo-aware checkpointing as the default mental model

ScratchBird may have checkpoint epochs, queue debt, and controlled drain
behavior, but those must be expressed as MGA-native checkpoint progression, not
as redo-log pressure management.

### 4. Implying that physical multi-pool layout is already the expected truth

The report is too willing to treat multi-instance or physically split pools as
the default target.

ScratchBird canon remains:

- logical domains over shared frames are current truth
- physical pool sharding is a future implementation choice, not a current
  promise

## Commercial-grade ScratchBird evolution lane

This report usefully sharpens the commercial-grade cache roadmap into seven
ScratchBird-native tracks:

### CG-CACHE-1 Admission and scan resistance

- stronger probation and protected behavior
- page-class-aware admission
- scan and bulk pollution containment

### CG-CACHE-2 Prefetch and I/O overlap

- asynchronous prefetch option
- pattern-aware prefetch
- debt, usefulness, and cancellation controls

### CG-CACHE-3 Writeback and stall control

- deeper queue-debt visibility
- background cleaner effectiveness tracking
- foreground stall accounting
- class-aware writeback shaping

### CG-CACHE-4 Locality and concurrency scale

- page-table partition observability
- contention reduction
- optional future sharding or locality-aware pool evolution

### CG-CACHE-5 Page-image safety hardening

- stronger torn-write detection
- stronger quarantine and repair semantics
- optional page-image mirror defenses that remain anti-WAL

### CG-CACHE-6 Observability and config UX

- effective-config reporting
- per-domain and per-tablespace cache visibility
- eviction, stall, and contention diagnostics

### CG-CACHE-7 Restart warmup

- derivative warmup hints
- hot-path prewarm
- warmup usefulness metrics

## Audit rule

Future cache or buffer findings may borrow donor engine comparisons, but canon
must normalize them before adoption.

If a recommendation depends on:

- WAL truth
- redo replay
- LSN ownership
- checkpoint legality derived from a log stream

then it must be rewritten or rejected before entering ScratchBird canon.

## Cross-section references

- `BUFFER_POOL_AND_FLUSH.md`
- `ADMISSION_REPLACEMENT_AND_GHOST_HISTORY.md`
- `MEMORY_POLICY_DOMAINS_AND_RESIDENCY_SEGMENTS.md`
- `PREFETCH_FAIRNESS_AND_THRASH_CONTROL.md`
- `BACKGROUND_WRITER_DIRTY_PAGE_TRACKING_AND_WRITEBACK.md`
- `../20_Diagnostics_Audit_and_Observability/STORAGE_METRICS.md`
- `../35_Durability_Crash_Recovery_and_Checkpoint_Model/DURABILITY_MODEL_AND_CORRECTNESS_BOUNDARY.md`
