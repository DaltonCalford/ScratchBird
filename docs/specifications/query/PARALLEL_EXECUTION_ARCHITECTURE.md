# ScratchBird Parallel Execution Architecture (Beta)

**Status:** Beta specification (design complete; implementation pending)
**Last Updated:** 2026-01-20
**Scope:** Parallel query execution within a node and across cluster nodes

---

## 1) Purpose and Scope

ScratchBird needs a robust, well-tested parallel execution model that can:

- Split a single query into parallel sub-parts.
- Execute parts concurrently across CPU cores.
- Extend the same model to a cluster with distributed execution.
- Preserve MGA snapshot semantics and deterministic results.

This specification defines the architecture and execution primitives for parallel execution, drawing from proven designs in SQL Server, Oracle, and distributed MPP systems (see References).

---

## 2) Design Goals

1. **Correctness first**: deterministic results, MGA-consistent visibility, stable ordering when required.
2. **Operator-level parallelism**: parallelize scans, joins, aggregates, sorts, and repartition stages.
3. **Scalable scheduling**: fine-grained work units (granules) with dynamic assignment.
4. **Cluster-ready**: same plan model supports intra-node and inter-node execution.
5. **Predictable resource usage**: cost-based DOP, explicit limits, and spill policies.
6. **Transparent observability**: explainable plan (why parallel or not), metrics per operator.

---

## 3) Reference Architecture Patterns

### 3.1 SQL Server (Exchange Operators)
- Parallel plans are created by inserting **exchange operators** into the plan tree.
- Exchange operator types:
  - **Distribute Streams** (scatter/round-robin)
  - **Repartition Streams** (hash on keys)
  - **Gather Streams** (merge into single stream)
- Degree of parallelism (DOP) is chosen by optimizer and may be reduced at runtime if resources are limited.

### 3.2 Oracle (Query Coordinator + PX Servers)
- A **Query Coordinator (QC)** orchestrates parallel execution.
- **PX server processes** act as workers, organized as **producers** and **consumers**.
- Work is split into **granules** (block ranges/partitions), dynamically assigned for load balancing.

### 3.3 MPP / Distributed Query (Control + Compute + Data Movement)
- **Control node** optimizes and splits query into tasks.
- **Compute nodes** execute tasks in parallel.
- A **Data Movement Service (DMS)** redistributes rows between nodes when needed.
- Distribution strategies: **hash**, **round-robin**, **replicate**.

ScratchBird adopts these primitives as formal execution operators and scheduling policies.

---

## 4) Parallel Execution Model

### 4.1 Coordinator/Worker Roles
- **Coordinator**: owns query lifecycle, plan segmentation, DOP selection, worker assignment, result assembly.
- **Workers**: execute plan fragments over assigned granules, produce partial results or shuffled streams.
- **Producers/Consumers**: a worker can act as producer (scan, build) or consumer (join, aggregate, sort).

### 4.2 Plan Segmentation and Exchange Operators
Parallel plans are expressed as plan fragments connected by explicit **Exchange** operators.

Exchange operator types:

- **EXCHANGE_DISTRIBUTE**: scatter tuples (round-robin or broadcast).
- **EXCHANGE_REPARTITION**: hash repartition on keys (join/group by).
- **EXCHANGE_GATHER**: merge or concatenate streams into a single stream.

Each Exchange defines a **parallel boundary**. Fragments on each side are executed by separate worker groups.

### 4.3 Degree of Parallelism (DOP)
- **Optimizer chooses DOP** based on estimated cost and system configuration.
- **Runtime may reduce DOP** if worker slots or memory are insufficient.
- DOP applies per plan segment; total worker count can exceed DOP when multiple segments run concurrently.

### 4.4 Parallel Safety
Operators and functions are tagged with parallel-safety levels:

- **ParallelSafe**: can run in multiple workers without shared-state hazards.
- **ParallelRestricted**: safe only if executed in coordinator (e.g., non-deterministic functions).
- **ParallelUnsafe**: disallows parallelism for the segment.

When an unsafe operator is present, the optimizer inserts a **GATHER** and forces serial execution downstream.

### 4.5 Non-Parallel Plan Reasons
Every plan must expose **NonParallelPlanReason** when parallelism is not used, for example:

- Cost below threshold.
- MAXDOP set to 1.
- Remote query or UDR marked unsafe.
- Unavailable worker resources.

---

## 5) Granule Scheduling

### 5.1 Granule Types
Granules are the smallest work unit assigned to workers.

- **Block Range**: contiguous page ranges within a tablespace.
- **Partition Range**: table partition or shard slice.
- **Index Range**: key range within an index.
- **List Chunk**: explicit list of tuple IDs (rare, maintenance tasks).

### 5.2 Dynamic Assignment
- Coordinator maintains a **granule queue** per scan operator.
- Workers request the next granule when they finish the current one.
- Enables load balancing and minimizes stragglers.

### 5.3 Cluster Extension
Granules can be assigned to **remote workers** on other nodes, with locality preference.

---

## 6) Parallel Operator Semantics

### 6.1 Scans
- Table scans can be parallelized by granule (page range).
- Index scans can be parallelized by key range or bitmap chunk.

### 6.2 Joins
- **Hash join**: repartition both sides on join key, build and probe in parallel.
- **Merge join**: local sorts or distributed order-preserving repartition.
- **Nested loop**: usually serial unless outer side is partitioned and inner access is independent.

