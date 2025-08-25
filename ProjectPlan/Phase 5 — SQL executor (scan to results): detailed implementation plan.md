# Phase 5 — SQL Executor (Scan to Results) ✅ **COMPLETED**

## Status: **FULLY IMPLEMENTED**

#### Scope and goals

- Implement a functional SQL executor that evaluates parsed SELECT queries over ScratchBird storage, producing correct results with transactional visibility.
- Cover expression evaluation, table and index scans, projections, filters, ORDER BY, LIMIT/OFFSET, aggregations (hash/sort-based), a first join operator (nested loop), and a subset of window functions.
- Integrate with existing snapshot visibility (RC, RR) and B-Tree V1 for point/range index scans.
- Provide work memory and spill-to-temp for sort/aggregation with instrumentation.
- Extend isql to run SELECT and EXPLAIN [ANALYZE].
- Exit: TPC-H subset queries return correct results; EXPLAIN costs roughly align with actual runtime metrics (within documented tolerances).

#### Non-goals (deferred to later phases)

- Advanced optimizer join order/costing (Phase 6).
- Additional index families beyond B-Tree (Phase 9).
- FDW/DBLINK execution (Phase 10).
- Full window function coverage and frame variations (Phase 17/18 as needed).
- Parallel execution.

---

### Architecture

- Execution model: Volcano-style iterator interface initially (`open()`, `next()`, `close()`), row-at-a-time for simplicity and correctness. Future upgrade path to vectorized batches if needed.
- Plan nodes: Logical nodes already formed by basic planner; introduce physical executor nodes mirroring those logical operators.
- Expression engine: Interpreted evaluator operating on a typed `Value`/`Datum` abstraction with null and collation semantics.
- Memory management: Per-operator memory contexts with a `work_mem` soft limit and spill paths using temp files via FileManager.
- Instrumentation: Each node records counters (input/output rows, filtered rows, spills, memory high-water, timing). EXPLAIN ANALYZE will emit these.

### Key components and APIs

1) Value system and tuple representation
- Reuse existing `Value` from heap/executor contexts; ensure it supports: NULL, booleans, integers (all widths), decimals/numerics, floats, text (charset/collation id), binary, date/time/timestamp, JSON placeholder, UUID.
- Define `Datum` as a lightweight view for evaluation; executor materializes `Tuple` as `std::vector<Value>` with column metadata (type, collation, nullability).
- Implement collation-aware comparison hooks (via ICU or placeholder deterministic collation mapping) with NULLS FIRST/LAST support.

2) Expression evaluator
- AST-to-expr lowering: Map parsed expression nodes into an internal `Expr` tree with typed opcodes.
- Supported operations initially: arithmetic (+,-,*,/), comparison (=, !=, <, <=, >, >=), boolean (AND/OR/NOT), IS [NOT] NULL, COALESCE, CASE WHEN, CAST, COLLATE, LIKE/ILIKE (basic), string concat, substring, length, date/time basic ops.
- Aggregate functions (for agg/window): COUNT(*), COUNT(expr), SUM, AVG, MIN, MAX. Type promotion rules and NULL handling defined.
- Deterministic evaluation order; short-circuiting for boolean ops.
- Collation-resolved comparisons for text, with deterministic vs nondeterministic note (deterministic only for now).

3) Executor node interfaces
- Base: `ExecutorNode` with lifecycle `open(ctx)`, `bool next(Tuple& out)`, `close()`. Context carries snapshot, memory context, temp manager, and instrumentation sink.
- Nodes:
  - `SeqScanNode` (table scan with predicate pushdown and projection list)
  - `IndexScanNode` (point and range scans; forward/backward; optional recheck for residual predicates)
  - `FilterNode` (applies boolean expr)
  - `ProjectNode` (computes output columns/expressions)
  - `SortNode` (ORDER BY with collation and nulls mode; in-memory quicksort; spill to external multiway mergesort)
  - `LimitNode` (LIMIT/OFFSET)
  - `NestedLoopJoinNode` (inner, left outer to start) with join quals
  - `HashAggNode` (GROUP BY + aggregates; spill-aware; fallback to sort-based agg on memory pressure)
  - `WindowNode` (subset: ROW_NUMBER, RANK, DENSE_RANK, SUM/AVG over PARTITION BY + ORDER BY with default frame)
  - `MaterializeNode` (optional; buffers child for re-scan in NLJ inner on small inputs)

