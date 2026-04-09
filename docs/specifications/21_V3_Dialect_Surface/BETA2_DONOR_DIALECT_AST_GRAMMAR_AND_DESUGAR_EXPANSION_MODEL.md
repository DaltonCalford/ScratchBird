Status: current_authority_beta2

# Beta 2 Donor Dialect AST Grammar and Desugar Expansion Model

## Purpose

Define the Beta 2 native-v3 grammar, canonical AST, and desugar expansions
required so the donor engines targeted for emulation can map to shared
ScratchBird structures instead of donor-private parser or executor branches.

## Scope

This file owns:

1. native-v3 admission of donor-shared syntax that is currently refused
2. new AST carriers for select, table-source, function-call, session, and
   policy or admin surfaces found in the donor audit
3. canonical desugar rules that map donor spellings into shared nodes
4. donor-identity preservation rules needed by reverse rendering, error
   shaping, plan shaping, and emulated-parser mapping

This file does not own:

1. SBLR container payload schemas; see section `22`
2. planner and executor staging; see section `23`
3. per-engine parser package wiring and coverage rollout; see section `28`

## Hard invariants

1. Native v3 shall not hard-refuse a donor-shared construct once this file
   defines a canonical AST landing zone for it.
2. Donor parsers shall map to the shared AST defined here and shall not invent
   donor-private AST node families.
3. The AST shall preserve donor-visible semantics explicitly. Argument mode,
   null-treatment, time-slice flavor, and select-pipeline stage order shall not
   be reconstructed later by guesswork.
4. Donor spellings that lower to existing semantics shall keep origin metadata
   so reverse rendering, explain rendering, and error shaping can reproduce the
   donor-facing surface when required.
5. Parser admission may normalize syntax, but it may not weaken semantics. A
   donor-visible clause that changes conflict handling, temporal scope, or row
   production must survive in structured form.
6. SQLite contributes no SQLite-unique Beta 2 backlog from this file until a
   local donor clone exists. Shared SQL carriers admitted here remain valid for
   later SQLite mapping.

## Beta 2 donor justification buckets

- `APPLY`, `QUALIFY`, `PIVOT`, `UNPIVOT`, and `MATCH_RECOGNIZE` are required by
  Apache Ignite. `QUALIFY` is also required by ClickHouse, DuckDB, MySQL, and
  Vitess. `PIVOT` and `UNPIVOT` are also required by DuckDB.
- `JSON_TABLE` is required by PostgreSQL, MySQL, MariaDB, and Vitess.
- `ROWS FROM (...)` is required by PostgreSQL and inherited PostgreSQL-family
  emulation lanes such as Citus and YugabyteDB.
- temporal clauses are required by CockroachDB, Dolt, TiDB, and XTDB via
  `AS OF`, `AS OF SYSTEM TIME`, `AS OF TIMESTAMP`, `FOR VALID_TIME`, and
  `FOR SYSTEM_TIME`.
- ClickHouse requires `PREWHERE`, `ARRAY JOIN`, `LIMIT BY`, `FINAL`, and
  statement-local `SETTINGS`.
- PostgreSQL, FirebirdSQL, MariaDB, DuckDB, and Apache Ignite require ordered-
  set aggregate support with `WITHIN GROUP`.
- PostgreSQL, DuckDB, and PostgreSQL-lineage donors require variadic and named
  function-call arguments.
- MySQL, TiDB, Vitess, FirebirdSQL, and DuckDB require window-function metadata
  beyond the current whitelist, including `IGNORE NULLS`, `RESPECT NULLS`, and
  `FROM FIRST` or `FROM LAST`.
- Cassandra, CockroachDB, TiDB, and YugabyteDB require reusable distribution or
  policy DDL carriers for keyspace, locality, placement, range, split-region,
  tablet, tablegroup, and co-location surfaces.
- immudb requires session-scope database and snapshot carriers plus
  `HISTORY OF`.
- InfluxDB requires metadata show-surface carriers for `SHOW MEASUREMENTS`,
  `SHOW FIELD KEYS`, `SHOW TAG KEYS`, and `SHOW TAG VALUES`.
- MongoDB, Redis, Milvus, OpenSearch, FoundationDB, Cassandra, and Neo4j
  require canonical multi-model command carriers or exposure of already-existing
  command families.

## Canonical AST expansion

### 1. Function-call metadata

The current positional-only function-call model shall be replaced by a
structured-argument model. The old `arguments` vector may remain as a temporary
compatibility mirror for positional calls only, but Beta 2 authority is the
structured carrier below.

