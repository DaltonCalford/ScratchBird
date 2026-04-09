Status: current_authority_beta2

# Beta 2 Donor Dialect Execution and Planner Binding Model

## Purpose

Define the planner, compiler, runtime-stage, and explain-surface obligations for
the Beta 2 donor dialect expansion so the new v3 and SBLR carriers become
first-class executable structures instead of parser-only syntax holders.

## Hard invariants

1. The planner and executor operate on canonical AST and SBLR only. Donor
   parser packages may not ship donor-private runtime paths for surfaces
   admitted in sections `21` and `22`.
2. Every new clause family shall have a fixed execution stage. `PREWHERE`,
   `QUALIFY`, `LIMIT BY`, temporal binding, and row-shaping transforms shall not
   be reordered by implementation convenience.
3. Explain and runtime-plan output shall preserve distinct node identity for
   every new first-class surface. No Beta 2 surface may appear only as an
   opaque annotation.
4. Unsupported runtime closure for an admitted Beta 2 carrier must fail closed
   with a deterministic capability error. Silent downgrade to a weaker plan is
   forbidden.

## Canonical stage order

The canonical select pipeline order for Beta 2 is:

1. parse-time desugar and canonicalization
2. table-source binding and table-scope temporal binding
3. table-source transforms:
   `JSON_TABLE`, `ROWS FROM`, `PIVOT`, `UNPIVOT`, `MATCH_RECOGNIZE`,
   `ARRAY JOIN`, and `FINAL`
4. `PREWHERE`
5. `WHERE`
6. grouping and aggregate build
7. `HAVING`
8. window evaluation
9. `QUALIFY`
10. `ORDER BY`
11. `LIMIT BY`
12. `LIMIT`, `OFFSET`, and `FETCH`

Rules:

1. `PREWHERE` is an early filter and must remain earlier than `WHERE`.
2. `QUALIFY` is a post-window filter and must remain later than window
   evaluation.
3. `LIMIT BY` applies after ordering and before the terminal limit stage.
4. Table-scope temporal clauses are resolved before the first physical access
   path is opened.

## Execution bindings by feature bucket

### 1. Desugar-only surfaces

1. `APPLY` is a parser-only spelling alias. The planner sees the already-
   lowered lateral join tree and must generate a normal join plan with
   `lateral = true` semantics.
2. `CREATE MATERIALIZED VIEW` is a parser-only spelling alias. The executor
   continues to use the existing `CreateViewStmt` runtime path with the
   materialized flag populated.
3. `REPLACE`, `INSERT IGNORE`, `ON DUPLICATE KEY UPDATE`, and `UPSERT` remain
   `InsertStmt` plan families. The planner and executor must branch on the
   canonical `surface_flavor` instead of reparsing donor text.

### 2. Function, aggregate, and window bindings

1. The executor shall dispatch window functions by canonical function symbol,
   not by the former eight-name whitelist.
2. Ordered-set aggregates shall evaluate direct arguments once per aggregate
   group and shall apply `WITHIN GROUP` ordering through a dedicated ordered-set
   path rather than normal aggregate `ORDER BY`.
3. Null-treatment and `FROM FIRST` or `FROM LAST` must be available to both the
   planner and executor when the donor surface requires them.
4. Named-argument and variadic metadata are compile-time binding inputs and
   shall not leak into executor heuristics as string parsing.

### 3. Table-source bindings

1. `JSON_TABLE` shall compile into a table-producing operator with explicit
   row-path, column projection, and nested-column subplans.
2. `ROWS FROM (...)` shall compile into a multi-function fan-out operator that
   preserves per-function ordinality and column alias identity.
3. `PIVOT` and `UNPIVOT` shall compile into explicit row-shape transform
   operators rather than macro-expanded text.
4. `MATCH_RECOGNIZE` shall compile into a row-pattern operator with partition,
   order, measure, pattern, and define sections preserved.
5. `ARRAY JOIN` shall compile into an explicit expansion operator and may not be
   folded into a scalar function call.
6. `FINAL` shall be visible at scan selection time so the chosen access path can
   honor the donor-visible final-read contract.

### 4. Temporal, session, and history bindings

1. Table-scope `AS OF`, `AS OF SYSTEM TIME`, `AS OF TIMESTAMP`, `FOR VALID_TIME`,
   and `FOR SYSTEM_TIME` shall compile into snapshot-resolution metadata on the
   relevant scan or table-source operator.