4) Work memory and spill
- `work_mem_bytes` default from engine config; overridable per-session later.
- Sort: In-memory until `work_mem` exceeded, then runs `k`-way external mergesort with temp runs.
- HashAgg: Monitor hash table load/bytes; on pressure, partition (grace hash) to temp runs and aggregate per partition. Fallback to sort-group-aggregate if pressure persists.
- Temp files: Use FileManager with temp directory (configurable); O_TMPFILE when available; ensure cleanup on query end.

5) Snapshot visibility integration
- Each scan obtains the current `SnapshotRC`/`SnapshotRR` from `TransactionManager` via the executor context.
- `SeqScanNode` and `IndexScanNode` apply `is_visible_rc/rr` to filter out invisible versions.
- For RR, ensure stability across re-scans within the same query execution.

6) Index scan specifics (B-Tree V1)
- Point lookups: equality predicates on leading key(s).
- Range scans: `<, <=, >, >=, BETWEEN`; support start/stop keys; inclusive/exclusive bounds.
- Order support: forward/backward scans to satisfy ORDER BY on index keys; allow index-only scan if projection is covered (`INCLUDE` or key-only) when available from catalog metadata.
- Residual predicate evaluation for non-key filters.

7) Sorting semantics
- Stable vs unstable: implement stable ordering to honor SQL semantics when ORDER BY ties require deterministic output with DISTINCT/aggregate interactions.
- Collations: respect column or explicit COLLATE; NULLS FIRST/LAST; tie-break by physical tiebreaker (OID) only when required and documented.

8) Aggregation semantics
- GROUP BY hashing with deterministic grouping keys (collation-aware for text); numeric and datetime keys supported.
- Aggregates: implement correct NULL handling (SUM/AVG ignore NULLs; COUNT(*) counts rows; COUNT(expr) ignores NULLs).
- AVG uses SUM/COUNT with exact decimal promotion where possible; document overflow behavior; prefer 128-bit intermediate for exact numerics.

9) Window functions (subset)
- Partitioning on discrete keys; ordering on single column initially (extendable).
- Functions: ROW_NUMBER, RANK, DENSE_RANK; SUM/AVG with default frame `RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW`.
- Implement via segment materialization per partition with running accumulators; spill if partition exceeds `work_mem`.

10) Error handling and diagnostics
- Standardized executor error codes: division by zero, invalid cast, overflow, collation mismatch, sort spill failure, temp file I/O.
- Diagnostic context: enrich errors with node type, operator description, and current row count processed.

11) Instrumentation and EXPLAIN ANALYZE
- Per-node: input_rows, output_rows, filtered_rows, memory_bytes_peak, temp_bytes_written, spills_count, cpu_time_ms, wall_time_ms, sort_runs, hash_partitions.
- EXPLAIN prints logical/physical plan; EXPLAIN ANALYZE executes and prints actuals; align/compare to estimated cost when available from Phase 4/early planner.
- Counters exposed to `MON$` compatibility views later; for now, only query-time output.

---

### Integration points

- Parser/AST: SELECT AST already available; extend executor lowering in `execute_ast` to build plan trees and run execution pipeline.
- Catalog: Use `CatalogManager` to resolve relation/column OIDs, index presence and key order, collation metadata.
- Storage: `HeapRelation` scan API, `HeapScan`; B-Tree V1 range API for index scan; transaction snapshot from `TransactionManager`.
- Config: Read `work_mem`, temp directory from `HeaderManager`/engine config clumplets.
- isql: Add SELECT execution and result printing; add `EXPLAIN` and `EXPLAIN ANALYZE` formatting.

