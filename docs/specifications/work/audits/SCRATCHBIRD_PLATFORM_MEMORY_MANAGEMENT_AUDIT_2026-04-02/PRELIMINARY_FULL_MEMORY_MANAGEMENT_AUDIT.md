# ScratchBird Platform Memory Management Audit

Status: preliminary_local_audit_refined_with_web_research

## Purpose

This audit determines the best memory-management direction for ScratchBird as both:

- a database environment with MGA lineage, buffer management, resident indexes, caches, and background services
- a bytecode and native-code execution platform with SBLR, optional LLVM JIT, artifact reuse, and runtime metrics

The goal is not donor parity. The goal is to determine which memory techniques fit ScratchBird's platform shape, which ones need adaptation, and which ones should be rejected.

## Hard ScratchBird constraints

The following are treated as non-negotiable:

1. MGA row-version lineage remains the truth.
2. Memory policy may not weaken correctness, rollback, reclaim legality, or schema publication.
3. ScratchBird is a platform, not just a storage engine. The model must govern parser, executor, bytecode VM, JIT, caches, resident indexes, temp structures, and background tasks.
4. OOM behavior must fail by throttling, shrinking, spilling, or canceling work before the process is left to the operating system OOM killer.
5. The memory model must support sandboxed schema trees and database UUID boundaries, not just process-global and database-global accounting.
6. JIT and native-code memory must be isolated and reclaimable; it cannot be treated like anonymous process heap.

## ScratchBird baseline already in place

ScratchBird already has more memory structure than a generic engine:

- Section 33 defines explicit memory domains, allocator boundaries, and process-local arena ownership.
- The current canonical domains already distinguish buffer pool, caches, executor runtime, temp and spill, resident indexes, JIT artifacts, and accelerator memory.
- The current pressure order already prefers shrinking transient execution memory and optional caches before page residency.
- The implementation already has domain-oriented buffer-pool policy, separate permission cache, statement cache, translation cache, result cache, and JIT artifact storage.

This means the best path is an extension and hardening of the existing model, not a ground-up replacement.

## What the local donor review established

The local reference repos were enough to establish the first-order shape:

1. PostgreSQL is the strongest donor for memory-context trees, resettable lifetimes, and live hierarchy introspection.
2. Firebird is the strongest donor for strict owner-bound pools and parent-child pool accounting.
3. MySQL gives the best short-lifetime heap model for parser and planner churn, plus a proven scan-resistant page cache.
4. DuckDB is the cleanest donor for a shared controller over persistent and temporary memory.
5. ClickHouse and OpenSearch are the clearest donors for hard and soft limit trees, breaker-style admission, and kill-or-wait policy.
6. Neo4j is the best donor for separating heap, page cache, direct buffers, and code cache.
7. Redis and allocator work show that fragmentation control and compact encodings matter, but mostly outside the core MGA row store.
8. Milvus shows that vector and ANN memory should be treated as a first-class budget class, not hidden under generic caches.

## What the web research changed

The web pass strengthened and refined the local result:

1. SQL Server memory grant feedback made the case for persisted, percentile-based operator grants rather than static estimates.
2. ClickHouse memory overcommit and OpenSearch circuit breakers made the case for explicit soft and hard pressure transitions, not just cache eviction.
3. Oracle's unified memory documentation reinforced the need for one process-wide control plane that can rebalance memory among major classes while keeping the classes visible.
4. DuckDB's OOM guidance and external aggregation paper confirmed that temporary intermediates should degrade gracefully, not fall off a cliff.
5. LeanStore and Umbra reinforced the value of low-overhead buffering that still handles larger-than-memory data sets.
6. Virtual-memory-assisted buffer management is promising, but it is a second-step optimization, not the first change ScratchBird should make.
7. Temeraire and mimalloc show that allocator choice can materially change TLB behavior, fragmentation, and fast-path contention.
8. ORCv2 clarified that JIT code memory needs explicit resource ownership and code-removal semantics.

## Audit method

Each optimization below follows the same sequence:

1. local donor-only result
2. web-research refinement
3. ScratchBird fit
4. step-by-step implementation shape
5. alternatives with pros and cons
6. recommendation

## MM-01: Hierarchical memory governance and breaker tree

`Current ScratchBird state`: good vocabulary, incomplete enforcement. Section 33 already defines domains, but the model still needs a harder, actor-aware budget tree.

`Local preliminary result`: PostgreSQL, Firebird, ClickHouse, and OpenSearch all converge on one lesson: ownership and limits must be hierarchical, not flat.

`Web refinement`: SQL Server memory brokers and OpenSearch parent-child breakers strengthen the case for a process-wide control plane with subordinate limits.

`Fit with ScratchBird`: very high. ScratchBird already has natural hierarchy points: process, database UUID, schema root, connection, transaction, statement, operator, background task, and JIT unit.

`Recommended implementation`:

1. Define one process-wide `MemoryRoot` with physical target, emergency reserve, and reclaim policy.
2. Keep the canonical Section 33 domains as first-class children under `MemoryRoot`.
3. Under each domain, charge memory through `database_uuid`, `schema_root_uuid`, `connection_uuid`, `transaction_uuid`, `statement_uuid`, and `task_uuid` where applicable.
4. Track `reserved_bytes`, `committed_bytes`, `peak_bytes`, `spillable_bytes`, `reclaimable_bytes`, and `nonspillable_bytes` at every node.
5. Apply soft limits that trigger shrink, spill, or wait, and hard limits that deny new reservation except for emergency rollback and error reporting.
6. Expose the full tree through metrics and a live inspection view.

`Alternative A`: process-global budget only

Pros:
- simple to implement
- low metadata overhead

Cons:
- cannot isolate tenants, statements, or runaway background jobs
- makes debugging hard

`Alternative B`: thread-local or task-local limits only

Pros:
- cheap fast-path checks
- easy to wire into worker pools

Cons:
- cannot express shared caches or page-residency policy
- poor fit for multi-tenant governance

`Recommendation`: use a full hierarchy. ScratchBird needs both fast local checks and correct parent aggregation.

## MM-02: Typed allocator suite and lifetime-bound contexts

`Current ScratchBird state`: partial. The specs define arena and context ideas, but the implementation still relies on more generic allocation in several runtime areas.

`Local preliminary result`: PostgreSQL contexts, Firebird pools, MySQL `MEM_ROOT`, and ClickHouse arenas all say the same thing: lifetime and object shape should choose the allocator.

`Web refinement`: mimalloc strengthens the case for page-local, low-contention fast paths for small objects; ORCv2 strengthens the need for a distinct code-memory lifecycle.

`Fit with ScratchBird`: very high. The parser, planner, translator, executor, JIT compiler, result builders, and background maintenance jobs all have distinct lifetimes.

`Recommended implementation`:

1. Introduce `SbArena` for parse, bind, rewrite, and short executor scratch with bulk reset.
2. Introduce `SbGenerationArena` for medium-lived structures retired by generation or schema epoch.
3. Introduce `SbSlab` for fixed-size descriptors, queue nodes, cache entries, and tracker metadata.
4. Introduce `SbPageBackedArena` for spillable operator state and other page-oriented temporary structures.
5. Introduce `SbCodeHeap` for executable code pages and a paired non-executable metadata heap for JIT objects.
6. Keep a small `SbObjectHeap` fallback for cases that do not fit the typed model.

`Alternative A`: one general-purpose allocator

Pros:
- simplest coding model
- lowest implementation complexity

Cons:
- highest fragmentation risk
- poorest lifetime visibility
- makes leak analysis and limit enforcement weaker

`Alternative B`: every subsystem invents its own allocator

Pros:
- can be highly specialized
- each subsystem can optimize aggressively

Cons:
- hard to govern consistently
- easy to lose global accounting

`Recommendation`: standardize on a small allocator suite owned by ScratchBird. The backend allocator is a detail, not the architecture.