```cpp
enum class FunctionArgumentMode : uint8_t {
  POSITIONAL = 0,
  NAMED = 1,
  VARIADIC = 2,
};

enum class WindowNullTreatment : uint8_t {
  DEFAULT_BEHAVIOR = 0,
  RESPECT_NULLS = 1,
  IGNORE_NULLS = 2,
};

enum class NthValueDirection : uint8_t {
  UNSPECIFIED = 0,
  FROM_FIRST = 1,
  FROM_LAST = 2,
};

struct FunctionArgumentItem {
  FunctionArgumentMode mode = FunctionArgumentMode::POSITIONAL;
  StringPool::StringId name = StringPool::INVALID_ID;
  Expression* value = nullptr;
};

struct OrderedSetAggregateSpec {
  bool present = false;
  std::vector<Expression*> direct_args;
  std::vector<OrderByItem*> within_group_order;
};

struct WindowBehaviorSpec {
  WindowNullTreatment null_treatment = WindowNullTreatment::DEFAULT_BEHAVIOR;
  NthValueDirection nth_direction = NthValueDirection::UNSPECIFIED;
};

class FunctionCallExpr : public Expression {
public:
  SchemaPath function_path;
  std::vector<FunctionArgumentItem> argument_items;
  bool distinct = false;
  Expression* filter = nullptr;
  std::vector<OrderByItem*> order_by;
  OrderedSetAggregateSpec ordered_set;
  bool is_window = false;
  WindowSpec* window = nullptr;
  WindowBehaviorSpec window_behavior;
  bool preserve_function_identity = false;
  StringPool::StringId donor_surface_name = StringPool::INVALID_ID;
};
```

Rules:

1. `WITHIN GROUP` shall populate `ordered_set.present = true`.
2. Named arguments shall populate `FunctionArgumentItem.name`.
3. `VARIADIC` shall be carried as `FunctionArgumentMode::VARIADIC`; it shall
   not be rewritten into an array literal.
4. Window calls shall preserve the function symbol and donor-facing spelling
   whenever the donor surface expects specific result or explain rendering.
5. `IGNORE NULLS`, `RESPECT NULLS`, `FROM FIRST`, and `FROM LAST` shall survive
   parsing as structured metadata and shall not be reduced to opaque flags.

### 2. Temporal clause carrier

```cpp
enum class TemporalClauseKind : uint8_t {
  NONE = 0,
  AS_OF,
  AS_OF_SYSTEM_TIME,
  AS_OF_TIMESTAMP,
  FOR_VALID_TIME,
  FOR_SYSTEM_TIME,
  SNAPSHOT_SINCE,
  SNAPSHOT_AFTER,
  SNAPSHOT_BEFORE,
  SNAPSHOT_UNTIL,
  SNAPSHOT_TX,
};

struct TemporalClause {
  TemporalClauseKind kind = TemporalClauseKind::NONE;
  Expression* anchor = nullptr;
  StringPool::StringId named_scope = StringPool::INVALID_ID;
  bool applies_to_table = false;
  bool applies_to_statement = false;
  bool applies_to_call = false;
  bool applies_to_show = false;
};
```

Rules:

1. The same carrier shall be reusable on table references, `SELECT`, `SHOW`,
   and `CALL` surfaces.
2. `AS OF SYSTEM TIME` and `AS OF TIMESTAMP` shall not collapse into a generic
   timestamp literal flag. The clause flavor is semantically significant.
3. immudb snapshot surfaces shall use the same carrier family, but the owning
   statement shall mark it as session-scope instead of table-scope.

### 3. Table-source expansion