---

### Detailed implementation steps

1) Foundations
- Define executor context struct: holds db path/handle, snapshot, memory manager, temp manager, instrumentation sink, collation resolver.
- Implement a simple memory context API with accounting; integrate with allocator for temp buffers.
- Implement temp file manager with lifecycle hooks.

2) Expression subsystem
- Create `expr.h/.cpp`: internal opcodes, nodes, type inference, and interpreter.
- Lower AST expressions to `Expr` with resolved column bindings and types.
- Implement evaluation functions with null propagation and collation-aware comparisons.
- Unit tests: arithmetic, comparison, boolean, CASE, COALESCE, CAST, text comparisons with collations, NULL behavior.

3) SeqScanNode
- Implement visibility-aware scan using `HeapScan` with predicate evaluation and projection pushdown.
- Add instrumentation counters.
- Tests: full table scan with filters; RC vs RR visibility; projection correctness.

4) IndexScanNode
- Add range builder from simple predicates; support point and range.
- Integrate with B-Tree V1 traversal for ordered iteration; optional backward scan.
- Apply residual predicates and projection. Use index-only fast path when covered.
- Tests: point/range queries; ordered scans satisfying ORDER BY; visibility; index-only path.

5) FilterNode and ProjectNode
- Implement generic nodes to allow composability even when scans cannot push everything down.
- Tests: stacked filters, computed projections.

6) SortNode
- Implement in-memory sort using stable sort with custom comparator honoring collations and NULLS FIRST/LAST.
- Implement external mergesort with run generation and k-way merge on spill.
- Tests: ordering with various types/collations; large dataset forcing spill; instrumentation of runs and temps.

7) LimitNode
- Implement straightforward LIMIT/OFFSET over child.
- Tests: boundaries, zero/large offsets; interaction with ORDER BY.

8) Aggregation
- Implement `HashAggNode` with grouping key hashing (collation-aware) and aggregate state structs.
- Spill-aware partitioning; fallback to sort-based aggregation via `SortNode` + streaming group aggregator.
- Aggregates: COUNT(*), COUNT(expr), SUM, AVG, MIN, MAX with correct types.
- Tests: group by correctness; NULL handling; spill scenarios; numeric promotion.

9) NestedLoopJoinNode
- Implement inner join first; add left outer afterward.
- Execution: for each outer row, re-scan inner child applying join quals; optional `MaterializeNode` to buffer small inner for reuse.
- Tests: basic joins; selective predicates; left-outer null extension; correctness under RR.

10) WindowNode (subset)
- Implement partition buffering and per-partition computation for ROW_NUMBER, RANK, DENSE_RANK; SUM/AVG with default frame.
- Tests: partitions and ordering; ties for RANK/DENSE_RANK; spill for large partitions.

11) EXPLAIN and EXPLAIN ANALYZE
- Extend existing EXPLAIN to print physical nodes with estimated costs (reuse basic planner costs).
- Implement ANALYZE execution wrapper to collect and print per-node actual metrics and timings.
- Tests: snapshot of output format; verify metrics monotonicity and presence.

12) isql integration
- Add ability to run SELECT queries and print tabular results (paged or all).
- Add `EXPLAIN` and `EXPLAIN ANALYZE` commands mapped to executor calls.
- Tests: golden-output samples for small queries; timing disabled/enabled behavior.

13) TPC-H subset validation
- Prepare small TPC-H dataset (scale factor 0.001 or synthetic) and create matching tables.
- Implement queries: Q1 (aggregation/order), Q3 (joins/filter/order), Q6 (filter/agg), Q12 (joins/agg), minimal subset.
- Validate results vs reference (PostgreSQL or precalculated fixtures). Document any numeric/rounding differences.

14) Telemetry hooks
- Add Monitoring increments for rows produced/filtered, spills, temp bytes, sort comparisons, hash probe counts.
- Surface per-query summary in executor result when ANALYZE is requested.

