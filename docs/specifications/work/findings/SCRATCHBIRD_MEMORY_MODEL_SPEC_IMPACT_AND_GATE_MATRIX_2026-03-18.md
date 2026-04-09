# ScratchBird Memory Model Spec Impact and Gate Matrix

Date: 2026-03-18

## v1 Boundary Decision
The first specification wave should make the following authoritative:
- policy domains
- admission and replacement
- workload hints and page-role mapping
- writeback pipeline and dirty-debt accounting
- local frame ownership and contention surfaces
- prefetch fairness and anti-thrashing
- telemetry and gate coverage

The first wave should reserve but not require:
- NUMA-specific pool placement
- persisted warmup heat maps
- compression tiers
- self-tuning policy loops

## New Authoritative Section-03 Documents
| File | Role |
| --- | --- |
| `MEMORY_POLICY_DOMAINS_AND_RESIDENCY_SEGMENTS.md` | defines the segmented residency market and domain budgets |
| `ADMISSION_REPLACEMENT_AND_GHOST_HISTORY.md` | defines probation, protected, ghost, and generation aging |
| `WORKLOAD_CLASSIFICATION_AND_POLICY_HINTS.md` | defines hint taxonomy and precedence with page-type classification |
| `PREFETCH_FAIRNESS_AND_THRASH_CONTROL.md` | defines speculative read policy, debt limits, and fairness rules |
| `NUMA_LOCALITY_AND_FRAME_OWNERSHIP.md` | defines frame lifecycle ownership, local concurrency, and later NUMA hooks |

## Existing Section-03 Documents to Revise
| File | Reason |
| --- | --- |
| `BUFFER_POOL_AND_FLUSH.md` | elevate the buffer contract from one pool to segmented domains, richer frame metadata, and queueable dirty states |
| `BACKGROUND_WRITER_DIRTY_PAGE_TRACKING_AND_WRITEBACK.md` | turn threshold flushing into a queue-based writeback pipeline |
| `WRITEBACK_FAILURE_AND_DISK_FULL_POLICY.md` | tag failures by queue, domain, and durability debt |
| `MGA_AWARE_BUFFER_POOL_POLICY.md` | align MGA page classes with the new domain and hint model |
| `BUFFER_WRITE_SHAPING_AND_FRAGMENTATION_CONTROL.md` | bind append shaping to the new writeback pipeline and bulk-write domain |
| `BUFFER_CONFIGURATION_AND_DEPLOYMENT_PROFILES.md` | add domain, admission, prefetch, fairness, and cleaner configuration |
| `BUFFER_GC_COORDINATION_AND_PAGE_MAINTENANCE.md` | align GC scheduling with version-domain reservations and anti-thrash rules |
| `SPEC_OUTLINE.md` | refresh the authoritative architecture map |
| `TEST_CONTRACT.md` | require new policy and correctness tests |

## Cross-Section Revisions

### Section 01
| File | Reason |
| --- | --- |
| `CONFIG_CATALOG_AND_BOOTSTRAP.md` | add canonical config families for domains, admission, writeback, prefetch, and fairness |
| `CONFIG_DEFAULTS.md` | set explicit first-wave defaults and mark deferred features |
| `CONFIG_SQL_SURFACE.md` | add `storage.buffer.%` visibility and mutation requirements |
| `TEST_CONTRACT.md` | require validation for new buffer-policy keys |

### Section 08
| File | Reason |
| --- | --- |
| `CHECKPOINT_AND_RECOVERY_STATE_MACHINE.md` | capture dirty-generation and writeback-debt interaction cleanly |
| `ALPHA_DURABILITY_MODES_AND_FLUSH_ORDERING.md` | define commit fences against the new dirty-state and queue model |
| `STARTUP_RECOVERY.md` | require queue rebuild and keep warmup hints advisory only |
| `TEST_CONTRACT.md` | add recovery and checkpoint cases for queue rebuild and debt correctness |

### Section 12
| File | Reason |
| --- | --- |
| `TEMP_TABLES_NORMATIVE_IMPLEMENTATION.md` | bind temp pages to the `temporary_work` domain |
| `TEST_CONTRACT.md` | require temp-domain containment under pressure |

### Section 20
| File | Reason |
| --- | --- |
| `BUFFER_CACHE_OBSERVABILITY.md` | add domain, ghost, admission, debt, and contention surfaces |
| `STORAGE_METRICS.md` | add per-object, per-filespace, and runtime buffer policy metrics |
| `RECOVERY_AND_CHECKPOINT_OBSERVABILITY.md` | add checkpoint debt and queue-health observability |
| `TEST_CONTRACT.md` | require stable schema and names for new views and metrics |

### Section 31
| File | Reason |
| --- | --- |
| `BUFFER_CACHE_AND_MGA_STORAGE_GATES.md` | add policy-domain, ghost-history, fairness, and contention gates |
| `RELIABILITY_CHAOS_AND_RECOVERY_GATES.md` | add crash and failpoint cases around frame ownership and queue rebuild |
| `TEST_CONTRACT.md` | make the new storage-policy suites mandatory for gate approval |

## Gate Expansion Matrix
| Gate area | New required scenario |
| --- | --- |
| Scan resistance | scans larger than memory do not evict protected transactional and metadata residency |
| Domain isolation | `critical_system` and `version_undo` reservations survive bulk scan and temp-work pressure |
| Admission policy | one-touch scan pages remain probationary or ring-only and do not collapse the protected set |
| Ghost history | recently evicted hot pages are detected through ghost hits and drive visible policy telemetry |
| Writeback debt | checkpoint drains the captured generation set without chasing later dirties |
| Cleaner fairness | background cleaners reduce dirty debt before foreground workers incur large flush stalls |
| Prefetch fairness | speculative pages are dropped first under pressure and useless prefetch is observable |
| Temp isolation | temp and work pages stay outside durable fences and outside protected durable reservations |
| Local concurrency | unrelated misses, evictions, and flushes do not serialize behind one global path |
| Crash recovery | crash during load, flush, eviction, or queue rebuild converges to one legal state |
| MGA correctness | version-chain and transaction-state pages never become unreachable due to cache policy |

## Authority and Index Maintenance Required
When the section-03 files are added:
1. update `03_Disk_Allocator_and_Free_Space/README.md`
2. update `03_Disk_Allocator_and_Free_Space/SPEC_OUTLINE.md`
3. update `AUTHORITATIVE_SPEC_INVENTORY.md`
4. run the section README sync script

## Expected Outcome
After the spec update pass, the authoritative tree should let an
implementation agent begin coding without needing to re-decide:
- what the domains are
- when pages are admitted or promoted
- how dirty debt is tracked
- how frame ownership is transferred
- which telemetry is mandatory
- which gates prove correctness