```cpp
struct JsonTableColumnSpec;
struct PivotSpec;
struct UnpivotSpec;
struct MatchRecognizeSpec;
struct RowsFromSpec;

struct QuerySettingItem {
  StringPool::StringId key = StringPool::INVALID_ID;
  Expression* value = nullptr;
};

struct ArrayJoinItem {
  Expression* expr = nullptr;
  StringPool::StringId alias = StringPool::INVALID_ID;
  bool left = false;
};

struct LimitBySpec {
  std::vector<Expression*> partition_keys;
  Expression* limit = nullptr;
  Expression* offset = nullptr;
};

struct JsonTableColumnSpec {
  enum class Kind : uint8_t {
    VALUE = 0,
    EXISTS = 1,
    ORDINALITY = 2,
    NESTED = 3,
  };

  Kind kind = Kind::VALUE;
  StringPool::StringId column_name = StringPool::INVALID_ID;
  TypeName target_type;
  Expression* path_expr = nullptr;
  std::vector<JsonTableColumnSpec> nested_columns;
  StringPool::StringId on_empty = StringPool::INVALID_ID;
  StringPool::StringId on_error = StringPool::INVALID_ID;
};

struct JsonTableSpec {
  Expression* source_expr = nullptr;
  Expression* row_path = nullptr;
  std::vector<JsonTableColumnSpec> columns;
};

struct RowsFromItem {
  FunctionCallExpr* function = nullptr;
  std::vector<StringPool::StringId> column_aliases;
};

struct RowsFromSpec {
  std::vector<RowsFromItem> items;
  bool with_ordinality = false;
};

struct PivotMeasure {
  FunctionCallExpr* aggregate = nullptr;
  StringPool::StringId alias = StringPool::INVALID_ID;
};

struct PivotSpec {
  std::vector<Expression*> pivot_keys;
  std::vector<PivotMeasure> measures;
  std::vector<Expression*> pivot_values;
};

struct UnpivotSpec {
  std::vector<StringPool::StringId> value_columns;
  StringPool::StringId name_column = StringPool::INVALID_ID;
  bool include_nulls = false;
};

struct MatchRecognizeSpec {
  std::vector<Expression*> partition_by;
  std::vector<OrderByItem*> order_by;
  std::vector<SelectItem*> measures;
  Expression* pattern = nullptr;
  std::vector<std::pair<StringPool::StringId, Expression*>> defines;
  StringPool::StringId rows_per_match = StringPool::INVALID_ID;
  StringPool::StringId after_match_skip = StringPool::INVALID_ID;
};

struct TableRefNode : public ASTNode {
  enum class Type : uint8_t {
    TABLE = 0,
    SUBQUERY = 1,
    FUNCTION = 2,
    JOIN = 3,
    JSON_TABLE = 4,
    ROWS_FROM = 5,
    PIVOT = 6,
    UNPIVOT = 7,
    MATCH_RECOGNIZE = 8,
  };

  Type ref_type = Type::TABLE;
  SchemaPath table_path;
  Statement* subquery = nullptr;
  FunctionCallExpr* function = nullptr;
  JsonTableSpec* json_table = nullptr;
  RowsFromSpec* rows_from = nullptr;
  PivotSpec* pivot = nullptr;
  UnpivotSpec* unpivot = nullptr;
  MatchRecognizeSpec* match_recognize = nullptr;
  TemporalClause* temporal = nullptr;
  bool final = false;
  StringPool::StringId alias = StringPool::INVALID_ID;
  bool has_alias = false;
  bool lateral = false;
  bool with_ordinality = false;
  TableSampleMethod sample_method = TableSampleMethod::NONE;
  Expression* sample_percent = nullptr;
  Expression* sample_repeatable_seed = nullptr;
  std::vector<StringPool::StringId> column_aliases;
};

class SelectStmt : public Statement {
public:
  WithClause* with = nullptr;
  bool distinct = false;
  bool all = false;
  std::vector<Expression*> distinct_on;
  std::vector<SelectItem*> items;
  TableRefNode* from = nullptr;
  std::vector<JoinNode*> joins;
  Expression* prewhere = nullptr;
  Expression* where = nullptr;
  std::vector<Expression*> group_by;
  GroupingType grouping_type = GroupingType::STANDARD;
  std::vector<std::vector<Expression*>> grouping_sets;
  Expression* having = nullptr;
  std::vector<std::pair<StringPool::StringId, WindowSpec*>> windows;
  Expression* qualify = nullptr;
  std::vector<ArrayJoinItem> array_join_items;
  std::vector<OrderByItem*> order_by;
  LimitBySpec* limit_by = nullptr;
  std::vector<QuerySettingItem> query_settings;
  Expression* limit = nullptr;
  Expression* offset = nullptr;
  FetchMode fetch_mode = FetchMode::NONE;
  bool fetch_with_ties = false;
  Expression* fetch_row_count = nullptr;
  SetOpType set_op = SetOpType::NONE;
  bool set_op_all = false;
  SelectStmt* set_op_right = nullptr;
  SelectLockStrength lock_strength = SelectLockStrength::NONE;
  bool for_update = false;
  bool for_share = false;
  bool nowait = false;
  bool skip_locked = false;
  bool with_lock = false;
  Expression* optimize_for_rows = nullptr;
  Expression* firebird_plan = nullptr;
  TemporalClause* temporal = nullptr;
};
```