## MM-03: Unified persistent-page and temporary-page control

`Current ScratchBird state`: partial. Persistent page management and cache governance exist, but spillable operator memory is not yet governed by the same controller.

`Local preliminary result`: DuckDB is the strongest donor here. Neo4j and SQL Server also reinforce separating page memory from object heaps while keeping both under one top-level policy.

`Web refinement`: LeanStore, Umbra, and DuckDB external aggregation all reinforce the same point: larger-than-memory performance depends on one coherent controller, not independent pools that fight each other.

`Fit with ScratchBird`: high. ScratchBird is neither a pure in-memory engine nor a pure disk-oriented engine. The right model must let intermediates spill and pages rebalance without semantic drift.

`Recommended implementation`:

1. Treat persistent data pages, resident-index pages, and spillable operator pages as residency-managed classes under one controller.
2. Give each class its own quota, temperature, and reclaim policy, but let all classes compete inside one process-wide budget.
3. Require spillable operator state to allocate through `SbPageBackedArena`.
4. Make operator pages evictable and restorable without per-row serialization when possible.
5. Track residency pressure, spill bytes, restore bytes, and grace-period debt.
6. Refuse duplicate hidden temp allocators outside the central controller.

`Alternative A`: separate temp-memory pool

Pros:
- easier to reason about initially
- clear operator accounting

Cons:
- page cache and temp pool compete blindly for RAM
- performance cliffs are more likely

`Alternative B`: virtual-memory-assisted mapping first

Pros:
- removes translation overhead
- elegant page-ID to pointer mapping

Cons:
- OS-specific complexity
- not the first pressure point ScratchBird needs to solve

`Recommendation`: unify the controller now. Consider virtual-memory-assisted mapping only after the budget tree and temp-page model are stable.

## MM-04: Feedback-based statement and operator grants

`Current ScratchBird state`: mostly estimate-driven and policy-driven, without persistent feedback loops for grant right-sizing.

`Local preliminary result`: MongoDB and ClickHouse show the value of per-operation tracking; local donor behavior also shows the cost of static limits when workloads vary.

`Web refinement`: SQL Server memory grant feedback is the strongest proof source for persisted and percentile-based grant correction.

`Fit with ScratchBird`: high. ScratchBird has stable normalization points in translated bytecode plans and statement caches.

`Recommended implementation`:

1. Estimate initial grants from plan shape, cardinality estimates, data-type widths, and operator class.
2. Reserve against the statement node before execution.
3. Track actual high-water marks, spill bytes, and unspillable bytes per operator.
4. Persist feedback by normalized plan signature, engine family, schema epoch compatibility, and workload class.
5. Use percentile-based adjustment, not last-run-only adjustment.
6. Disable or damp feedback automatically for unstable workloads.

`Alternative A`: static grants only

Pros:
- predictable
- easy to implement

Cons:
- wastes memory on overestimates
- causes spill cliffs on underestimates

`Alternative B`: unbounded adaptive overcommit

Pros:
- fewer early denials
- easy for bursty workloads

Cons:
- can destabilize the process
- hurts concurrency if not tightly bounded

`Recommendation`: persisted percentile feedback with caps is the best fit.

## MM-05: Pressure protocol and OOM-safe failure behavior

`Current ScratchBird state`: Section 33 orders shrink preference correctly, but the full soft-pressure to hard-pressure protocol still needs to be codified.

`Local preliminary result`: ClickHouse, OpenSearch, Cassandra, and MongoDB all show that explicit transition states beat ad hoc failure.

`Web refinement`: ClickHouse overcommit and DuckDB OOM guidance both point toward staged degradation, not binary success or failure.

`Fit with ScratchBird`: very high. ScratchBird must never put MGA correctness at risk because the process ran out of memory unexpectedly.

`Recommended implementation`:

