# ScratchBird Optimizer Plan Surface And Donor Render Analysis

## Purpose

This audit identifies the actual ScratchBird optimizer plan contract, the
current ScratchBird plan emission and adapter behavior, and the proven
client-visible plan shapes used by the donor engines targeted for emulation.

The implementation goal that follows from this audit is straightforward:

1. ScratchBird must keep one canonical engine-understood plan contract.
2. Each parser must lower donor explain requests into a render-profile request
   over that canonical plan.
3. Each parser must render the canonical plan into the donor plan surface that
   its client expects.
4. Text scraping should be treated as a compatibility fallback, not the long
   term architecture.

## Evidence Baseline

### ScratchBird

- `include/scratchbird/optimizer/plan_payload.h`
- `src/optimizer/query_planner.cpp`
- `src/sblr/query_compiler_v3_optimizer_support.cpp`
- `src/sblr/executor.cpp`
- `src/parser/v3_emitter.cpp`
- `src/parser/parser_v3.cpp`
- `src/protocol/adapters/mysql_adapter.cpp`
- `src/protocol/adapters/postgresql_adapter.cpp`
- `src/ipc/external_agents/firebird_parser_agent.cpp`

### Donor Engines

The donor authority roots were taken from the existing emulation reference
packets under:

- `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/*/source_authority_matrix.csv`

This audit only makes strong claims where the sampled donor source directly
shows a client-visible explain or plan-output contract.

## ScratchBird: Actual Plan Design

### 1. Canonical plan truth is `optimizer::RuntimePlan`

The engine-side plan contract is not the text returned by `PlanNode::toString()`
and it is not the older executor-side bytecode summarizer. The canonical plan is
the structured `optimizer::RuntimePlan` contract.

What that contract carries:

- contract envelope and versioning:
  - `version`
  - `contract_id`
  - planner, diagnostics, join graph, join search, and rewrite contract ids
- execution-tree surface:
  - `root`
  - `RuntimePlanNode.children`
- relation and access-path surface:
  - `relations`
  - `index_family_signature`
  - `storage_layer_shape`
- join-search surface:
  - `join_steps`
  - `search_summary`
- optimizer proof and diagnostics surface:
  - `proof_surface_json`
  - `diagnostics_payload_json`
  - `considered_paths`
  - `rejected_paths`
  - `statistics_provenance`
  - `adaptive_feedback`
  - `optimizer_controls`
  - `advisor_signals`
  - `advisor_recommendations`
- compatibility and invalidation surface:
  - `plan_hash`
  - `compatibility_version_identifiers`
  - `invalidation_dependencies`
  - `fallback_and_rejection_stream`

This is already large enough to drive donor-specific plan rendering without
inventing parser-private plan structures.

### 2. `RuntimePlanNode` is richer than a generic scan tree

The node shape already contains what a donor renderer needs for most plan
surfaces:

- operator identity: `node_type`
- relation identity: `relation_alias`, `table_path`
- access-path identity: `index_name`
- join or predicate detail: `join_type`, `condition_text`, `detail_text`
- cost model data:
  - `startup_cost`
  - `total_cost`
  - `estimated_rows`
  - `input_estimates`
  - `expanded_cost_terms`
- execution actuals:
  - `actual_rows`
  - `rows_examined`
  - `rows_filtered`
  - `loop_count`
  - `startup_time_us`
  - `execution_time_us`
- parallel data:
  - `parallel_aware`
  - `parallel_enabled`
  - `parallel_workers_planned`
  - `gather_merge`
  - `parallel_stage`
  - `parallel_reason`
- memory and spill data:
  - `estimated_memory_bytes`
  - `memory_budget_bytes`
  - `spill_expected`
  - `spill_passes`
  - `spill_bytes`
  - `spill_policy`
- calibration and governance data:
  - `formula_profile_id`
  - `formula_profile_version`
  - `calibration_profile_id`
  - `storage_profile`
  - `workload_profile`
  - `resource_governance_outcome`

### 3. The optimizer already emits a stable structured payload

`query_planner.cpp` lowers the chosen `PlanNode` tree into `RuntimePlanNode`
instances and then assembles the final `RuntimePlan`.

Observed behavior from current source:

- the planner converts the selected execution tree through
  `toRuntimePlanNode(...)`
- `proof_surface_json` is attached before final hashing
- `search_summary`, `considered_paths`, and `rejected_paths` are copied into the
  runtime plan
- `plan_hash` includes not just the visible text plan, but the wider proof,
  search, and environment signatures

This means the text plan is not the contract. The plan hash proves the planner
already treats the broader structured payload as the authoritative identity.

### 4. `explain_text` is derived, not canonical

The planner sets:

- `runtime_plan.explain_text = current_plan ? current_plan->toString() : "Result"`

That makes `explain_text` a convenience rendering of the chosen tree, not the
full optimizer contract. It is useful for text-oriented donors such as
FirebirdSQL, but it is not rich enough for engines whose clients expect plan
rowsets, JSON objects, or property-rich explain trees.

### 5. ScratchBird already supports multi-render explain output

The `SBLR3_EXPLAIN_PLAN` path in `executor.cpp` reads the structured plan bytes
when they exist and can render:

- line-oriented text
- JSON
- XML
- YAML

That renderer already emits more than a simple operator tree. It can expose:

- `plan_hash`
- plan root and nested children
- per-node actuals
- memory and spill data
- parallel data
- statistics provenance
- considered and rejected paths
- adaptive feedback
- advisor outputs
- proof-surface JSON

The current executor therefore already has the right architectural direction:
render multiple client-facing surfaces from one structured engine plan.

### 6. The older `executeExplainPlan()` path is legacy and lossy

`Executor::executeExplainPlan()` still exists and directly parses bytecode for
`SELECT`, `INSERT`, `UPDATE`, and `DELETE`. It then synthesizes simplified plan
lines such as:

- `Seq Scan on ...`
- `Nested Loop`
- `Sort`
- `Aggregate`
- `Filter`
- `Limit/Offset`

This path also hard-refuses anything outside the four root DML families. That
makes it unsuitable as the long-term plan-conversion authority for donor
parsers.

## ScratchBird: Current Consumer State

### Query compiler and payload transport

`query_compiler_v3_optimizer_support.cpp` already injects:

- `plan`
- `plan_text`
- `plan_hash`

into rewritten SELECT and EXPLAIN payloads.

This is the correct transport boundary for later parser work: the parser does
not need to reconstruct plan state if the engine has already supplied the
encoded `RuntimePlan`.

### MySQL adapter

Current state:

- `mysql_adapter.cpp` rewrites ScratchBird explain results into MySQL EXPLAIN
  rows
- the current implementation expects a one-column `"QUERY PLAN"` rowset
- it text-scrapes plan lines for patterns such as:
  - `relation[...]`
  - `ACCESS_PATH ... verdict=CHOSEN`
  - `table=...`

Interpretation:

- the current MySQL path is a compatibility shim
- it works by parsing a derived text surface
- it should later be replaced by a direct `RuntimePlan` to MySQL-EXPLAIN rowset
  renderer

### Firebird parser agent

Current state:

- `firebird_parser_agent.cpp` extracts `plan_text` from root or nested query
  payload objects
- it uses that text to satisfy `isc_info_sql_get_plan` and
  `isc_info_sql_explain_plan`

Interpretation:

- this is aligned with a text-plan donor surface
- FirebirdSQL is the strongest current case where `plan_text` remains a valid
  first-class emitted surface

### PostgreSQL adapter

Current state:

- `postgresql_adapter.cpp` only proves debug inspection of `plan`,
  `plan_text`, and nested query payloads
- no dedicated PostgreSQL plan conversion layer was proven by the sampled local
  code

Interpretation:

- a PostgreSQL plan renderer still needs to be designed as a proper structured
  conversion path
- the future implementation should consume `RuntimePlan`, not `plan_text`

## Donor Plan Surface Taxonomy

### Structured explain tree families

These donors expose a structured plan tree or property-rich explain model and
are the best fit for direct `RuntimePlan` rendering.

#### PostgreSQL

Current source shows:

- `ExplainState`
- `ExplainBeginOutput`
- `ExplainOpenGroup`
- `ExplainProperty*`
- text vs non-text output branching

Implication:

- PostgreSQL wants a hierarchical explain tree with explicit properties, not a
  flat text dump
- a future PostgreSQL parser should map `EXPLAIN` options to a ScratchBird
  render profile and render `RuntimePlan` into a PostgreSQL-like grouped tree

#### YugabyteDB

Current packet authority points at the PostgreSQL-derived
`src/postgres/src/backend/commands/explain.c`.

Implication:

- treat YugabyteDB as PostgreSQL-family for plan rendering unless later local
  donor work proves additional Yugabyte-specific plan fields

#### Neo4j

Current source proves a tree of execution steps with:

- `getName()`
- `getChildren()`
- `getArguments()`
- `getIdentifiers()`
- profiler statistics such as rows, DB hits, page-cache hits or misses, and
  time

Implication:

- a Neo4j renderer should map `RuntimePlanNode` plus runtime counters into an
  argument-bearing operator tree, not into SQL-style rowsets

#### OpenSearch

Current source proves two separate structured surfaces:

- `ExplainResponse` style explanation trees
- `ProfileResult` trees with:
  - `type`
  - `description`
  - `breakdown`
  - `debug`
  - `time`
  - `children`

