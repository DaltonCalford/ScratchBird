Status: current_authority_beta2

# Beta 2 Donor Dialect SBLR Payload and Opcode Expansion Model

## Purpose

Define the canonical SBLR payload extensions and new command-envelope families
required by the Beta 2 donor dialect expansion so every new parser surface can
be emitted into stable, verifiable, engine-facing structures.

## Hard invariants

1. The parser-to-engine contract remains SBLR. No donor parser may bypass SBLR
   by handing private runtime requests directly to the engine.
2. Existing statement payloads shall be extended where the current statement
   class remains valid. New top-level opcodes are reserved for genuinely new
   statement or command families only.
3. Donor-visible semantics shall be preserved structurally. SBLR shall not rely
   on magic strings or parser-private side channels to reconstruct ordered-set,
   temporal, or window behavior.
4. Verifier rules shall reject partially populated Beta 2 payloads.
5. Donor origin metadata is rendering and mapping data only. It shall not alter
   core engine semantics once the canonical payload is formed.

## Statement-payload extensions

### 1. Function-call payload

```cpp
enum class SblrFunctionArgumentMode : uint8_t {
  POSITIONAL = 0,
  NAMED = 1,
  VARIADIC = 2,
};

struct SblrFunctionArgItem {
  SblrFunctionArgumentMode mode = SblrFunctionArgumentMode::POSITIONAL;
  SymbolId name_symbol = INVALID_SYMBOL_ID;
  PoolRef value_expr = INVALID_POOL_REF;
};

struct SblrOrderedSetPayload {
  bool present = false;
  VectorRef direct_arg_exprs = INVALID_VECTOR_REF;
  VectorRef within_group_order_items = INVALID_VECTOR_REF;
};

struct SblrWindowBehaviorPayload {
  WindowNullTreatment null_treatment = WindowNullTreatment::DEFAULT_BEHAVIOR;
  NthValueDirection nth_direction = NthValueDirection::UNSPECIFIED;
};

struct SblrFunctionCallPayloadV2 {
  SymbolId canonical_function_symbol = INVALID_SYMBOL_ID;
  SymbolId donor_surface_symbol = INVALID_SYMBOL_ID;
  VectorRef argument_items = INVALID_VECTOR_REF;
  bool distinct = false;
  PoolRef filter_expr = INVALID_POOL_REF;
  VectorRef aggregate_order_items = INVALID_VECTOR_REF;
  SblrOrderedSetPayload ordered_set;
  bool is_window = false;
  PoolRef window_spec_ref = INVALID_POOL_REF;
  SblrWindowBehaviorPayload window_behavior;
};
```

Rules:

1. Generic window calls shall retain the actual function identity. The fallback
   that collapses unknown window calls to `ROW_NUMBER` is forbidden after Beta
   2 adoption.
2. Ordered-set aggregates shall populate both `direct_arg_exprs` and
   `within_group_order_items`.
3. Named arguments shall preserve the symbol id for the argument name.
4. Variadic calls shall preserve argument-mode identity. They shall not be
   transformed into positional-only arrays inside SBLR.

### 2. Select-statement payload

```cpp
struct SblrArrayJoinItem {
  PoolRef expr = INVALID_POOL_REF;
  SymbolId alias_symbol = INVALID_SYMBOL_ID;
  bool left = false;
};

struct SblrLimitByPayload {
  VectorRef partition_keys = INVALID_VECTOR_REF;
  PoolRef limit_expr = INVALID_POOL_REF;
  PoolRef offset_expr = INVALID_POOL_REF;
};

struct SblrQuerySettingItem {
  SymbolId key_symbol = INVALID_SYMBOL_ID;
  PoolRef value_expr = INVALID_POOL_REF;
};

struct SblrTemporalBindingPayload {
  TemporalClauseKind kind = TemporalClauseKind::NONE;
  PoolRef anchor_expr = INVALID_POOL_REF;
  SymbolId named_scope_symbol = INVALID_SYMBOL_ID;
  bool applies_to_table = false;
  bool applies_to_statement = false;
  bool applies_to_call = false;
  bool applies_to_show = false;
};

struct SblrSelectExtensionPayload {
  PoolRef prewhere_expr = INVALID_POOL_REF;
  PoolRef qualify_expr = INVALID_POOL_REF;
  VectorRef array_join_items = INVALID_VECTOR_REF;
  PoolRef limit_by_payload = INVALID_POOL_REF;
  VectorRef query_settings = INVALID_VECTOR_REF;
  PoolRef temporal_binding = INVALID_POOL_REF;
};
```

Rules:

1. `PREWHERE` and `QUALIFY` shall live in dedicated slots and shall not be
   folded into `WHERE` or `HAVING`.
2. `LIMIT BY` is not equivalent to a normal limit and shall therefore have its
   own payload block.
3. Temporal binding is a reusable payload family and shall be referenceable by
   select, table, call, history, and show surfaces.

