# High Performance OLTP Plan Shapes, Contention Avoidance, and Prepared Execution Model

Status: current_authority_with_reconstructed_expansion

## Purpose

Define the current authoritative fast-path rules for:

- high-performance OLTP point reads and point DML
- prepared statement reuse
- prepared-query performance bundles
- prepared-plan and result-cache interaction
- hot-key and contention avoidance

This file exists so implementation agents can land prepared-query performance
and OLTP reuse behavior without guessing:

- which reuse surface owns which artifact
- when a prepared statement may reuse a bound plan or row layout
- when a prepared execution may also use the executor result cache
- when schema, security, placement, parameter regime, or policy change force
  rebuild

## Scope

This file owns:

- prepared OLTP fast-path admission
- prepared execution identity
- prepared fast-path bundle contents
- distinction between prepared bundles, plan cache, and result cache
- contention-avoidance rules for hot right edge, hot keys, and exact-secondary
  maintenance
- explain, metrics, refusals, and tests for prepared-query performance

This file does not replace:

- section `23` plan-cache or executor result-cache ownership
- section `18` exact-family legality, `ICP`, `MRR`, or `BKA`
- section `34` heap multi-insert and heap-only update semantics
- section `39` bulk ingest lane authority
- section `33` grant or spill ownership
- section `03` locality and NUMA ownership

## Reader rule

Examples in this file are user-surface examples for shape intuition only.
ScratchBird still executes canonical lowered SBLR and internal procedures, not
SQL text directly.

## Hard invariants

1. Prepared execution is a reuse optimization, not a semantics change.
2. A prepared statement handle is not the same thing as a reusable plan-cache
   entry.
3. A prepared fast-path bundle is not the same thing as a query result cache.
4. A result cache hit may skip execution only for cacheable top-level `SELECT`
   shapes. It shall never stand in for DML execution.
5. Hot-path reuse may survive only while schema, security, placement, policy,
   and parameter-regime identities remain valid.
6. Prepared-query reuse may not bypass MGA visibility, security checks, or
   explicit runtime refusal boundaries.
7. Multi-row prepared batches must keep one stable prepared identity for the
   batch and may not silently fan out into unrelated per-row layouts.

## Canonical workload shapes

The planner shall recognize these prepared OLTP performance shapes:

| Shape | Meaning |
| --- | --- |
| `PREPARED_POINT_SELECT` | single-key point read or short exact-key set |
| `PREPARED_POINT_UPDATE` | single-key update with bounded touched rows |
| `PREPARED_POINT_DELETE` | single-key delete with bounded touched rows |
| `PREPARED_APPEND_INSERT` | append-heavy insert with stable key/layout posture |
| `PREPARED_MICRO_BATCH_INSERT` | small multi-row insert with one stable layout |
| `PREPARED_BOUNDED_LOOKUP_JOIN` | small bounded lookup join against one keyed relation |
| `PREPARED_BOUNDED_INSERT_SELECT` | `INSERT ... SELECT` from a bounded local producer only |

The following are not OLTP fast-path shapes under this file:

- broad scans
- wide analytical joins
- large unbounded `INSERT ... SELECT`
- large file-backed `COPY`
- full-sort analytical windows
- batch DML that materially changes placement or routing per row

## Reuse surface taxonomy

Prepared-query performance uses four distinct reuse surfaces.

| Surface | Owner | Purpose | Reused artifacts | Never conflated with |
| --- | --- | --- | --- | --- |
| `PreparedStatementHandle` | protocol or front-end session | stable parameterized statement identity | parameter slots, lowered-shape id, dependency skeleton | plan cache |
| `PreparedFastPathBundle` | planner plus executor | high-performance runtime bundle for a legal prepared shape | bound object ids, key program, row layout, write program, result metadata | result cache |
| `VNextPlanCache` | optimizer | reusable frozen runtime plan | validated runtime plan payload and native-ready artifacts | prepared handle |
| `QueryResultCache` | executor | reusable final rowset for cacheable select | result rows and result metadata | prepared bundle |

### Non-conflation rules

1. Preparing a statement does not by itself admit a plan-cache entry.
2. A plan-cache hit does not by itself prove a prepared fast path exists.
3. A prepared fast-path bundle hit does not by itself prove a result-cache hit.
4. A result-cache hit may occur on an executed prepared statement only after
   prepared identity and security validation succeed.

## Canonical prepared identities

### `PreparedStatementIdentity`

Every prepared statement shall own this logical identity:

```cpp
struct PreparedStatementIdentity {
  Uuid prepared_statement_uuid;
  string canonical_shape_hash;
  string parameter_type_signature;
  string parameter_arity_signature;
  string rewrite_contract_id;
  string execution_intent;
  string dialect_lowering_signature;
  string schema_dependency_signature;
  string security_dependency_signature;
  string policy_snapshot_signature;
};
```

Rules:

1. the canonical shape hash shall be stable across executions that differ only
   by parameter values
2. parameter type or arity change produces a new identity
3. rewrite contract or dialect lowering change produces a new identity
4. policy snapshot change may preserve the handle but invalidates the fast-path
   bundle when performance semantics differ materially

### `PreparedParameterRegimeSignature`

Prepared execution shall classify runtime values into a bounded regime
signature:

```cpp
struct PreparedParameterRegimeSignature {
  string selectivity_regime;
  string cardinality_regime;
  string route_regime;
  string order_requirement_regime;
  string volatility_regime;
};
```

Required bounded regime families:

- `POINT_SINGLETON`
- `POINT_SMALL_SET`
- `RANGE_TINY`
- `RANGE_MEDIUM`
- `ROUTE_SINGLE`
- `ROUTE_MULTI`
- `ORDER_REQUIRED`
- `ORDER_NOT_REQUIRED`

No implementation may create unbounded per-value regimes. Regimes must stay
bucketed and explainable.

### `PreparedFastPathBundle`

For an admitted prepared performance shape, the planner or binder shall build:

```cpp
struct PreparedFastPathBundle {
  Uuid prepared_statement_uuid;
  string fast_path_shape;
  string parameter_regime_signature;
  vector<Uuid> bound_relation_uuids;
  vector<Uuid> bound_index_uuids;
  string plan_cache_key;
  string key_extraction_program_id;
  string row_layout_identity;
  string write_program_identity;
  string conflict_check_posture;
  string result_shape_identity;
  string locality_posture;
  bool result_cache_candidate;
  bool plan_cache_candidate;
  bool hot_key_mitigation_required;
  bool exact_secondary_deferral_allowed;
};
```

Required field meanings:

1. `fast_path_shape` is one of the prepared shapes listed above
2. `parameter_regime_signature` is the bounded bucket for the live value shape
3. `plan_cache_key` may be empty only when the plan is intentionally uncached
4. `row_layout_identity` is the canonical row serialization layout for the
   prepared path
5. `write_program_identity` is required for prepared DML fast paths
6. `result_cache_candidate` is true only for cacheable top-level selects
7. `hot_key_mitigation_required` is true only when table policy and shape prove
   hot-key behavior

## Canonical process flow

### Flow A: prepare

The `PREPARE` or equivalent front-end operation shall execute this exact flow:

1. lower the statement to canonical shape
2. build `PreparedStatementIdentity`
3. classify the primary prepared shape or reject fast-path admission
4. derive the bounded parameter regime classifier
5. build or reuse a `PreparedFastPathBundle` template without parameter values
6. record dependency signatures for:
   - schema
   - security
   - policy
   - placement or route state
   - family statistics signature when the fast path is cost-sensitive
7. optionally seed the VNext plan cache if a frozen plan is already available
8. publish the prepared handle

Required outputs from `PREPARE`:

- prepared statement handle id
- fast-path admission yes or no
- fast-path refusal reason when no
- prepared layout identity
- plan-cache candidate yes or no

### Flow B: execute prepared point read

On `EXECUTE` for a prepared point-read shape, the runtime shall apply this
exact order:

1. resolve the prepared handle
2. validate schema, security, and policy identities
3. derive the live `PreparedParameterRegimeSignature`
4. resolve or rebuild the `PreparedFastPathBundle`
5. if `result_cache_candidate = true`, build the executor result-cache key and
   probe `QueryResultCache`
6. if result-cache hit, return rows immediately
7. otherwise probe the VNext plan cache using the plan-cache key
8. if plan-cache miss or stale, replan and publish a fresh plan when legal
9. execute using the prepared bundle and the frozen runtime plan
10. if the result is cacheable, insert the final rowset into `QueryResultCache`
11. publish prepared execution metrics and feedback

No implementation may probe result cache before prepared identity validation.

### Flow C: execute prepared point DML

On `EXECUTE` for prepared point `INSERT`, `UPDATE`, or `DELETE`, the runtime
shall apply this exact order:

1. resolve the prepared handle
2. validate schema, security, policy, and placement identities
3. derive the live parameter regime
4. resolve or rebuild the fast-path bundle
5. probe plan cache or rebuild the runtime plan
6. execute the write program using the stable row layout and key program
7. publish table and plan invalidations as required on commit
8. update prepared fast-path metrics

Prepared DML never probes or inserts the executor result cache.

### Flow D: execute prepared micro-batch insert