Implication:

- OpenSearch requires separate render profiles for explain-vs-profile behavior
- the parser should not assume a single universal plan layout

#### MongoDB

Current source proves:

- execution-stage trees
- stats trees
- `flattenExecTree(...)`
- `flattenStatsTree(...)`
- BSON explain emission through `statsToBSON(...)`
- winner and rejected-plan semantics

Implication:

- MongoDB requires BSON or JSON object rendering from `RuntimePlan`
- a future parser should be able to expose winning-plan and rejected-plan views
  from ScratchBird’s chosen and rejected path data

#### Vitess

Current source proves `PrimitiveDescription`, a serializable primitive tree with
fields such as:

- `OperatorType`
- `Variant`
- `Keyspace`
- `TargetDestination`
- `TargetTabletType`
- `Other`
- `InputName`
- `Inputs`
- row or shard metrics

Implication:

- Vitess wants a JSON-serializable primitive tree, not a rowset explain

#### XTDB

Current source proves structured logical plan assertions in EDN-like forms such
as:

- `[:select ...]`
- `[:project ...]`
- `[:scan ...]`
- `[:apply ...]`

Implication:

- XTDB requires a structural logical-plan renderer
- text-tree rendering is the wrong primary model for this donor

### Tree or text render families

These donors expose a text tree or string plan surface, even if richer internal
plan objects may exist.

#### FirebirdSQL

Current local evidence supports a text-plan surface. The existing ScratchBird
Firebird agent already treats `plan_text` as the relevant compatibility output.

Implication:

- FirebirdSQL is a valid donor for direct text-plan rendering

#### CockroachDB

Current source proves:

- `buildExplainOpt(...)` formats plan or memo text through `treeprinter`
- catalog and memo output may be prepended
- `ConstructExplainOpt(planText.String(), envOpts)` emits the explain result

Implication:

- CockroachDB needs at least a text-tree or memo-text render profile
- if later donor work proves a richer structured output, that can be layered on
  top, but the current local proof is text-oriented

#### Dolt

Current local source proves expected explain-plan strings in
`dolt_query_plans.go`, for example:

- `Filter`
- `IndexedTableAccess(...)`
- nested tree-indented lines

Implication:

- Dolt currently behaves as a text-tree donor surface in the sampled local
  evidence
- since Dolt sits on the MySQL-family SQL ecosystem, later work may still reuse
  some MySQL request lowering, but the rendered plan shape proven here is not
  the MySQL tabular EXPLAIN rowset

#### Apache Ignite

Current source proves:

- parser support for `EXPLAIN` with depth and format tokens including
  `TEXT`, `XML`, `JSON`, and `DOT`
- `PrepareServiceImpl.prepareExplain(...)` builds the plan with
  `RelOptUtil.toString(...)`
- `ExplainPlan` stores a single `plan` string
- execution returns that string in one `PLAN` column

Implication:

- the currently proven client-visible output is a single text plan cell
- any future JSON or XML compatibility claim for Ignite must be tied to a later
  execution-side proof, not just parser grammar support

### Rowset or tabular explain families

These donors expose row-oriented explain results, usually backed by a richer
internal tree.

#### MySQL

Current source proves:

- traditional EXPLAIN row buffering through `qep_row`
- traditional columns such as:
  - `id`
  - `select_type`
  - `table`
  - `partitions`
  - `type`
  - `possible_keys`
  - `key`
  - `key_len`
  - `ref`
  - `rows`
  - `filtered`
  - `Extra`
- an internal distinction between traditional and hierarchical explain handling
- access-type vocabulary such as:
  - `system`
  - `const`
  - `eq_ref`
  - `ref`
  - `ALL`
  - `range`
  - `index`
  - `fulltext`
  - `ref_or_null`
  - `index_merge`

Implication:

- the correct ScratchBird renderer for MySQL is not textual scraping
- it is a structured `RuntimePlan` to MySQL-rowset conversion

#### MariaDB

Current packet authority points at `sql/sql_explain.cc`, placing it in the same
rowset or hierarchical explain family as MySQL.

Implication:

- MariaDB should be treated as a MySQL-family explain renderer unless later
  donor work proves MariaDB-specific columns or variants

#### TiDB

Current source proves:

- a binary protobuf plan format
- decoding into rows
- multiple output formats:
  - `Brief`
  - `ROW`
  - `PlanTree`
  - `Verbose`
- row headers such as:
  - `id`
  - `estRows`
  - `estCost`
  - `actRows`
  - `task`
  - `access object`
  - `execution info`
  - `operator info`
  - `memory`
  - `disk`

Implication:

- TiDB is a strong match for rendering `RuntimePlan` plus per-node actuals into
  row-oriented plan tables

### Multi-profile explain families

These donors support several explain surfaces or plan introspection modes from
the same base planning state.

#### ClickHouse

Current source proves multiple explain kinds and option groups, including:

- syntax or AST views
- query tree views
- query plan views
- query pipeline views
- query estimates
- options such as:
  - `header`
  - `description`
  - `actions`
  - `indexes`
  - `json`
  - `sorting`
  - `distributed`
  - `input_headers`
  - `column_structure`

Implication:

- ClickHouse parser work must model explain as a profile family, not a single
  flag

#### DuckDB

Current source proves:

- `ExplainType`:
  - `EXPLAIN_STANDARD`
  - `EXPLAIN_ANALYZE`
- `ExplainFormat`:
  - `DEFAULT`
  - `TEXT`
  - `JSON`
  - `HTML`
  - `GRAPHVIZ`
  - `YAML`
  - `MERMAID`

Implication:

- DuckDB also requires render-profile selection over one canonical plan

## Engines With Internal Plan Evidence But No Stable Client Surface Proved Here

The following donors do show internal planning objects, planner directories, or
plan-related code in the local source tree, but the sampled evidence does not
yet justify a strong claim about a stable client-visible explain contract:

- Cassandra
- Citus
- FoundationDB
- immudb
- InfluxDB
- Milvus
- Redis

What is currently proven for some of them:

- Citus: distributed planner internals exist, but this audit did not sample a
  separate client-facing explain renderer beyond the PostgreSQL-family context
- InfluxDB: internal logical and physical plan creation exists through the
  query planner and DataFusion-style execution plans
- Milvus: serialized internal query plans are created for search and delete
  paths
- FoundationDB, Redis, immudb, Cassandra: the sampled local source did not
  prove a standalone client-visible explain-plan contract

Parser implication:

- these engines require a targeted follow-up reference packet if byte-for-byte
  explain emulation is mandatory
- until then, no canonical parser spec should overclaim a proven donor plan
  layout for them

## Explicit Evidence Gap

### SQLite

The existing reference packet already marks SQLite plan-output donor files as
missing in the local reference tree. This audit preserves that gap.

Parser implication:

- no SQLite plan-layout spec should be written from guesswork

## Design Rules For Future Parser Specs

### Rule 1: `RuntimePlan` is the only engine-plan authority

Future parser specifications should treat:

- `plan` bytes as canonical
- `plan_text` as a derived rendering
- the older `executeExplainPlan()` path as legacy compatibility behavior only

### Rule 2: Donor explain requests should lower to render profiles

The reverse mapping is not “parse donor plan text back into the engine.”
Instead, it should be:

1. donor `EXPLAIN` request arrives
2. parser lowers donor options to a ScratchBird explain render profile
3. engine executes and returns `RuntimePlan`
4. parser renders donor-compatible output from `RuntimePlan`

### Rule 3: Donor render profiles should be explicit

At minimum, later parser specs will need render profiles for:

- `TREE_TEXT`
- `TREE_TEXT_WITH_PROPERTIES`
- `ROWSET_EXPLAIN`
- `JSON_OBJECT_TREE`
- `BSON_OBJECT_TREE`
- `XML_TREE`
- `YAML_TREE`
- `GRAPH_OR_DIAGRAM`
- `LOGICAL_FORM_TREE`
- `SINGLE_TEXT_CELL`

### Rule 4: Rejected-path data should be preserved where donors expose it

ScratchBird already has:

- `considered_paths`
- `rejected_paths`

That is directly relevant for donors such as MongoDB and optimizer-oriented
surfaces that distinguish chosen from rejected alternatives.

### Rule 5: Per-node actuals must stay first-class

ScratchBird already has:

- `actual_rows`
- `rows_examined`
- `rows_filtered`
- `loop_count`
- timing
- memory
- spill

That makes it possible to satisfy analyze or profile style donors such as TiDB,
Neo4j, DuckDB, OpenSearch, and others without inventing a second runtime-plan
model.

### Rule 6: Text scraping should be retired where richer donor surfaces exist

Current MySQL behavior is functional but lossy. The long-term design should
replace text scraping with direct structured rendering from `RuntimePlan`.

## Audit Conclusion

ScratchBird already has the correct base architecture for donor-compatible plan
emulation:

- one structured canonical plan contract
- transport of that contract in rewritten payloads
- executor-side multi-format rendering

What remains is donor-specific rendering and request-lowering work, not a new
engine plan model.

The only plan surface that should continue to depend primarily on derived text
is the text-first donor family exemplified here by FirebirdSQL and other
text-tree donors. All richer donors should render from `RuntimePlan` directly.