1. Under soft pressure, shrink result cache, translation cache, statement cache, and optional resident-index warming.
2. If pressure persists, suspend new compile jobs and background builders, and start bounded spill on eligible operators.
3. If a statement exceeds its hard limit and is spillable, spill or repartition it.
4. If a statement exceeds its hard limit and is not spillable, cancel the statement, not the process.
5. Maintain an emergency reserve for rollback, error serialization, telemetry flush, and checkpoint-safe cleanup.
6. Refuse to rely on the operating system OOM killer as normal control flow.

`Alternative A`: deny new allocations immediately

Pros:
- simplest semantics
- low implementation effort

Cons:
- poor throughput under bursty mixed workloads
- leaves too much performance on the table

`Alternative B`: kill work only at process level

Pros:
- easy to wire

Cons:
- unacceptable reliability profile
- can leave little diagnostic context

`Recommendation`: staged pressure handling is mandatory.

## MM-06: General allocator backend and fragmentation strategy

`Current ScratchBird state`: allocator backend is not yet the main leverage point because subsystem-specific allocation policy is still the bigger gap.

`Local preliminary result`: Redis, Firebird, and ClickHouse show that allocator choice matters, but only after ownership and charging are correct.

`Web refinement`: mimalloc and Temeraire make the tradeoff clear.

`Fit with ScratchBird`: medium to high. ScratchBird needs a pluggable backend, but it should sit underneath ScratchBird-owned arenas, not replace them.

`Recommended implementation`:

1. Define a narrow backend interface for `malloc`, `free`, aligned alloc, usable-size query, statistics, and background maintenance hooks.
2. Route ScratchBird general-purpose fallback heaps through that interface only.
3. Keep arenas, slabs, page-backed allocators, and code heaps under ScratchBird control regardless of backend.
4. Benchmark at least `mimalloc` and `tcmalloc` on real ScratchBird mixed workloads.
5. Keep `jemalloc` available as a profiling and observability fallback if its arena introspection proves valuable.
6. Add fragmentation metrics and periodic backend maintenance hooks where supported.

`Alternative A`: `mimalloc` as default general backend

Pros:
- excellent small-object locality
- page-local sharded free lists
- good fit for parser, compiler, and runtime churn

Cons:
- less hugepage-centric than Temeraire
- still needs ScratchBird-side accounting

`Alternative B`: `tcmalloc` with Temeraire path

Pros:
- strongest hugepage story
- good large-allocation and TLB behavior
- mature CPU-cache design

Cons:
- more tuned toward fleet-scale service deployments
- less obviously ideal for short-lived compiler churn

`Alternative C`: `jemalloc`

Pros:
- strong profiling and arena tooling
- mature and widely deployed

Cons:
- not obviously the best fast path for ScratchBird's mixed workload
- background purge behavior may need extra tuning

`Recommendation`: do not commit architecture to the allocator backend. Implement ScratchBird-owned typed allocators first, then benchmark `mimalloc` and `tcmalloc`. If a default must be chosen before the benchmark gate exists, start with `mimalloc`.

## MM-07: NUMA placement and hugepage policy

`Current ScratchBird state`: partial. The process already detects host and cgroup memory, but placement and hugepage policy are not yet a first-class model.

`Local preliminary result`: Neo4j and SQL Server both show that page residency and execution memory need topology awareness.

`Web refinement`: Temeraire makes the performance value of hugepage coverage explicit.

`Fit with ScratchBird`: high, especially for buffer pool, resident indexes, vector segments, and large temp operators.

`Recommended implementation`:

1. Detect NUMA topology at process start and record node-local capacity.
2. Shard large stable arenas and the buffer pool by NUMA node.
3. Prefer local-node allocation for connection, statement, and operator scratch.
4. Allow bounded remote stealing only under pressure.
5. Use hugepages for the buffer pool and other large stable arenas where the platform supports them.
6. Do not force hugepages for tiny or extremely bursty transient arenas.

`Alternative A`: uniform interleave

Pros:
- easy to implement
- avoids local exhaustion imbalance

Cons:
- pays cross-node traffic constantly
- weaker cache and TLB locality

`Alternative B`: strict local-only placement

Pros:
- strongest locality

Cons:
- can fail too early on skewed workloads

