# ScratchBird Memory Model Requirement-to-Code Delta

Date: 2026-03-18

## Status Scale
- `implemented`: present in the live code in materially usable form
- `partial`: present as a narrow mechanism or hint, but not as the required
  architecture
- `absent`: not materially present in the live code

## Delta Matrix
| Requirement area | Live status | Code evidence | Spec action |
| --- | --- | --- | --- |
| Segmented policy domains | absent | `BufferPool::PoolLayout` parses future layouts, but initialization warns and falls back to one shared pool; all frames still live in one array and one residency market | add explicit domain spec and config surface |
| Protected plus probationary residency | absent | frames only carry `usage_count`; no protected or probationary split exists | add admission and replacement spec |
| Ghost history | absent | no ghost directory or eviction memory exists in the buffer pool | add ghost-history contract and observability |
| Workload-aware admission control | partial | `AccessStrategy` supports `Normal`, `Sequential`, `Vacuum`, and `BulkWrite`; classification is limited to a few page types and scan hints | add workload-class and hint precedence spec |
| Scan containment | partial | dedicated rings exist for sequential, vacuum, and bulk write traffic | keep rings, but subordinate them to domains and admission policy |
| Page-type-specific protection | partial | `MgaPageClass` and automatic classification protect `TX_STATE` more than generic pages | expand to system, index-root, version, temp, recovery, and metadata roles |
| Writeback pipeline with dirty generations | partial | background writer and dirty ratios exist; frames only track boolean dirty state in live code | revise writeback and checkpoint specs for dirty generations and queue families |
| Flush queues and checkpoint debt | absent | no distinct flush queues, debt ledger, or per-class flush priority live in the buffer pool | revise section 03 and 08 writeback contracts |
| Local concurrency around misses and eviction | partial | lookup uses partitioned page-table locks and frame content mutexes; miss and eviction still use a global mutex | add frame ownership and partition-local victim model |
| NUMA and cacheline locality | absent | no node-local pools, freelists, or header layout contract exists | specify as deferred extension with first-wave prerequisites |
| Predictive prefetch with debt limits | partial | prefetch is a synchronous pin and release batch, not an asynchronous policy subsystem | add prefetch, fairness, and debt controls |
| Anti-thrashing and fairness budgets | absent | rings reduce some scan pollution, but there are no per-session, per-object, or per-domain budgets | add fairness and thrash detector spec plus gates |
| Persistent hot-set memory | absent | startup recovery rebuilds derived state but does not preserve hot page summaries | reserve later-wave warmup extension in specs |
| Compression-aware caching | absent | no compressed secondary tier or cold-page compression policy exists | reserve later-wave extension |
| Observability for admission, domains, and debt | partial | metrics publish hits, misses, and some MGA counters, but not domain occupancy, ghost hits, queue debt, or contention breakdowns | revise section 20 observability surfaces |
| Self-tuning policy loops | absent | no feedback-driven resizing of protected share, rings, or cleaner aggressiveness | defer until telemetry and gates are mature |
| Recovery and checkpoint integration | partial | commit fence hooks and checkpoint specs exist, but live writeback is not generation-queue driven | revise sections 03 and 08 to align checkpoint and writeback |
| Correctness under stress | partial | there are buffer tests, but not the full race, crash, thrash, and failure-injection matrix required by the target design | expand section-31 gates |

## High-Value Live Implementation Notes

### Useful existing hooks
- `MgaPageClass` and `MgaFrameHints` already give ScratchBird a place to attach
  page-role and GC-sensitive policy.
- page-table partitioning provides a base for local ownership.
- access strategies and rings provide an existing containment path for scan and
  bulk traffic.
- the background writer and commit-fence hooks provide a base for queue-based
  writeback.

### Dominant architectural gaps
- one shared eviction economy still dominates behavior
- one global allocation and eviction mutex still serializes misses
- dirty tracking is not yet generation-aware in the live frame lifecycle
- no policy surface exists for ghost hits, admission rejects, or budget-based
  fairness

## First-Wave Implementation Boundary Suggested by the Delta
The codebase is ready for a first wave that:
1. extends frame metadata and policy APIs
2. adds domain accounting and observability
3. adds protected, probationary, and ghost history
4. adds workload hints and stronger page-role policy
5. replaces boolean dirty handling with queueable dirty states
6. localizes victim search and frame ownership

The codebase is not yet ready to jump directly into:
- full NUMA placement
- compression tiers
- autonomous self-tuning loops
- persisted warmup summaries as if they were core recovery state
