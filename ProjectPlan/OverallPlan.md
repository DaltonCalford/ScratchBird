### Current state (high-level)

- Parser: Broad SQL coverage inc. modern features; admin/FDW/DBLINK DDL parsed; semicolonless mode and leading-comments doc mode implemented.

- Specs: System catalog (SDB$), domains, compat views (RDB$*, MON$*), bootstrap plan, config, admin/isql meta, index-build design, Phase J perf notes.

- Engine core: File manager, pager + buffer cache, checksums, header open/create/validate, config, WAL (logical) + recovery, B-Tree V1 (insert/delete/validate/stats), index build (offline/online) with WAL delta ingestion, basic planner/stats/EXPLAIN, monitoring, side tools (dbhdr, page_dump, fast_check, bench_btree).

- Gaps: Heap/table storage; transactions/MGA/TIP; catalog persistence (exec bootstrap); full SQL executor (scans/joins/agg/window/sort); optimizer (join order, cardinality); DML constraints/RI/triggers; PSQL runtime; index families (hash, GIN, bitmap, R-Tree); FDW+DBLINK execution; Y-Valve server + protocol/auth; backup/restore; replication shipper/replayer; tablespaces; admin surfaces execution; security/RLS; JSON/spatial; partitioning/materialized views; end-to-end tests, perf CI, packaging/docs.

### Phase plan to completion

**Phase 1 — Heap storage and row format**

- Implement heap pages: line pointers, null bitmap, varlena, off-page overflow/BLOb, row headers (txn ids, version chain).

- RowID format and mapping; table root structures.

- Free space tracking per page.

- Exit: Create/insert/select basic rows via internal harness; page validate tool.

**Phase 2 — Space management and allocation**

- PIP/TIP/space catalog; extents; free page/space maps; multi-segment growth; tablespace placement.

- Integrate Allocator with PIP; resilient on crash.

- Exit: Deterministic growth, reclaim on drop/truncate, allocator soak.

**Phase 3 — Transactions and MGA**

- 64-bit transaction ids; TIP pages; snapshot acquisition; versioned record visibility rules; write/write conflict detection; garbage collection/sweep.

- Statement/transaction/attachment id generators; deadlock detection.

- Exit: Correct isolation semantics (read committed, repeatable read), GC removes unreachable versions.

**Phase 4 — Catalog persistence and bootstrap**

- Materialize SDB$* tables and system domains; execute bootstrap SQL inside engine; fixed UUIDs seeded.

- RDB$*/MON$* compat views; metadata read/write via transactional DDL.

- Exit: CREATE/ALTER/DROP objects persist; catalog versioning and migrations.

**Phase 5 — SQL executor (scan to results)**

- ****Expression evaluator; table/Index scans (point/range), projections, filters; ORDER BY; LIMIT/OFFSET; hash/sort aggregations; window functions (subset).

- Executor operators: seq scan, index scan, nested loop join; sort; hash agg; work memory and spill.

- Exit: TPC-H subset queries produce correct results; EXPLAIN cost aligns with actuals.

**Phase 6 — Optimizer and statistics**

- ****Cardinality estimation using histograms/MCV/correlation; selectivity for predicates; join order DP (small N), greedy (large N); index/scan costing; parallelizable hooks (later).

- EXPLAIN ANALYZE with timings; plan cache; stats refresh via ANALYZE.

- Exit: Planner chooses indexes/joins sensibly on canonical workloads; regression suite stable.

**Phase 7 — Constraints, RI, triggers**

- CHECK/NOT NULL/UNIQUE/PK, DEFERRABLE/INITIALLY modes; FK with CASCADE/RESTRICT/SET NULL/NO ACTION; SET CONSTRAINTS.

- Row/statement-level triggers (BEFORE/AFTER); trigger firing order and transition tables.

- Exit: Constraints enforced with proper deferral; trigger suites pass.

**Phase 8 — PSQL runtime**

- EXECUTE BLOCK/PROCEDURE/FUNCTION bodies; variables, control flow, exceptions, cursors; SECURITY DEFINER/INVOKER.

- Deterministic/Nondeterministic flags honored for inlining/caching.

- Exit: PSQL tests (packages, exceptions) pass; performance acceptable.

**Phase 9 — Index families and advanced options**

- ****Hash index (dir/bucket); bitmap index (compressed bitmaps); GIN (posting lists, simple tokenization); R-Tree (rectangles).

- INCLUDE columns payload; partial index predicate enforcement in executor; validate/reindex support across families.

- Exit: CREATE/VALIDATE/REINDEX/SCAN for all families; planner picks appropriate indexes.

**Phase 10 — FDW/SPI and Database Links**

- Provider SPI; FOREIGN SERVER/USER MAPPING/FOREIGN TABLE, IMPORT FOREIGN SCHEMA; local adapters: Files, PostgreSQL (basic).

