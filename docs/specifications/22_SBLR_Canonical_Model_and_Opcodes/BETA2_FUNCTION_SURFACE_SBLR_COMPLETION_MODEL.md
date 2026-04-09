Status: current_authority_beta2

# Beta 2 Function Surface SBLR Completion Model

## Purpose

Define the SBLR payload, verifier, and opcode completion required for the
remaining Beta 2 function-surface backlog.

This file extends:

- `BETA2_DONOR_DIALECT_SBLR_PAYLOAD_AND_OPCODE_EXPANSION_MODEL.md`

## Hard invariants

1. Function-clause metadata shall survive encode and decode in structured form.
2. Parametric-function parameters shall not be merged into ordinary function
   arguments during SBLR emission.
3. Insert-source `VALUES(col)` expressions shall remain scope-checked in SBLR.
4. Lambda parameter identity shall survive encode and decode.
5. `XMLTABLE` and richer `ROWS FROM` item metadata shall use dedicated payloads,
   not opaque blobs.

## Canonical payload additions

### 1. Function call payload v3

```cpp
struct SblrAggregateDialectOptionsPayload {
  PoolRef separator_expr = INVALID_POOL_REF;
  PoolRef local_limit_expr = INVALID_POOL_REF;
  PoolRef local_offset_expr = INVALID_POOL_REF;
  uint8_t null_policy = 0;
};

struct SblrTrimSyntaxPayload {
  uint8_t function_type = 0;
  uint8_t direction = 0;
  PoolRef trim_arg_expr = INVALID_POOL_REF;
  PoolRef source_expr = INVALID_POOL_REF;
};

struct SblrPositionSyntaxPayload {
  PoolRef needle_expr = INVALID_POOL_REF;
  PoolRef haystack_expr = INVALID_POOL_REF;
};

struct SblrSubstringSyntaxPayload {
  PoolRef source_expr = INVALID_POOL_REF;
  PoolRef start_expr = INVALID_POOL_REF;
  PoolRef length_expr = INVALID_POOL_REF;
  bool used_from_keyword = false;
  bool used_for_keyword = false;
};

struct SblrWeightStringPayload {
  PoolRef source_expr = INVALID_POOL_REF;
  uint8_t as_kind = 0;
  PoolRef codepoints_expr = INVALID_POOL_REF;
  PoolRef level_1_expr = INVALID_POOL_REF;
  PoolRef level_2_expr = INVALID_POOL_REF;
  PoolRef level_3_expr = INVALID_POOL_REF;
};

struct SblrSqlJsonBehaviorPayload {
  uint8_t kind = 0;
  PoolRef default_expr = INVALID_POOL_REF;
};

struct SblrSqlJsonFunctionPayload {
  uint8_t kind = 0;
  PoolRef context_item_expr = INVALID_POOL_REF;
  PoolRef path_expr = INVALID_POOL_REF;
  VectorRef passing_args = INVALID_VECTOR_REF;
  PoolRef returning_type_ref = INVALID_POOL_REF;
  SblrSqlJsonBehaviorPayload on_empty;
  SblrSqlJsonBehaviorPayload on_error;
  uint8_t wrapper_mode = 0;
  uint8_t quotes_mode = 0;
  uint8_t unique_keys_mode = 0;
};

struct SblrSqlXmlAttributePayload {
  SymbolId name_symbol = INVALID_SYMBOL_ID;
  PoolRef value_expr = INVALID_POOL_REF;
};

struct SblrSqlXmlFunctionPayload {
  uint8_t kind = 0;
  uint8_t document_mode = 0;
  uint8_t whitespace_mode = 0;
  SymbolId element_name_symbol = INVALID_SYMBOL_ID;
  VectorRef attributes = INVALID_VECTOR_REF;
  PoolRef expr_ref = INVALID_POOL_REF;
  PoolRef version_expr = INVALID_POOL_REF;
  PoolRef standalone_expr = INVALID_POOL_REF;
  PoolRef serialize_target_type_ref = INVALID_POOL_REF;
};

struct SblrFunctionCallPayloadV3 {
  SymbolId canonical_function_symbol = INVALID_SYMBOL_ID;
  SymbolId donor_surface_symbol = INVALID_SYMBOL_ID;
  VectorRef argument_items = INVALID_VECTOR_REF;
  VectorRef parameter_items = INVALID_VECTOR_REF;
  bool distinct = false;
  PoolRef filter_expr = INVALID_POOL_REF;
  VectorRef aggregate_order_items = INVALID_VECTOR_REF;
  PoolRef aggregate_dialect_ref = INVALID_POOL_REF;
  VectorRef direct_arg_exprs = INVALID_VECTOR_REF;
  VectorRef within_group_order = INVALID_VECTOR_REF;
  bool is_window = false;
  PoolRef window_spec_ref = INVALID_POOL_REF;
  SblrWindowBehaviorPayload window_behavior;
  uint8_t surface_syntax = 0;
  PoolRef trim_syntax_ref = INVALID_POOL_REF;
  PoolRef position_syntax_ref = INVALID_POOL_REF;
  PoolRef substring_syntax_ref = INVALID_POOL_REF;
  PoolRef weight_string_ref = INVALID_POOL_REF;
  PoolRef sql_json_ref = INVALID_POOL_REF;
  PoolRef sql_xml_ref = INVALID_POOL_REF;
};
```