Rules:

1. `QUALIFY` is a post-window filter and shall live beside `HAVING`, not as a
   disguised `WHERE`.
2. `PREWHERE` is an early filter and shall not be merged into `WHERE`.
3. `ARRAY JOIN`, `LIMIT BY`, and statement-local `SETTINGS` shall keep distinct
   structured carriers so the planner can preserve donor stage order.
4. `FINAL` is a table-source modifier and shall remain attached to the owning
   `TableRefNode`.
5. `JSON_TABLE`, `ROWS FROM`, `PIVOT`, `UNPIVOT`, and `MATCH_RECOGNIZE` shall
   be represented as first-class table-source kinds, not as anonymous function
   calls with magic names.

### 4. DML alias and conflict-surface admission

```cpp
enum class InsertSurfaceFlavor : uint8_t {
  STANDARD = 0,
  MYSQL_REPLACE = 1,
  MYSQL_INSERT_IGNORE = 2,
  MYSQL_ON_DUPLICATE_KEY_UPDATE = 3,
  COCKROACH_UPSERT = 4,
  IMMUDB_UPSERT = 5,
};

class InsertStmt : public Statement {
public:
  InsertSurfaceFlavor surface_flavor = InsertSurfaceFlavor::STANDARD;
  bool ignore_errors = false;
  OnConflictClause* on_conflict = nullptr;
  // existing fields remain
};
```

Rules:

1. `REPLACE INTO` shall not be a separate statement class. It shall lower to
   `InsertStmt` with `surface_flavor = MYSQL_REPLACE`.
2. `INSERT IGNORE` shall lower to `InsertSurfaceFlavor::MYSQL_INSERT_IGNORE`.
3. `ON DUPLICATE KEY UPDATE` shall use the existing conflict carrier, but the
   surface flavor shall remain explicit.
4. `UPSERT` and `UPSERT INTO` shall lower to `InsertStmt` with the appropriate
   flavor and a populated canonical conflict action.

### 5. Session, history, metadata, and policy surfaces

```cpp
class UseDatabaseStmt : public Statement {
public:
  SchemaPath database_path;
};

class SetSnapshotScopeStmt : public Statement {
public:
  TemporalClause snapshot;
};

class HistoryQueryStmt : public Statement {
public:
  SchemaPath object_path;
  TemporalClause temporal;
  Statement* source_query = nullptr;
};

enum class MetadataShowSurface : uint8_t {
  SHOW_MEASUREMENTS = 0,
  SHOW_FIELD_KEYS = 1,
  SHOW_TAG_KEYS = 2,
  SHOW_TAG_VALUES = 3,
};

class ShowMetadataSurfaceStmt : public Statement {
public:
  MetadataShowSurface surface = MetadataShowSurface::SHOW_MEASUREMENTS;
  SchemaPath namespace_path;
  Expression* where = nullptr;
  TemporalClause* temporal = nullptr;
};

enum class PolicySurfaceKind : uint8_t {
  KEYSPACE = 0,
  LOCALITY = 1,
  PLACEMENT = 2,
  RANGE = 3,
  CHANGEFEED = 4,
  SCRUB = 5,
  TABLEGROUP = 6,
  TABLETS = 7,
  COLOCATION = 8,
  SPLIT = 9,
};

struct PolicyOptionItem {
  StringPool::StringId key = StringPool::INVALID_ID;
  Expression* value = nullptr;
};

class PolicySurfaceStmt : public Statement {
public:
  PolicySurfaceKind kind = PolicySurfaceKind::KEYSPACE;
  SchemaPath target_path;
  StringPool::StringId action = StringPool::INVALID_ID;
  std::vector<PolicyOptionItem> options;
};
```

Rules:

1. Cassandra keyspace surfaces, Cockroach locality or placement surfaces,
   TiDB placement or split surfaces, and Yugabyte tablegroup or tablet surfaces
   shall all lower into reusable policy carriers instead of donor-private AST.
2. immudb snapshot selection shall use `SetSnapshotScopeStmt`, while
   table-scope `AS OF` or `FOR VALID_TIME` shall use `TemporalClause` on the
   relevant table or statement node.
3. InfluxQL `SHOW` surfaces shall not be hidden inside generic identifier
   lookups; they are first-class metadata statements.

### 6. Multi-model command carrier