For `PREPARED_MICRO_BATCH_INSERT`, the runtime shall:

1. validate that every row in the batch uses one `row_layout_identity`
2. validate that every row in the batch uses one conflict-check posture
3. perform one layout resolution for the whole batch
4. perform one exact-maintenance strategy resolution for the whole batch
5. select one write lane for the batch
6. apply one preallocation decision for the batch
7. execute the batch with one stable prepared bundle identity

If a batch contains mixed layouts, mixed route policy, or mixed conflict
posture, the batch is not a prepared micro-batch fast path and must fail closed
to the ordinary row path or be split explicitly by the caller.

### Flow E: invalidation and rebuild

When a dependency changes, the runtime shall apply this order:

1. mark the prepared bundle stale
2. invalidate the associated plan-cache entry if its dependencies changed
3. invalidate executor result-cache entries by referenced table when required
4. preserve the prepared statement handle if only the bundle is stale
5. force bundle rebuild on the next execute attempt

The prepared handle itself shall be retired only when:

- canonical shape changes
- parameter arity or type shape changes
- dialect or rewrite contract changes
- protocol or front-end explicitly deallocates it

## Prepared-query result-cache rules

### Cacheability

A prepared query may use the executor result cache only when all of the
following hold:

1. it is a top-level `SELECT`
2. the lowered shape is marked cacheable by planner or executor policy
3. it is deterministic and non-volatile
4. it does not depend on transaction-local mutable state that the current cache
   model cannot prove safe
5. the final result size stays within result-cache admission limits

### Required result-cache key dimensions

For prepared queries, the final result-cache key shall include:

- prepared statement identity or canonical lowered shape identity
- statement index
- strict-mode flag
- bound parameter values and null map
- current user id
- active role id
- security policy epoch
- result-shape identity

The result-cache key may additionally include the plan-profile signature when
the result depends on plan-governed semantics.

### Required process order

When prepared-query result caching is legal, the runtime shall:

1. validate the prepared handle
2. validate the prepared bundle
3. build the final result-cache key
4. probe the executor result cache
5. on miss, execute the plan
6. on success, insert the final rowset into the result cache

### Forbidden behavior

The runtime shall not:

- return a result-cache hit for a prepared DML statement
- reuse a prepared result cache entry across security epoch mismatch
- reuse a prepared result cache entry across parameter-value mismatch
- reuse a prepared result cache entry across statement-shape mismatch

## Contention-avoidance rules

### Point read and point DML

For admitted prepared point shapes:

1. exact-key extraction shall be compiled or resolved once per bundle, not once
   per row
2. bound table and index identities shall be resolved once per valid bundle
3. same-key update suppression shall be preserved when indexed state is
   unchanged
4. ordered hot-right-edge insert shall prefer the section `18` and section `34`
   mitigation paths before generic pessimistic fallback

### Hot-key mitigation

Hot-key mitigation is legal only when explicit table or route policy declares
it. When legal, the prepared bundle shall record:

- hot-key mitigation on or off
- route class
- refusal reason when the mitigation is unsafe

### Exact-secondary maintenance

Prepared point writes and micro-batches shall prefer:

1. unchanged-key suppression
2. statement-local metadata reuse
3. batch apply or deferred exact-secondary merge when the shape and policy
   allow it

Prepared execution may not invent a maintenance mode that was not admitted by
section `18`.

## Canonical examples

### Example 1: prepared point select with result cache

Surface-shape example:

```sql
PREPARE q1(int) AS
  SELECT customer_id, status
  FROM customers
  WHERE customer_id = $1;
```

Required prepared behavior:

1. classify `PREPARED_POINT_SELECT`
2. build one key-extraction program for `customer_id = $1`
3. build one result-shape identity for `(customer_id, status)`
4. on execute, validate prepared bundle
5. probe result cache with parameter value `$1`
6. on miss, execute point read
7. on hit, return cached rowset without re-executing the plan

### Example 2: prepared point update

Surface-shape example:

```sql
PREPARE q2(text, int) AS
  UPDATE customers
  SET status = $1
  WHERE customer_id = $2;
```

Required prepared behavior:

1. classify `PREPARED_POINT_UPDATE`
2. bind exact-key program for `customer_id`
3. bind one row layout for the changed columns
4. preserve same-key exact-index suppression if indexed columns are unchanged
5. never consult result cache
6. publish table invalidation only on successful commit

### Example 3: prepared micro-batch insert

Surface-shape example:

```sql
PREPARE q3(int, text) AS
  INSERT INTO customers(customer_id, status)
  VALUES ($1, $2);
```

Caller then executes `q3` in a micro-batch of stable `(int, text)` rows.

