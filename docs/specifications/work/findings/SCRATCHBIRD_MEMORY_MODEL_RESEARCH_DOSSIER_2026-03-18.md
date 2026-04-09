# ScratchBird Memory Model Research Dossier

Date: 2026-03-18

## Objective
Distill the research needed for the next-generation ScratchBird memory model
and turn it into concrete specification guidance for the first implementation
wave.

## Boundary
This dossier covers:
- segmented residency domains
- admission and replacement
- workload-aware policy
- writeback pipeline redesign
- frame ownership and local concurrency
- page-type intelligence
- prefetch, fairness, and anti-thrashing
- observability and gate implications

This dossier explicitly defers full implementation of:
- NUMA specialization
- persisted warm restart heat maps
- compression-aware secondary tiers
- self-tuning policy loops

## Executive Summary
ScratchBird already has the beginnings of a mixed-workload buffer manager:
partitioned page lookup, scan rings, page-class hints, and a background writer.
The live code still behaves as one shared eviction market with one global
allocation and eviction choke point. That is the dominant architectural limit.

The donor engines converge on a consistent lesson:
- scan traffic must be contained
- admission and replacement must be separated
- dirty-page debt must be shaped continuously, not only at thresholds
- transaction and metadata pages must receive special protection
- concurrency and ownership must be localized

The recommended ScratchBird v1 direction is:
1. explicit policy domains
2. protected and probationary residency with ghost history
3. workload-class hints attached to buffer admission
4. generation-aware writeback queues and debt accounting
5. partition-local frame ownership and victim search
6. first-class telemetry and new correctness gates

## ScratchBird Baseline

### Live code observations
Based on the current implementation:
- the buffer pool uses one fixed frame array and one shared clock-style
  residency market
- lookup is partitioned, but miss, allocation, and eviction still funnel
  through a global mutex
- `Sequential`, `Vacuum`, and `BulkWrite` rings exist, but only as narrow
  access-strategy exceptions
- MGA page classes exist as hints, not as budgeted domains
- the background writer is threshold-driven rather than debt-driven
- prefetch is synchronous page pin and release batching, not a predictive
  subsystem
- configuration parses `single`, `hot_cold`, and `tablespace` layouts, but
  only `single` is accepted at runtime

### Practical consequence
The current design is better than a naive single-lock cache, but it still
allows:
- bulk scans to influence OLTP residency
- metadata and transaction-state pages to compete with disposable scan traffic
- dirty-page pressure to build until foreground work helps flush
- unrelated misses to serialize behind shared victim selection

## Donor Findings

### Donor matrix
| Topic | PostgreSQL | InnoDB | Firebird | ScratchBird direction |
| --- | --- | --- | --- | --- |
| Scan containment | Access-strategy rings for bulk read, bulk write, and vacuum keep scan traffic out of the ordinary cache path | Midpoint insertion and old-block handling bias one-touch scans away from the hot end of the LRU | Dedicated prefetch and background helpers keep scan behavior from becoming the only cache priority | Keep ring containment for scan and bulk paths, but do not treat rings as the whole cache policy |
| Admission versus replacement | Normal clock plus strategy rings; not every access behaves the same | Midpoint insertion and old-block timing distinguish fresh pages from proven-hot pages | Older LRU style and MGA-sensitive priorities emphasize keeping transactional state nearby | Use probation plus protected tiers with generation aging and ghost history |
| Dirty-page shaping | Bgwriter and checkpointer cooperate around dirty boundaries | Flush list, page cleaner threads, and batch discipline treat dirtying as a pipeline | Cache writer maintains free-page supply and can help with prefetch or cleaning | Use dirty generations, queue families, debt accounting, and multiple cleaner roles |
| MGA sensitivity | Limited because PostgreSQL is not MGA-first | Limited because InnoDB is undo-log based | Strongest donor for transaction inventory and version visibility pressure | Keep MGA semantics first; borrow mechanisms, not visibility rules |
| Prefetch | Read streams and access strategies shape scan behavior | Read-ahead has explicit pending-read limits to avoid flooding the pool | Asynchronous prefetch cooperates with cache reader and writer threads | Make prefetch speculative, budgeted, cancelable, and easy to evict |
| Warm restart | `pg_prewarm` and autoprewarm show the value of persisted hot-page knowledge | Buffer pool dump and load preserve the hottest portion of the pool | Not the primary emphasis | Reserve a post-recovery prewarm extension point, but keep it advisory |
| Concurrency | Good partitioning and atomic buffer state, but still one shared buffer arena | Multiple buffer-pool instances, page cleaner workers, and many local mutexes | Mature cache-writer and prefetch worker separation | Partition ownership, partition-local free lists, and explicit frame lifecycle states |

### Borrow and avoid guidance

#### PostgreSQL
Borrow:
- scan rings as an explicit containment tool
- pin-limit awareness for readahead and ring users
- rejection of dirty scan-ring victims when ring reuse would force bad writeback

Do not copy literally:
- a single shared residency market with only strategy exceptions is not enough
  for ScratchBird's intended mixed MGA workload

#### InnoDB
Borrow:
- midpoint-style separation of fresh and proven-hot pages
- continuous cleaner pipeline and flush-list style ordering
- prefetch and read-ahead debt limits
- warmup via persisted hot-page summaries

Do not copy literally:
- InnoDB's undo or redo ordering assumptions are not ScratchBird's recovery
  truth model

#### Firebird
Borrow:
- strong protection for transaction inventory and read-consistency support pages
- MGA-sensitive cache priorities
- cooperative background helpers for cache writer and prefetching