Rules:

1. `SblrFunctionCallPayloadV3` supersedes v2 for every surface owned by this
   file.
2. `parameter_items` shall encode parametric-function parameters only.
3. At most one of `trim_syntax_ref`, `position_syntax_ref`,
   `substring_syntax_ref`, or `weight_string_ref` may be populated.
4. `sql_json_ref` and `sql_xml_ref` are mutually exclusive.

### 2. Insert-source value and lambda payloads

```cpp
struct SblrInsertSourceValuePayload {
  SymbolId column_symbol = INVALID_SYMBOL_ID;
};

struct SblrLambdaParameterPayload {
  SymbolId name_symbol = INVALID_SYMBOL_ID;
};

struct SblrLambdaPayload {
  uint8_t syntax_kind = 0;
  VectorRef parameters = INVALID_VECTOR_REF;
  PoolRef body_expr = INVALID_POOL_REF;
};
```

### 3. Table-function payload completion

```cpp
struct SblrTableFunctionColumnDefPayload {
  SymbolId column_name_symbol = INVALID_SYMBOL_ID;
  PoolRef target_type_ref = INVALID_POOL_REF;
  bool has_default = false;
  PoolRef default_expr = INVALID_POOL_REF;
  bool is_not_null = false;
};

struct SblrRowsFromItemPayload {
  PoolRef function_expr = INVALID_POOL_REF;
  VectorRef column_alias_symbols = INVALID_VECTOR_REF;
  VectorRef column_defs = INVALID_VECTOR_REF;
};

struct SblrXmlTableNamespacePayload {
  SymbolId prefix_symbol = INVALID_SYMBOL_ID;
  PoolRef uri_expr = INVALID_POOL_REF;
  bool default_namespace = false;
};

struct SblrXmlTableColumnPayload {
  SymbolId column_name_symbol = INVALID_SYMBOL_ID;
  PoolRef target_type_ref = INVALID_POOL_REF;
  bool for_ordinality = false;
  PoolRef path_expr = INVALID_POOL_REF;
  PoolRef default_expr = INVALID_POOL_REF;
  bool is_not_null = false;
};

struct SblrXmlTablePayload {
  VectorRef namespaces = INVALID_VECTOR_REF;
  PoolRef row_expr = INVALID_POOL_REF;
  PoolRef document_expr = INVALID_POOL_REF;
  VectorRef columns = INVALID_VECTOR_REF;
};
```

`SblrTableRefKind` is extended with `XML_TABLE`.

## Opcode additions

The following canonical expression and table payload families are admitted:

1. `EXPR_FUNCTION_CALL_V3`
2. `EXPR_INSERT_SOURCE_VALUE`
3. `EXPR_LAMBDA`
4. `TABLE_XML_TABLE`
5. `TABLE_ROWS_FROM_V2`

## Verifier rules

1. `EXPR_INSERT_SOURCE_VALUE` is legal only inside insert conflict-update
   payload scope.
2. `EXPR_LAMBDA` must carry at least one parameter and a non-null body.
3. `EXPR_FUNCTION_CALL_V3` with non-empty `parameter_items` must not encode the
   same values again in `argument_items`.
4. `SQL_JSON` payloads must reject impossible clause combinations:
   invalid wrapper mode on `JSON_VALUE`, invalid unique-key mode on
   `JSON_SERIALIZE`, and duplicated `ON EMPTY` or `ON ERROR` semantics.
5. `SQL_XML` payloads must reject incompatible mode combinations such as
   missing serialize target type on `XMLSERIALIZE`.
6. `TABLE_XML_TABLE` must reject a column entry that is both
   `for_ordinality = true` and carries path/default/type override payloads.
7. `TABLE_ROWS_FROM_V2` must preserve per-item typed column definitions in the
   order provided by the parser.

## Sample emission snippets

```cpp
PoolRef emitInsertSourceValue(const InsertSourceValueExpr& expr) {
  SblrInsertSourceValuePayload payload;
  payload.column_symbol = resolveColumnSymbol(expr.column_path);
  return pool.write(payload);
}

PoolRef emitLambda(const LambdaExpr& expr) {
  SblrLambdaPayload payload;
  payload.syntax_kind = static_cast<uint8_t>(expr.syntax);
  payload.parameters = emitLambdaParams(expr.parameters);
  payload.body_expr = emitExpr(expr.body);
  return pool.write(payload);
}
```

## Required proof

1. every payload added here round-trips through encode, decode, and verifier
2. canonical order is deterministic for parameter items, aggregate options,
   XMLTABLE columns, and ROWS FROM item column definitions
3. invalid payload combinations are rejected fail-closed
4. donor-facing render identity is recoverable from the encoded payloads