`Recommendation`: local-first with bounded rebalance is the best compromise.

## MM-08: Cache and residency segregation with adaptive shrink

`Current ScratchBird state`: good base. Section 33 already orders pressure correctly, and the implementation already has separate caches.

`Local preliminary result`: Neo4j and MySQL reinforce that page cache should not be collapsed into generic heap policy. OpenSearch reinforces that request memory needs different controls than cached memory.

`Web refinement`: Redis and MongoDB reinforce the importance of selectively shrinking optional memory first.

`Fit with ScratchBird`: very high.

`Recommended implementation`:

1. Keep separate quotas for buffer pool, resident indexes, statement cache, translation cache, permission cache, result cache, and JIT artifacts.
2. Charge all of them into the same hierarchy even though their shrink policies differ.
3. Preserve the existing pressure order, but make the order executable rather than descriptive.
4. Add per-cache shrink aggressiveness and floor settings.
5. Let caches expose their own reclaim cost and latency sensitivity to the central scheduler.
6. Prevent a single generic LRU from treating all cached memory as equivalent.

`Alternative A`: one global LRU

Pros:
- simple
- easy to explain

Cons:
- destroys subsystem intent
- can evict critical page residency before optional caches

`Recommendation`: keep segregated caches with unified charging.

## MM-09: JIT and native-code memory isolation

`Current ScratchBird state`: partial. JIT artifacts already exist, but compile memory, object-link memory, executable code pages, and code-cache budgets still need a stricter lifecycle.

`Local preliminary result`: Neo4j's explicit code-cache visibility and separate native memory reinforce the need for isolation.

`Web refinement`: ORCv2 is decisive here. Resource trackers and JITDylib boundaries are exactly the model ScratchBird needs.

`Fit with ScratchBird`: very high. ScratchBird is a bytecode and native-code platform, not only a database runtime.

`Recommended implementation`:

1. Separate JIT compile arenas from executable code heaps and artifact metadata.
2. Charge compile memory to the statement or job that requested compilation.
3. Publish executable code only after materialization succeeds.
4. Tag every compiled unit by normalized bytecode signature, engine compatibility, CPU features, and schema epoch compatibility.
5. Retire compiled units by resource tracker or generation, not by ad hoc pointer ownership.
6. Keep the VM interpreter as the deterministic fallback whenever the code budget or platform policy says no.

`Alternative A`: interpreter only

Pros:
- simplest memory model
- lowest native-code risk

Cons:
- leaves performance on the table
- weakens the platform story

`Alternative B`: untracked native-code allocations

Pros:
- easy to prototype

Cons:
- unacceptable for a long-running database platform
- code-cache leaks are hard to diagnose

`Recommendation`: tracked JIT memory is mandatory if JIT remains a platform feature.

## MM-10: Background memory-debt scheduler

`Current ScratchBird state`: partial. There are existing maintenance concepts, but no single memory-debt control plane for fragmentation, reclaim, shrink, spill-merge, and JIT retirement.

`Local preliminary result`: Redis active defrag, Cassandra cleaner behavior, and ClickHouse accounting all point toward explicit background debt management.

`Web refinement`: Temeraire and DuckDB both reinforce the value of paying maintenance incrementally instead of at cliff points.

`Fit with ScratchBird`: high. ScratchBird already has background maintenance culture in indexing and storage; memory should use the same discipline.

`Recommended implementation`:

1. Create a `MemoryDebtLedger` keyed by domain and work class.
2. Track at least fragmentation debt, reclaim debt, temp-spill merge debt, cache-shrink debt, and JIT retirement debt.
3. Schedule debt work by benefit-per-byte and latency sensitivity.
4. Permit small foreground assists only when the work is obviously bounded and profitable.
5. Record debt age, bytes recovered, and failure reason.
6. Keep the scheduler subordinate to foreground reliability.

`Alternative A`: synchronous cleanup only

Pros:
- easiest correctness story

Cons:
- worst latency spikes
- high debt cliffs