### 3. Table-source union additions

```cpp
enum class SblrTableSourceKind : uint8_t {
  BASE_TABLE = 0,
  SUBQUERY = 1,
  FUNCTION = 2,
  JSON_TABLE = 3,
  ROWS_FROM = 4,
  PIVOT = 5,
  UNPIVOT = 6,
  MATCH_RECOGNIZE = 7,
};

struct SblrJsonTableColumnPayload {
  uint8_t kind = 0;
  SymbolId column_name_symbol = INVALID_SYMBOL_ID;
  PoolRef target_type_ref = INVALID_POOL_REF;
  PoolRef path_expr = INVALID_POOL_REF;
  VectorRef nested_columns = INVALID_VECTOR_REF;
  SymbolId on_empty_symbol = INVALID_SYMBOL_ID;
  SymbolId on_error_symbol = INVALID_SYMBOL_ID;
};

struct SblrJsonTablePayload {
  PoolRef source_expr = INVALID_POOL_REF;
  PoolRef row_path_expr = INVALID_POOL_REF;
  VectorRef columns = INVALID_VECTOR_REF;
};

struct SblrRowsFromPayload {
  VectorRef function_items = INVALID_VECTOR_REF;
  bool with_ordinality = false;
};

struct SblrPivotPayload {
  VectorRef pivot_keys = INVALID_VECTOR_REF;
  VectorRef measures = INVALID_VECTOR_REF;
  VectorRef pivot_values = INVALID_VECTOR_REF;
};

struct SblrUnpivotPayload {
  VectorRef value_columns = INVALID_VECTOR_REF;
  SymbolId name_column_symbol = INVALID_SYMBOL_ID;
  bool include_nulls = false;
};

struct SblrMatchRecognizePayload {
  VectorRef partition_by = INVALID_VECTOR_REF;
  VectorRef order_by = INVALID_VECTOR_REF;
  VectorRef measures = INVALID_VECTOR_REF;
  PoolRef pattern_expr = INVALID_POOL_REF;
  VectorRef define_pairs = INVALID_VECTOR_REF;
  SymbolId rows_per_match_symbol = INVALID_SYMBOL_ID;
  SymbolId after_match_skip_symbol = INVALID_SYMBOL_ID;
};

struct SblrTableSourcePayloadV2 {
  SblrTableSourceKind kind = SblrTableSourceKind::BASE_TABLE;
  PoolRef payload_ref = INVALID_POOL_REF;
  PoolRef temporal_binding = INVALID_POOL_REF;
  bool lateral = false;
  bool with_ordinality = false;
  bool final = false;
  SymbolId alias_symbol = INVALID_SYMBOL_ID;
  VectorRef column_aliases = INVALID_VECTOR_REF;
};
```

Rules:

1. Each new table-source kind shall be a first-class discriminated union arm.
2. `FINAL` shall remain attached to the owning table source payload.
3. Table-scope temporal binding shall be carried beside the table-source payload
   and shall not be hidden inside expression options.

### 4. DML alias payload additions

```cpp
enum class SblrInsertSurfaceFlavor : uint8_t {
  STANDARD = 0,
  MYSQL_REPLACE = 1,
  MYSQL_INSERT_IGNORE = 2,
  MYSQL_ON_DUPLICATE_KEY_UPDATE = 3,
  COCKROACH_UPSERT = 4,
  IMMUDB_UPSERT = 5,
};

struct SblrInsertPayloadV2 {
  SblrInsertSurfaceFlavor surface_flavor = SblrInsertSurfaceFlavor::STANDARD;
  PoolRef on_conflict_payload = INVALID_POOL_REF;
  bool ignore_errors = false;
  // existing insert fields remain
};
```

### 5. Session, history, metadata, and policy payloads

```cpp
enum class SblrSessionScopeKind : uint8_t {
  USE_DATABASE = 0,
  SET_SNAPSHOT_SCOPE = 1,
  HISTORY_QUERY = 2,
};

enum class SblrMetadataShowSurface : uint8_t {
  SHOW_MEASUREMENTS = 0,
  SHOW_FIELD_KEYS = 1,
  SHOW_TAG_KEYS = 2,
  SHOW_TAG_VALUES = 3,
};

enum class SblrPolicySurfaceKind : uint8_t {
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

struct SblrPolicyOptionItem {
  SymbolId key_symbol = INVALID_SYMBOL_ID;
  PoolRef value_expr = INVALID_POOL_REF;
};
```

Rules:

1. These surfaces shall use dedicated statement payload families and shall not
   be tunneled through free-form administrative text.
2. Snapshot and history payloads shall reuse `SblrTemporalBindingPayload`.
3. Policy DDL payloads shall use structured option items so the verifier can
   reject malformed mixes deterministically.

## Multi-model command envelope expansion

The canonical command-envelope family shall be expanded for both currently
exposed operations and genuinely new donor operations.