### 6.3 Aggregates
- Use **partial aggregation** in workers and a **final aggregation** above an exchange.

### 6.4 Sorts
- Parallel sort uses local sorts with final merge in coordinator.

### 6.5 Inserts/Updates/Deletes
- Allowed when target supports parallel-safe writes and write conflicts are prevented by row ownership rules.
- DML parallelism is restricted by transaction semantics and lock scope.

---

## 7) Distributed Execution (Cluster)

### 7.1 Control Node / Compute Node
- **Control Node**: query parsing, optimization, plan segmentation, DOP assignment.
- **Compute Nodes**: execute plan fragments and return results or shuffled streams.

### 7.2 Data Movement Service (DMS)
The DMS is responsible for:

- **Shuffle**: repartition rows across nodes.
- **Broadcast**: replicate small tables to all workers.
- **Gather**: return streams to control node.

### 7.3 Distribution Policies
Tables can be:

- **Hash-distributed**: rows assigned by hash of key.
- **Round-robin**: even distribution without key alignment.
- **Replicated**: full copy on all nodes (small dimension tables).

Planner must prefer **co-located joins** and only invoke shuffle when necessary.

---

## 8) MGA Semantics and Concurrency

- Each worker executes under the **same MGA snapshot** as the coordinator.
- Snapshot metadata is propagated with task assignments.
- Workers do not commit; only coordinator finalizes the transaction.
- Rollback and cancel must terminate all running tasks.

---

## 9) Resource Governance

Configurable controls:

- **max_parallel_workers** (global, per node)
- **max_parallel_workers_per_query**
- **parallel_cost_threshold**
- **max_dop** (upper bound)
- **parallel_work_mem** (per worker)
- **exchange_buffer_size** (stream buffering)

When limits are exceeded:

- Reduce DOP.
- Spill to disk for sort/aggregate.
- Fallback to serial plan if necessary.

---

## 10) Observability

Add plan/execution metadata:

- **Plan operators** show Exchange nodes with stream counts.
- **NonParallelPlanReason** recorded for serial decisions.
- **Execution metrics**: rows per worker, CPU time per fragment, spill stats, exchange bytes.

Monitoring views should expose:

- Active parallel queries
- Worker utilization
- Exchange queue depth
- Task failures/retries

---

## 11) SBLR Integration

### 11.1 Plan and Opcode Representation
SBLR must support explicit parallel boundaries:

- `SBLR_EXCHANGE_DISTRIBUTE`
- `SBLR_EXCHANGE_REPARTITION`
- `SBLR_EXCHANGE_GATHER`
- `SBLR_PARTIAL_AGG`
- `SBLR_FINAL_AGG`

Each exchange includes routing metadata (hash keys, stream count, distribution policy).

### 11.2 Execution Contract
- Executor uses exchange metadata to schedule tasks and spawn workers.
- Parallelism can be disabled by policy and executed as serial with exchanges collapsed.

---

## 12) Failure Handling

- Worker failure causes fragment retry (if idempotent) or query abort.
- If remote node becomes unavailable, distributed query falls back to available nodes or fails with explicit reason.
- Coordinator must propagate error state to all workers and release resources.

---

## 13) SQL/Session Controls

Proposed control surface:

- `SET max_parallel_workers = <n>`
- `SET max_parallel_workers_per_query = <n>`
- `SET max_dop = <n>`
- `SET parallel_cost_threshold = <float>`
- `SET enable_parallel = ON|OFF`

Optional per-statement hints (future):

- `/*+ PARALLEL(n) */`
- `/*+ NO_PARALLEL */`

---

## 14) Phased Implementation (Beta)

**Beta Phase 1: Intra-node parallelism**
- Exchange operators in plan.
- Granule scheduling for scans.
- Parallel hash aggregation.
- Worker pool management.

**Beta Phase 2: Distributed execution**
- Control/compute roles with DMS.
- Hash/round-robin/replicated distribution.
- Shuffle/broadcast/gather support.

**Beta Phase 3: Adaptive parallelism**
- Runtime DOP adjustments based on load.
- Adaptive repartition for skew.
- Enhanced parallel plan feedback for optimizer.

---

## 15) References

1. SQL Server Query Processing Architecture Guide (Exchange operators, DOP behavior)
   https://learn.microsoft.com/en-us/sql/relational-databases/query-processing-architecture-guide?view=sql-server-ver16
2. Oracle Database Concepts - Parallel Execution (QC + PX servers, granules)
   https://docs.oracle.com/en/database/oracle/oracle-database/21/cncpt/process-architecture.html
3. Azure Synapse SQL architecture (Control node, Compute nodes, DMS, distribution types)
   https://learn.microsoft.com/en-us/azure/synapse-analytics/sql/overview-architecture

---

## 16) Related Specifications

- Query Optimizer: `docs/specifications/query/QUERY_OPTIMIZER_SPEC.md`
- SBLR: `docs/specifications/sblr/README.md`
- Cluster Distributed Query: `docs/specifications/Cluster Specification Work/SBCLUSTER-06-DISTRIBUTED-QUERY.md`
- Transactions: `docs/specifications/transaction/README.md`
- Storage: `docs/specifications/storage/README.md`