```cpp
enum class MultiModelFamily : uint8_t {
  CQL = 0,
  MONGO = 1,
  CYPHER = 2,
  REDIS = 3,
  MILVUS = 4,
  OPENSEARCH = 5,
  FOUNDATIONDB = 6,
};

struct NamedExpressionItem {
  StringPool::StringId name = StringPool::INVALID_ID;
  Expression* value = nullptr;
};

class MultiModelCommandStmt : public Statement {
public:
  MultiModelFamily family = MultiModelFamily::CQL;
  StringPool::StringId verb = StringPool::INVALID_ID;
  SchemaPath target_path;
  std::vector<NamedExpressionItem> named_args;
  std::vector<Expression*> positional_args;
  Statement* embedded_query = nullptr;
  TemporalClause* temporal = nullptr;
};
```

Rules:

1. Existing Redis, CQL, Mongo, Cypher, and Milvus parser exposure gaps shall
   land on `MultiModelCommandStmt`.
2. New command families for OpenSearch and FoundationDB shall also land on
   `MultiModelCommandStmt`.
3. The family and verb shall be structured enum or symbol data, not raw free-
   form text consumed later by heuristics.

## Native-v3 grammar obligations

The native parser shall admit the following surfaces in Beta 2:

1. `CROSS APPLY` and `OUTER APPLY`, lowered to lateral joins
2. `CREATE MATERIALIZED VIEW`, lowered to `CreateViewStmt.materialized = true`
3. `REPLACE INTO`, `INSERT IGNORE`, `ON DUPLICATE KEY UPDATE`, `UPSERT`, and
   `UPSERT INTO`, lowered to `InsertStmt`
4. `QUALIFY`
5. `JSON_TABLE`
6. `PIVOT`
7. `UNPIVOT`
8. `MATCH_RECOGNIZE`
9. `ROWS FROM (...)`
10. `AS OF`, `AS OF SYSTEM TIME`, `AS OF TIMESTAMP`, `FOR VALID_TIME`,
    `FOR SYSTEM_TIME`
11. `PREWHERE`, `ARRAY JOIN`, `LIMIT BY`, `FINAL`, and statement-local
    `SETTINGS`
12. `WITHIN GROUP`
13. named-argument and `VARIADIC` call syntax
14. `IGNORE NULLS`, `RESPECT NULLS`, `FROM FIRST`, and `FROM LAST`
15. `USE DATABASE`, `USE SNAPSHOT`, and `HISTORY OF`
16. InfluxQL metadata show surfaces
17. canonical front doors for `CQL`, `MONGO`, `CYPHER`, `REDIS`, `MILVUS`,
    `OPENSEARCH`, and `FOUNDATIONDB`

## Canonical desugar rules

1. `CROSS APPLY rhs` lowers to `INNER JOIN LATERAL rhs ON TRUE`.
2. `OUTER APPLY rhs` lowers to `LEFT JOIN LATERAL rhs ON TRUE`.
3. `CREATE MATERIALIZED VIEW` lowers to the existing `CreateViewStmt` with
   `materialized = true`.
4. `REPLACE INTO` lowers to `InsertStmt` with
   `surface_flavor = MYSQL_REPLACE`.
5. `UPSERT` lowers to `InsertStmt` with the owning donor flavor retained.
6. Donor procedure families already expressible as `CALL`,
   `EXECUTE PROCEDURE`, or `FunctionCallExpr` shall not create new AST classes.

## Sample canonical builder snippets

```cpp
JoinNode* buildApplyJoin(TableRefNode* left, TableRefNode* right, bool outer) {
  right->lateral = true;

  auto* join = arena.make<JoinNode>();
  join->join_type = outer ? JoinType::LEFT : JoinType::INNER;
  join->left = left;
  join->right = right;
  join->on_condition = buildBooleanLiteral(true);
  return join;
}

InsertStmt* buildReplaceInto(const ParsedReplaceInto& parsed) {
  auto* stmt = arena.make<InsertStmt>();
  stmt->surface_flavor = InsertSurfaceFlavor::MYSQL_REPLACE;
  stmt->table_path = parsed.table_path;
  stmt->columns = parsed.columns;
  stmt->values_rows = parsed.rows;
  return stmt;
}
```

## Required proof

1. native v3 shall stop producing hard-refusal errors for every surface listed
   in this file
2. AST round-trip shall preserve donor-facing modifiers, argument modes,
   temporal kinds, and select-stage clauses
3. donor parser packages shall be able to lower their owning feature buckets
   into these shared nodes without donor-private AST extensions
4. unsupported donor-only surfaces that are not admitted here shall fail closed
   with explicit capability errors rather than parser crashes or silent rewrites