2. `USE SNAPSHOT ...` shall alter the session temporal scope through a dedicated
   runtime path rather than by mutating parser-only state.
3. `HISTORY OF` shall compile into an explicit history-source operator.
4. Session-scope temporal state shall be visible in plan keys so plan-cache
   reuse cannot cross incompatible snapshot scopes.

### 5. Policy, distribution, and metadata bindings

1. Cassandra keyspace surfaces, Cockroach locality or placement surfaces, TiDB
   placement or split surfaces, and Yugabyte tablegroup or tablet surfaces
   shall compile into structured DDL or control-plan payloads using the policy
   option carrier from section `21`.
2. InfluxQL metadata show surfaces shall compile into metadata-read plans with
   stable result-shape identifiers.
3. `CHANGEFEED`, `SCRUB`, `ALTER RANGE`, `TABLEGROUP`, and split-region surfaces
   shall each receive deterministic plan or control-operation ids even when the
   current implementation path is package-owned.

### 6. Multi-model command bindings

1. `MultiModelCommandStmt` shall compile into canonical command-dispatch plans.
2. Existing covered families such as CQL batch or TTL surfaces, Mongo
   `find/aggregate/findAndModify/bulkWrite`, Redis PubSub, and Milvus core
   verbs shall continue to use package-owned runtime handlers, but the planner
   front door shall be canonical.
3. New verbs such as Mongo `count/distinct/createIndexes/listCollections`,
   Redis `MULTI/EXEC/WATCH/UNWATCH`, Milvus `HybridSearch`, OpenSearch bulk and
   index-admin verbs, and FoundationDB CLI verbs shall be admitted through the
   same command-dispatch plan family.
4. Parser packages never execute these command verbs locally. They compile and
   forward canonical command plans only.

## Required first-class plan nodes and metrics

The planner and explain pipeline shall expose distinct node kinds for at least:

1. `LATERAL_APPLY`
2. `JSON_TABLE_SCAN`
3. `ROWS_FROM_FANOUT`
4. `PIVOT_TRANSFORM`
5. `UNPIVOT_TRANSFORM`
6. `ROW_PATTERN_MATCH`
7. `TEMPORAL_BIND`
8. `ARRAY_JOIN_EXPAND`
9. `PREWHERE_FILTER`
10. `QUALIFY_FILTER`
11. `LIMIT_BY`
12. `MULTI_MODEL_COMMAND_DISPATCH`

Every new Beta 2 plan node shall expose the following minimum metrics:

1. `input_rows_estimate`
2. `output_rows_estimate`
3. `selectivity_estimate`
4. `fanout_estimate` where row multiplication is possible
5. `cpu_cost`
6. `memory_bytes_estimate`
7. `spill_risk_class`
8. `pushdown_state`
9. `temporal_scope_class` where applicable

## Plan-cache and determinism obligations

1. Temporal scope, window-function identity, ordered-set identity, and
   multi-model verb identity shall participate in plan-cache keys.
2. Explain output shall retain the canonical node identity even when the donor
   parser later translates the plan into donor-native phrasing.
3. Native v3 and donor parser packages shall produce structurally equivalent
   plan trees when they target the same canonical Beta 2 carrier.

## Sample planner sketch

```cpp
PlanNode* planSelectWithExtensions(const SelectStmt& stmt) {
  PlanNode* root = planFromClause(stmt.from, stmt.joins, stmt.temporal);
  root = applyArrayJoins(root, stmt.array_join_items);
  root = applyPrewhere(root, stmt.prewhere);
  root = applyWhere(root, stmt.where);
  root = applyGrouping(root, stmt.group_by, stmt.having);
  root = applyWindows(root, stmt.windows, stmt.items);
  root = applyQualify(root, stmt.qualify);
  root = applyOrdering(root, stmt.order_by);
  root = applyLimitBy(root, stmt.limit_by);
  root = applyLimitOffsetFetch(root, stmt.limit, stmt.offset, stmt.fetch_row_count);
  return root;
}
```

## Required proof

1. every Beta 2 select-stage clause shall appear in plan and explain output as
   a first-class node or metric block
2. generic window-function execution shall preserve function identity for
   `CUME_DIST`, `PERCENT_RANK`, `NTILE`, and donor-equivalent families
3. ordered-set aggregate execution shall preserve direct-argument and
   `WITHIN GROUP` ordering semantics
4. temporal and session-snapshot carriers shall affect plan keys and runtime
   binding deterministically
5. multi-model command verbs shall route through canonical command plans and
   never through parser-local execution shortcuts