- DBLINK execution: table@link refs and cross-db joins; transaction semantics (best-effort).

- Exit: SELECT across FOREIGN TABLE and table@link; GRANT/REVOKE on DATABASE LINK enforced.

**Phase 11 — Server (Y-Valve) and protocol/auth**

- Listener and session; Firebird wire protocol compatibility layers; Y-Valve dispatch to embedded/remote/providers with version negotiation.

- Auth providers: password, trusted (SSPI/Kerberos-like), 2FA; TLS; role attributes.

- Exit: Remote clients connect across versions; auth works; basic throughput measured.

**Phase 12 — Backup/restore and PITR**

- Online consistent snapshot; backup format tooling; restore; WAL integration for PITR to-now.

- SHOW BACKUP HISTORY metadata.

- Exit: Backup/restore cycles with PITR validated under load.

**Phase 13 — Replication (logical)**

- WAL shipper (file, remote_db, Kafka); batching, compression; replayer applying logical records idempotently.

- Publications/subscriptions DDL; pause/resume; consistency markers/checkpoints.

- Exit: Change stream replicated to subscriber; switchover tested.

**Phase 14 — Tablespaces and secondary files**

- ****CREATE/ALTER/DROP TABLESPACE; ADD FILE/SET OPTIONS; object placement and MOVE/SET; DETACH/ATTACH.

- Exit: Objects reside and move across spaces; rebalance scripts.

**Phase 15 — Admin/maintenance surfaces**

- Logging/tracing/audit profiles; START/STOP TRACE; CREATE/ALTER/DROP AUDIT POLICY; AUDIT/NOAUDIT.

- Job scheduler/agent; RUN JOB NOW; schedules.

- VACUUM [FULL]; ANALYZE; CREATE STATISTICS; SWEEP, PAGE CACHE, READ CONSISTENCY; START/STOP BACKGROUND TASK.

- Cluster/service/auth provider objects; clustered deployment configs.

- Exit: SQL surfaces operate and produce logs/metrics/artifacts.

**Phase 16 — Security and RLS**

- GRANT/REVOKE full lifecycle; VISIBILITY privilege; RLS policies (USING/WITH CHECK; FORCE).

- Routine security semantics; LOCK TABLE modes.

- Exit: RLS enforcement verified; metadata visibility decoupled from operation.

**Phase 17 — JSON, spatial, and collations**

- JSON/JSONB types/operators; deterministic collations via ICU; spatial types and ST_* ops (via library/extension).

- Exit: JSON and spatial query suites pass; collation rules consistent.

**Phase 18 — Partitioning and materialized views**

- RANGE/LIST/HASH partitioning; attach/detach; pruning; global/local index support.

- CREATE/REFRESH MATERIALIZED VIEW (on-demand/incremental if possible).

- Exit: Partitioned tables operate with pruning; MV refresh correctness/perf validated.

**Phase 19 — Tooling and UX**

- isql: full meta-command set; SHOW HEADER, EXPLAIN [ANALYZE], ANALYZE, VACUUM, admin surfaces.

- CLIs: backup/restore, analyze/vacuum, trace/audit, replication, index tools, page/heap dumpers; perf microbench; catalog inspector.

- Exit: Admin workflows complete.

**Phase 20 — QA, perf gates, and hardening**

- Concurrency/soak tests; randomized/fuzzing (SQL, storage); fault-injection; chaos testing of WAL/replication.

- Perf CI gates with baselines for ops vs page size/distribution; memory/cpu regressions caught.

- Exit: Green CI with perf thresholds; release candidates ready.

**Phase 21 — Packaging and docs**

- Packages/containers; config samples; migration/compat guides; API docs; Doxygen; admin/ops manuals.

- Exit: Install-and-go experience; documented guarantees/limits.

**Phase 22 — ScratchBird Implentation**

- Perform detailed full analysis on existing flamerobin code base

- Create a comprehensive list of features for Alpha

- Create a phased implementation plan

- Execute the phased plan, adjusting as needed.

### Cross-cutting concerns

Telemetry/monitoring throughout (counters, sys.monitoring.* views).

Config knobs surfaced for tunables (I/O, cache, prefetch, WAL, planner).

Error codes/diagnostics standardized; observer tooling (EXPLAIN ANALYZE, validator/fast-check).

Compatibility surface with Firebird highlighted; gaps and workarounds documented.

Short path to first end-to-end usable milestone

- Phases 1–6, 8 (subset), 11 (basic server/auth), 19, 20, 21 → minimal single-node DB: create tables, insert/select/update/delete, basic joins/aggregations, B-Tree indexes, backup/restore, WAL recovery, auth, tooling.

Final hard features

- 9–10, 12–18, 15–16, 13, 14 complete full parity-plus feature set.
