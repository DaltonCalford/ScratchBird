# Emulated Engine Process Flow Optimization Tracks (2026-02-11)

## Scope
- Goal: compare process flow efficiency patterns in emulated engine source trees and extract optimization tracks for ScratchBird plan generation and result set production.
- Focus: parser to planning to execution to result streaming path.
- Constraint alignment: ScratchBird engine remains UUID plus SBLR only; SQL dialect handling stays parser-side.

## Method
- Reviewed optimizer, planner, executor, cache, paging, and scheduler code paths in local reference clones:
  - Firebird
  - PostgreSQL
  - MySQL
  - Cassandra
  - MongoDB
  - Neo4j
  - Redis
  - Milvus
- Extracted direct behavior evidence from source code, then mapped each pattern into ScratchBird-compatible architecture decisions.

## Evidence Anchors
- Firebird:
  - `/home/dcalford/CliWork/firebird/src/jrd/optimizer/Optimizer.cpp:2764`
  - `/home/dcalford/CliWork/firebird/src/jrd/optimizer/Optimizer.cpp:2782`
  - `/home/dcalford/CliWork/firebird/src/jrd/optimizer/Optimizer.cpp:2821`
  - `/home/dcalford/CliWork/firebird/src/jrd/optimizer/Retrieval.cpp:1132`
- PostgreSQL:
  - `/home/dcalford/CliWork/postgresql/src/backend/optimizer/path/allpaths.c:177`
  - `/home/dcalford/CliWork/postgresql/src/backend/optimizer/path/costsize.c:449`
  - `/home/dcalford/CliWork/postgresql/src/backend/optimizer/path/costsize.c:517`
  - `/home/dcalford/CliWork/postgresql/src/backend/optimizer/path/costsize.c:2645`
- MySQL:
  - `/home/dcalford/CliWork/mysql-server/sql/join_optimizer/cost_model.cc:69`
  - `/home/dcalford/CliWork/mysql-server/sql/join_optimizer/cost_model.cc:106`
  - `/home/dcalford/CliWork/mysql-server/sql/sql_executor.cc:129`
  - `/home/dcalford/CliWork/mysql-server/sql/sql_executor.cc:2117`
  - `/home/dcalford/CliWork/mysql-server/sql/sql_executor.cc:2257`
- Cassandra:
  - `/home/dcalford/CliWork/cassandra/src/java/org/apache/cassandra/transport/messages/QueryMessage.java:117`
  - `/home/dcalford/CliWork/cassandra/src/java/org/apache/cassandra/cql3/statements/SelectStatement.java:363`
  - `/home/dcalford/CliWork/cassandra/src/java/org/apache/cassandra/cql3/statements/SelectStatement.java:411`
  - `/home/dcalford/CliWork/cassandra/src/java/org/apache/cassandra/service/StorageProxy.java:2625`
  - `/home/dcalford/CliWork/cassandra/src/java/org/apache/cassandra/service/StorageProxy.java:2665`
- MongoDB:
  - `/home/dcalford/CliWork/mongo/src/mongo/db/query/plan_cache/README.md:105`
  - `/home/dcalford/CliWork/mongo/src/mongo/db/query/plan_cache/README.md:131`
  - `/home/dcalford/CliWork/mongo/src/mongo/db/query/query_planner.cpp:1726`
  - `/home/dcalford/CliWork/mongo/src/mongo/db/query/query_planner.cpp:1780`
- Neo4j:
  - `/home/dcalford/CliWork/neo4j/community/cypher/planner-spi/src/main/scala/org/neo4j/cypher/internal/planner/spi/PlannerName.scala:33`
  - `/home/dcalford/CliWork/neo4j/community/cypher/cypher-cache/src/main/scala/org/neo4j/cypher/internal/cache/TwoLayerCache.scala:36`
- Redis:
  - `/home/dcalford/CliWork/redis/src/server.c:3830`
  - `/home/dcalford/CliWork/redis/src/server.c:3937`
  - `/home/dcalford/CliWork/redis/src/server.c:1857`
  - `/home/dcalford/CliWork/redis/src/networking.c:2693`
- Milvus:
  - `/home/dcalford/CliWork/milvus/internal/querycoordv2/task/scheduler.go:793`
  - `/home/dcalford/CliWork/milvus/internal/querycoordv2/balance/balance.go:52`
  - `/home/dcalford/CliWork/milvus/internal/parser/planparserv2/parser_visitor.go:1380`

## Comparative Efficiency Findings
1. Firebird
- More efficient at deterministic join choice under hash-memory pressure: explicit `hashOverflow` gate prefers merge join.
- More efficient at low-overhead index candidate costing: simple, stable cost plus selectivity equations.
- Less efficient for broad adaptive plan-cache behavior compared with MongoDB and PostgreSQL-style memoization.
- ScratchBird takeaway: keep Firebird-like deterministic gates for join overflow fallback.

2. PostgreSQL
- More efficient for mature cost decomposition and parallel pipeline accounting (`parallel_setup_cost`, `parallel_tuple_cost`, gather merge uplift).
- More efficient for parameterized re-scan workloads via memoize hit and eviction modeling.
- Potentially less efficient for very short queries due to heavier planning machinery.
- ScratchBird takeaway: adopt explicit operator cost formulas and memoized subplan policy.