Required prepared behavior:

1. one row-layout identity for the whole batch
2. one exact-maintenance posture for the whole batch
3. one write-lane decision for the whole batch
4. one preallocation decision for the batch window
5. no per-row layout or route recompilation inside the hot loop

### Example 4: prepared bounded lookup join

Surface-shape example:

```sql
PREPARE q4(int) AS
  SELECT o.order_id, c.status
  FROM orders o
  JOIN customers c ON c.customer_id = o.customer_id
  WHERE o.order_id = $1;
```

Required prepared behavior:

1. classify `PREPARED_BOUNDED_LOOKUP_JOIN`
2. bind the point-read shape on `orders`
3. bind the indexed lookup on `customers`
4. optionally admit memoize or batched key access if the lowered shape and live
   bundle qualify
5. admit result cache only if the top-level select is cacheable

## Explain requirements

`EXPLAIN` or equivalent runtime-plan disclosure shall show:

- `OLTP_FAST_PATH = true|false`
- `PREPARED_FAST_PATH_SHAPE`
- `PREPARED_BUNDLE_HIT = true|false`
- `PLAN_CACHE_HIT = true|false`
- `RESULT_CACHE_HIT = true|false`
- `PREPARED_LAYOUT_IDENTITY`
- `PREPARED_PARAMETER_REGIME`
- `HOT_KEY_MITIGATION = true|false`
- fast-path refusal reason when the fast path is absent

## Structured refusal rules

The following refusal codes are canonical under this file:

- `OLTP_FAST_PATH_SCHEMA_STALE`
- `OLTP_FAST_PATH_SECURITY_STALE`
- `OLTP_FAST_PATH_POLICY_STALE`
- `OLTP_FAST_PATH_PLACEMENT_STALE`
- `OLTP_FAST_PATH_PARAMETER_REGIME_MISMATCH`
- `OLTP_FAST_PATH_SHAPE_UNSUPPORTED`
- `OLTP_FAST_PATH_HOT_KEY_UNSAFE`
- `OLTP_FAST_PATH_LAYOUT_MISMATCH`
- `OLTP_FAST_PATH_RESULT_CACHE_INELIGIBLE`
- `OLTP_FAST_PATH_PLAN_CACHE_STALE`

Each refusal shall also preserve:

- `candidate_shape`
- `cause_domain`
- `rejected_at_stage`

## Metrics

The runtime shall publish at least:

- prepared fast-path hit ratio
- prepared bundle rebuild count
- prepared layout reuse hit ratio
- prepared plan-cache hit ratio
- prepared result-cache hit ratio
- parameter-regime mismatch count
- hot-key mitigation usage count
- hot-key refusal count
- micro-batch size distribution
- point-DML latency distribution

## Required tests

Before this file is considered closed, tests shall prove:

1. prepared point select reuses the prepared bundle across executions with the
   same dependency and parameter regime
2. prepared point select may hit the executor result cache without bypassing
   security or parameter-value identity
3. prepared point update never uses the result cache
4. prepared point update preserves unchanged-key exact-index suppression when
   legal
5. prepared micro-batch insert preserves one row-layout identity for the full
   batch
6. schema or security change invalidates the prepared bundle before the next
   execution
7. prepared handle survives bundle invalidation when the canonical shape is
   unchanged
8. plan-cache invalidation does not incorrectly retire the prepared handle
9. result-cache invalidation by referenced table removes prepared select result
   entries after committed change
10. parameter-regime mismatch either rebuilds the bundle or emits the canonical
    refusal instead of silently reusing the wrong fast path

## Cross references

- `PLAN_CACHE_AND_INVALIDATION_RULES.md`
- `../23_SBLR_VM_Compiler_and_Executor/EXECUTION_CACHE_AND_INVALIDATION.md`
- `../23_SBLR_VM_Compiler_and_Executor/PLAN_CACHE_PARALLELISM_AND_OPTIMIZER_FEEDBACK.md`
- `../23_SBLR_VM_Compiler_and_Executor/QUERY_PERFORMANCE_ORCHESTRATION_AND_CROSS_LAYER_COORDINATION_MODEL.md`
- `../18_Index_Framework/SECONDARY_ACCESS_LOCALITY_PUSHDOWN_AND_COVERING_EXECUTION_MODEL.md`
- `../34_Table_Storage_and_Access_Methods/HEAP_MULTI_INSERT_AND_HEAP_ONLY_UPDATE_PERFORMANCE_MODEL.md`
- `../39_Backup_Restore_and_Bulk_Data_Paths/BULK_INGEST_LANES_AND_SHADOW_LOAD_CUTOVER_MODEL.md`