```cpp
enum class SblrMultiModelFamily : uint8_t {
  CQL = 0,
  MONGO = 1,
  CYPHER = 2,
  REDIS = 3,
  MILVUS = 4,
  OPENSEARCH = 5,
  FOUNDATIONDB = 6,
};

enum class SblrMultiModelVerb : uint16_t {
  // Existing covered families remain valid.
  MONGO_FIND = 100,
  MONGO_AGGREGATE = 101,
  MONGO_FIND_AND_MODIFY = 102,
  MONGO_BULK_WRITE = 103,
  CQL_APPLY_BATCH = 200,
  REDIS_PUBLISH = 300,
  REDIS_SUBSCRIBE = 301,
  MILVUS_CREATE_COLLECTION = 400,
  MILVUS_SEARCH = 401,

  // New Beta 2 verbs.
  MONGO_COUNT = 500,
  MONGO_DISTINCT = 501,
  MONGO_CREATE_INDEXES = 502,
  MONGO_LIST_COLLECTIONS = 503,
  REDIS_MULTI = 600,
  REDIS_EXEC = 601,
  REDIS_DISCARD = 602,
  REDIS_WATCH = 603,
  REDIS_UNWATCH = 604,
  MILVUS_HYBRID_SEARCH = 700,
  OPENSEARCH_BULK = 800,
  OPENSEARCH_MSEARCH = 801,
  OPENSEARCH_CREATE_INDEX = 802,
  OPENSEARCH_DELETE_INDEX = 803,
  OPENSEARCH_PUT_MAPPING = 804,
  FOUNDATIONDB_COORDINATORS = 900,
  FOUNDATIONDB_DATADISTRIBUTION = 901,
  FOUNDATIONDB_MAINTENANCE = 902,
  FOUNDATIONDB_THROTTLE = 903,
  FOUNDATIONDB_VERSIONEPOCH = 904,
};

struct SblrNamedArgumentItem {
  SymbolId name_symbol = INVALID_SYMBOL_ID;
  PoolRef value_expr = INVALID_POOL_REF;
};

struct SblrMultiModelCommandPayload {
  SblrMultiModelFamily family = SblrMultiModelFamily::CQL;
  SblrMultiModelVerb verb = SblrMultiModelVerb::MONGO_FIND;
  PoolRef target_path_ref = INVALID_POOL_REF;
  VectorRef named_args = INVALID_VECTOR_REF;
  VectorRef positional_args = INVALID_VECTOR_REF;
  PoolRef embedded_query_ref = INVALID_POOL_REF;
  PoolRef temporal_binding = INVALID_POOL_REF;
  SymbolId donor_surface_symbol = INVALID_SYMBOL_ID;
};
```

Rules:

1. Existing multi-model families already present in the emitter shall move to
   the same canonical envelope family as the new verbs; there shall not be two
   incompatible command-payload paths.
2. Family and verb mismatches are verifier failures.
3. OpenSearch, FoundationDB, MongoDB expansion verbs, Redis transaction verbs,
   and Milvus `HybridSearch` require new executor dispatch coverage, but they
   still enter the engine through canonical SBLR.

## Verifier additions

The SBLR verifier shall reject:

1. ordered-set payloads missing `within_group_order`
2. named arguments without a name symbol
3. variadic arguments emitted without explicit mode
4. generic window payloads without a canonical function symbol
5. `PREWHERE` or `QUALIFY` expressions emitted into the wrong select slot
6. table-source discriminators that do not match the referenced payload type
7. temporal payloads missing an anchor where the clause kind requires one
8. policy payloads containing unsupported option-value shapes
9. multi-model envelopes whose verb does not belong to the selected family

## Sample emission sketch

```cpp
PoolRef emitFunctionCallV2(const FunctionCallExpr& expr) {
  SblrFunctionCallPayloadV2 payload;
  payload.canonical_function_symbol = resolveFunctionSymbol(expr.function_path);
  payload.donor_surface_symbol = internDonorSurface(expr.donor_surface_name);
  payload.argument_items = emitFunctionArgs(expr.argument_items);
  payload.filter_expr = emitOptionalExpr(expr.filter);
  payload.aggregate_order_items = emitOrderItems(expr.order_by);
  payload.ordered_set = emitOrderedSet(expr.ordered_set);
  payload.is_window = expr.is_window;
  payload.window_spec_ref = emitOptionalWindow(expr.window);
  payload.window_behavior = emitWindowBehavior(expr.window_behavior);
  return arena.store(payload);
}
```

## Required proof

1. every Beta 2 AST carrier from section `21` shall round-trip through SBLR
   serialization and decode
2. window-function identity shall survive encode and decode for every donor
   window family admitted in Beta 2
3. multi-model command envelopes shall verify family or verb compatibility
4. select-stage extension payloads shall retain deterministic order and render
   metadata across encode and decode