3. MySQL
- More efficient where linear-regression cost equations were calibrated for optimizer path classes.
- More efficient in execution-time practicality: record buffer sizing balances low-latency and high-concurrency behavior.
- More efficient for limit-aware hash-join spill policy (`allow_spill_to_disk` gates).
- ScratchBird takeaway: combine optimizer-time and executor-time guardrails, especially limit-aware spill controls.

4. Cassandra
- More efficient for distributed consistency-aware reads: closest-replica data plus digest, speculative retries, read repair.
- More efficient for page/limit-aware result acquisition through `DataLimits` plus `QueryPager`.
- Less efficient as a direct model for single-node relational query optimization.
- ScratchBird takeaway: use this primarily for cluster-mode distributed read and repair flow contracts.

5. MongoDB
- More efficient in adaptive plan cache lifecycle: inactive to active promotion, trial run, replan thresholds.
- More efficient in runtime protection against stale bad plans (works/read metrics with deactivation and replan).
- Less efficient determinism in equal-cost tie handling (known unresolved tie-selection behavior remains).
- ScratchBird takeaway: adopt stateful plan-cache lifecycle with trial execution metrics.

6. Neo4j
- More efficient for graph workload cache behavior: two-layer cache and configurable planner modes.
- More efficient cache retention under churn through primary plus secondary strategy.
- Less directly applicable to relational join-order enumeration.
- ScratchBird takeaway: apply two-layer cache strategy to parser artifacts, plan artifacts, and name-map lookups.

7. Redis
- More efficient at tail-latency control and fairness: event loop fairness cap and per-command latency telemetry.
- More efficient operational observability with cheap always-on command timing paths.
- Less directly applicable to complex relational optimization due to single-thread command model.
- ScratchBird takeaway: port fairness and latency accounting concepts to coordinator and result streaming pipeline.

8. Milvus
- More efficient in strict parser-level expression validation and normalization for vector-query semantics.
- More efficient in coordinator scheduling clarity for distributed query tasks.
- Current balancing is intentionally simple round-robin, not globally cost-optimal.
- ScratchBird takeaway: parser normalization strictness should be mirrored in native parser before SQL->SBLR conversion.

## Recommended Optimization Tracks For ScratchBird
## P0 (must-have before full DDL/DML/PSQL/TSQL language finalization)
1. Plan cache state machine
- Implement `inactive`, `active`, `trial`, `deactivated`, `replan_required`.
- Track `works`, `reads`, first-batch latency, and spill count per plan UUID.

2. Explicit cost model formulas
- Define formulas for scan, index seek, hash join, merge join, nested loop, sort, gather, gather-merge, materialize.
- Store calibrated coefficients in catalog data, not hard-coded constants.

3. Join and spill guardrails
- Add deterministic hash-overflow fallback to merge join for applicable predicates.
- Add limit-aware spill gating policy at executor level.

4. Parallel coordinator contract
- Add deterministic plan fragment scheduler contract, backpressure windows, and partial-result merge contract.
- Require fairness caps to prevent starvation during large result streams.

5. Parser normalization gate
- Force strict type-checked normalization and constant folding before SQL->SBLR emission.
- Reject non-normalized AST emission into SBLR compiler path.

## P1 (next optimization layer)
1. Distributed read strategy for cluster mode
- Add consistency-level strategy contracts, speculative replica retry, and repair workflow mappings.

2. Two-layer caches
- Add primary plus secondary cache patterns for parse trees, normalized AST, and plan templates.

3. End-to-end latency telemetry
- Standardize p50/p95/p99/p99.9 for parse, plan, execute, stream, and client-send stages.

## P2 (advanced improvements)
1. Cost-based scheduler upgrades for distributed fragments
- Start with deterministic weighted round-robin and move toward cost-aware scheduling.

2. Deterministic tie-break rules for equal-cost plan candidates
- Avoid unstable plan choice across identical metadata snapshots.

## Gaps In Current ScratchBird Specs Revealed By This Pass
1. Need explicit normative plan-cache lifecycle state transitions and failure modes.
2. Need formula-level cost model definitions with catalog-stored coefficients and recalibration process.
3. Need executor fairness and spill policy language tied to LIMIT, ORDER, GROUP behaviors.
4. Need distributed read/repair flow mapping for cluster-mode parity targets.
5. Need strict parser normalization rejection criteria before SBLR generation.

## Immediate Spec Actions
1. Add a section-23 normative doc for plan cache lifecycle states, thresholds, and invalidation transitions.
2. Add a section-23 normative doc for cost model formulas and calibration metadata tables.
3. Add a section-23 executor policy doc for spill, fairness, backpressure, and first-row latency targets.
4. Add a section-28 parser contract doc for normalization gate and SQL->SBLR emission preconditions.
5. Add section-25/26 cluster read strategy doc for speculative reads and repair-level policy controls.

## Conclusion
- The strongest direct optimization sources for ScratchBird relational flow are PostgreSQL plus MySQL for cost and execution policy, Firebird for deterministic MGA-aligned behavior, and MongoDB for adaptive plan cache control.
- Cassandra, Neo4j, Redis, and Milvus contribute specialized optimizations for distributed consistency, cache layering, fairness telemetry, and strict parser normalization.