`Alternative B`: opportunistic-only cleanup

Pros:
- lightweight background machinery

Cons:
- unreliable completion
- hot-spot bias

`Recommendation`: use an explicit background debt ledger with bounded foreground assists.

## MM-11: Observability, inspection, and fault injection

`Current ScratchBird state`: partial. Metrics exist, but full live memory-context inspection and allocation-failure drills are still incomplete.

`Local preliminary result`: PostgreSQL, ClickHouse, and MongoDB are the strongest donors here.

`Web refinement`: SQL Server feedback and OpenSearch breakers reinforce that you cannot tune what you cannot see.

`Fit with ScratchBird`: very high.

`Recommended implementation`:

1. Expose a live `sb_memory_contexts` or equivalent inspection surface modeled after PostgreSQL's hierarchy view.
2. Emit per-domain and per-node metrics for reserved, committed, peak, reclaimable, spilled, and canceled bytes.
3. Sample allocation sizes and allocation latency for hotspot analysis.
4. Record why grants changed, why a statement spilled, and why a statement was canceled.
5. Add fault-injection hooks for allocation failure, hard-limit clamps, and emergency-reserve exhaustion tests.
6. Capture memory-tree dumps in crash-safe diagnostics when pressure events occur.

`Alternative A`: process-level RSS only

Pros:
- trivial to expose

Cons:
- nearly useless for tuning a platform runtime

`Recommendation`: full introspection is not optional.

## MM-12: Schema-tree quotas and platform multi-tenancy

`Current ScratchBird state`: conceptually aligned because ScratchBird already has a recursive schema tree and sandboxing model, but not yet fully memory-governed at that granularity.

`Local preliminary result`: no donor matches this shape exactly, which is why ScratchBird should not copy a donor unmodified.

`Web refinement`: Oracle, OpenSearch, and SQL Server show the value of layered governance, but ScratchBird's schema-root sandboxing is its own differentiator.

`Fit with ScratchBird`: very high.

`Recommended implementation`:

1. Make schema-root quota objects first-class in the memory hierarchy.
2. Inherit defaults from process and database policy, but allow tighter per-schema-root caps.
3. Charge parser sessions, temp operators, caches, and JIT activity into the active schema-root branch.
4. Permit nested schema trees to inherit or override quotas explicitly.
5. Emit tenant-facing metrics at the schema-root level.
6. Make quota breaches cancel or throttle work in the offending branch, not in unrelated branches.

`Alternative A`: database-only quotas

Pros:
- simpler governance

Cons:
- weak match for ScratchBird sandboxing
- poor fit for platform hosting

`Recommendation`: schema-root quotas should be first-class from the start.

## Research options to keep visible but not ship first

The following techniques are promising, but they are not the first implementation step:

- virtual-memory-assisted page translation like `vmcache` and `exmap`
- persistent-memory-specific mutable structures such as Cassandra's planned persistent-memory memtable path
- more radical message-buffered tree or VM-assisted page indirection models for all indexes

These are worth preserving as future-track items, but ScratchBird should first harden governance, typed allocation, spill control, and JIT isolation.

## Overall recommendation

The best ScratchBird memory model is a hybrid:

- PostgreSQL-style lifetime contexts
- Firebird-style ownership discipline
- DuckDB and Umbra style larger-than-memory grace
- ClickHouse and OpenSearch style hard and soft limits
- SQL Server style feedback-based grants
- Neo4j style separation of heap, page cache, native memory, and code cache
- mimalloc or Temeraire class allocator thinking under ScratchBird-owned arenas
- ORCv2-style resource tracking for generated code

The important point is that ScratchBird should own the policy and allocator classes itself. Vendor allocators and donor patterns are inputs, not the architecture.

If ScratchBird implements only three things first, they should be:

1. the hierarchical budget tree with schema-root charging
2. the typed allocator suite plus unified temp and persistent page control
3. the staged pressure protocol with statement feedback grants and JIT isolation

Those three unlock almost everything else and improve both database reliability and platform-runtime quality.