15) Documentation/spec updates
- specs/MANIFEST.yaml: add executor specs.
- specs/docs: add `executor/overview.md`, `executor/expr.md`, `executor/operators.md`, `executor/spill.md` (follow AI-friendly template).

---

### Data structures and types (sketch)

- `ExecutorContext { TransactionSnapshot snapshot; MemoryContext mem; TempManager temp; CollationResolver coll; InstrumentationSink sink; }`
- `ExecutorNode { virtual void open(ExecutorContext&); virtual bool next(Tuple& out); virtual void close(); Instrumentation instr; }`
- `Expr` nodes: `Const`, `VarRef`, `FuncCall` (builtin ops), `Case`, `Coalesce`, `Cast`, `Collate`.
- `HashAgg` state: map from `GroupKey` → `AggState` (COUNT,SUM,MIN,MAX,AVG with accumulators).
- `Sort` run files: vector of temp run descriptors with schema and comparator.

---

### Configuration knobs

- `executor.work_mem_mb` (default 64MB) → bytes per node soft cap.
- `executor.temp_dir` (default engine dir/tmp subdir).
- `executor.sort_kway` (default 32), `executor.merge_buffers`.
- `executor.enable_index_only_scan` (default on).
- `executor.window.partition_spill_threshold_mb`.

---

### Testing strategy

- Unit tests per subsystem: expr, sort (with spill), hash agg (with spill), seq scan, index scan, NLJ, window subset.
- Integration tests: SELECT with filters/projections; ORDER BY + LIMIT; GROUP BY aggregates; simple joins; window subset.
- Visibility tests: RC and RR snapshots across concurrent writers (read-only queries must see correct versions).
- Collation tests: ORDER BY with explicit COLLATE and NULLS FIRST/LAST; comparisons in filters.
- TPC-H subset: deterministic fixtures; CI time-bounded.
- Fault injection: simulate temp I/O errors; force spill via low `work_mem`.

---

### Milestones and sequencing

M1 Foundations and expression evaluator (1 week)
- Context/memory/temp managers; expr interpreter; unit tests.

M2 Scans and basic SELECT (1 week)
- SeqScan + Filter + Project; RC/RR visibility; isql SELECT printing.

M3 Sorting and LIMIT/OFFSET (1 week)
- Sort node with spill; Limit node; ORDER BY correctness tests.

M4 Aggregations (1–1.5 weeks)
- HashAgg with spill + sort-based fallback; aggregates; GROUP BY tests.

M5 Index scans (1 week)
- IndexScan range/point; ordered scans; index-only path; planner hooks to choose index when obvious.

M6 Joins (1 week)
- Nested loop join (inner, left); Materialize helper; tests.

M7 Window subset and EXPLAIN ANALYZE (1–1.5 weeks)
- WindowNode; EXPLAIN ANALYZE with instrumentation; telemetry counters.

M8 TPC-H subset and polish (1 week)
- Load fixtures; validate results; docs/specs; CI gates; fix bugs.

---

### Risks and mitigations

- Spill complexity: Start with well-tested mergesort and grace hash patterns; aggressive unit tests with forced low `work_mem`.
- Collation correctness/perf: Begin with deterministic ICU collation handles cached per collation id; microbench and cache comparators.
- Visibility edge cases: Reuse proven MGA snapshot logic from Phase 3; add integration tests with concurrent writers.
- Window function memory: Partition-level spill when exceeding threshold; limit to simple frames initially.
- Performance regressions: Add microbench for sort/agg/index scan; compare to baselines from Phase J.

---

### Exit criteria

- All listed operators implemented with unit/integration tests passing under CI.
- isql supports SELECT and EXPLAIN/EXPLAIN ANALYZE with clear, stable output.
- TPC-H subset queries (Q1, Q3, Q6, Q12 minimal set) return correct results; runtime metrics recorded.
- Documented behavior for collations, NULL ordering, numeric promotions, and spill policies.