Do not copy literally:
- Firebird's exact TIP and commit-order implementation details; ScratchBird
  must preserve its own MGA and checkpoint contracts

## External Literature and Official Documentation

### Policy research
- ARC shows the value of combining recency, frequency, and ghost history for
  self-correcting cache policy.
- CLOCK-Pro shows how low-overhead clock structures can approximate richer
  recency and reuse behavior.
- TinyLFU reinforces the admission lesson: many one-touch items should never
  displace established residency.

### Warmup and operational guidance
- PostgreSQL `pg_prewarm` and autoprewarm show a practical model for
  background warmup after restart.
- InnoDB buffer pool dump and load show a production-tested hot-set memory
  surface that is advisory rather than recovery truth.

### Hardware locality
- Linux kernel false-sharing guidance reinforces the need to isolate hot write
  counters, align frame-header fields, and shard statistics instead of using
  global atomic storms.

## Recommended ScratchBird v1 Architecture

### 1. Policy domains
Adopt six first-wave policy domains:
- `critical_system`
- `hot_oltp`
- `read_mostly`
- `scan_bulk_ring`
- `version_undo`
- `temporary_work`

Rules:
- `critical_system` and `version_undo` have hard minimum reservations
- `scan_bulk_ring` and `temporary_work` are capped and easiest to reclaim
- `read_mostly` and `hot_oltp` compete only after domain reservations and
  pressure rules are satisfied

### 2. Admission and replacement
Use:
- first touch to probation
- second touch within a bounded generation window to protected
- direct protected admission only for a narrow set of critical page roles
- ghost history by domain and page role
- generation-based temperature decay instead of one flat recency counter

This captures the useful part of ARC, 2Q, and segmented clock without turning
the design into an opaque adaptive black box.

### 3. Workload-aware policy
Make the policy engine explicit, not incidental.

Required workload classes:
- point lookup
- index probe
- range scan
- sequential scan
- nested-loop reread
- sweep and GC maintenance
- bulk insert or bulk write
- checkpoint cleaner
- recovery replay
- speculative prefetch
- temporary work

The engine should combine:
- page-type classification
- workload hint
- current pressure state

### 4. Writeback pipeline
Replace threshold-only flushing with:
- dirty generations
- queue families by reason and priority
- cleaner workers
- checkpoint debt tracking
- device-aware batching
- write coalescing by filespace and locality

Checkpoint truth remains generation-based. Queue contents remain derived state.

### 5. Local concurrency and frame ownership
First-wave design target:
- partition-local victim search
- partition-local free lists
- separate lookup, residency-metadata, and content access coordination
- explicit frame lifecycle states for load, valid, dirty, flush, evict, and
  error paths

This is the prerequisite for later NUMA placement.

### 6. Prefetch, fairness, and thrash control
Prefetch must be:
- budgeted
- cancelable
- usefulness-tracked
- demotable on pressure

Fairness must include:
- per-session or per-query residency budgets
- per-object protection caps
- automatic pressure responses when ghost hits, churn, or useless prefetch
  spike

### 7. Observability
The design is not complete unless operators and gates can see:
- domain occupancy and pressure
- admission rejects and ghost hits
- promotion and demotion counts
- dirty debt age and queue depth
- foreground flush stalls
- lookup and pin contention
- prefetch usefulness and cancellation

## Deferred Later-Wave Features

### NUMA specialization
Research now, but defer implementation until:
- frame ownership is localized
- stats are sharded
- hot header layout is stabilized

### Persistent hot-set memory
Keep an explicit extension point for:
- root and metadata prewarm
- last-known hot working set
- post-recovery background warmup

Warmup hints must never become recovery truth.

### Compression-aware caching
Reserve an extension point for:
- cold-page compression
- compressed secondary residency
- different policy by page role

### Self-tuning loops
Do not auto-tune until:
- manual policy is stable
- telemetry is complete
- section-31 gates cover the failure modes

## Recommended Specification Changes
The authoritative spec tree should:
- add section-03 architecture docs for domains, admission and replacement,
  workload hints, prefetch fairness, and NUMA or frame ownership
- promote the existing writeback and failure docs already referenced across the
  tree
- revise sections 01, 08, 12, 20, and 31 so the memory architecture is
  reflected in config, recovery, temp, telemetry, and gate contracts

## Sources

### Local donor source files reviewed
- PostgreSQL: `freelist.c`, `bufmgr.h`, `read_stream.h`
- InnoDB: `buf0rea.cc`, `buf0buf.ic`, `buf0flu.cc`, `buf0dump.cc`,
  `ha_innodb.cc`
- Firebird: `cch.cpp`, `cch.h`, `tra.cpp`, `vio.cpp`,
  `README.read_consistency.md`

### External references
- ARC: Megiddo and Modha, "ARC: A Self-Tuning, Low Overhead Replacement Cache"
  (FAST 2003), USENIX
- CLOCK-Pro: Jiang, Chen, and Zhang, "CLOCK-Pro: An Effective Improvement of
  the CLOCK Replacement" (USENIX 2005)
- TinyLFU: Einziger et al., "TinyLFU: A Highly Efficient Cache Admission
  Policy"
- PostgreSQL `pg_prewarm` documentation: `postgresql.org/docs/current/pgprewarm.html`
- MySQL InnoDB buffer pool documentation: midpoint insertion and buffer pool
  dump or load guidance on `dev.mysql.com`
- Linux kernel documentation on false sharing and cacheline contention:
  `docs.kernel.org`
