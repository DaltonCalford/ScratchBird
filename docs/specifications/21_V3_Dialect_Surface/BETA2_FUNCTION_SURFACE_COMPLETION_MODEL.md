Status: current_authority_beta2

# Beta 2 Function Surface Completion Model

## Purpose

Close the remaining shared function and table-function gaps left after the
initial Beta 2 donor-dialect expansion so every emulation-target donor can map
its proven function surfaces into shared native-v3 and AST structures.

This file extends:

- `BETA2_DONOR_DIALECT_AST_GRAMMAR_AND_DESUGAR_EXPANSION_MODEL.md`

## Scope

This file owns:

1. SQL/XML function carriers
2. `XMLTABLE` and richer `ROWS FROM` item metadata
3. clause-rich SQL/JSON function carriers
4. aggregate-local dialect option carriers
5. MySQL-family special-function desugar landing zones
6. insert-source `VALUES(col)` expression semantics
7. parametric-function parameter carriers
8. lambda-expression carriers

This file does not own:

1. SBLR payload schemas; see section `22`
2. planner or executor semantics; see section `23`
3. donor-package wiring; see section `28`

## Donor justification

- PostgreSQL, Citus, and YugabyteDB require SQL/XML functions, `XMLTABLE`, and
  richer `ROWS FROM` item metadata.
- PostgreSQL, MySQL, and Vitess require clause-rich SQL/JSON function carriers.
- MySQL, MariaDB, Vitess, and TiDB require aggregate-local option carriers.
- MySQL, MariaDB, and Vitess require special syntax carriers for `TRIM`,
  `POSITION`, `SUBSTRING`, and `WEIGHT_STRING`.
- MySQL, MariaDB, TiDB, Vitess, and Dolt require insert-source `VALUES(col)`
  semantics inside duplicate-key conflict expressions.
- ClickHouse requires parametric-function parameters and lambda expressions.
- DuckDB requires lambda expressions.
- SQLite remains fail-closed for SQLite-unique function surfaces until local
  donor parser evidence exists.

## Hard invariants

1. Donor parsers shall not invent donor-private AST nodes for any surface owned
   by this file.
2. Clause metadata that changes result semantics or reverse-render fidelity
   shall survive parsing in structured form; it shall not be flattened into raw
   strings.
3. Native v3 shall admit the shared forms defined here. Donor parsers remain
   responsible for donor spellings, result shaping, plan shaping, and error
   shaping.
4. A shared function carrier may preserve donor-facing spelling metadata, but
   canonical execution semantics must remain engine-shared.
5. SQLite remains fail-closed for SQLite-unique function backlog until local
   donor parser evidence exists.

## Canonical AST additions

### 1. Function carrier completion

```cpp
enum class FunctionSurfaceSyntaxKind : uint8_t {
  ORDINARY = 0,
  MYSQL_TRIM,
  MYSQL_POSITION_IN,
  MYSQL_SUBSTRING_FROM,
  MYSQL_WEIGHT_STRING,
};

enum class AggregateNullPolicy : uint8_t {
  UNSPECIFIED = 0,
  NULL_ON_NULL,
  ABSENT_ON_NULL,
};

struct AggregateDialectOptions {
  Expression* separator = nullptr;
  Expression* local_limit = nullptr;
  Expression* local_offset = nullptr;
  AggregateNullPolicy null_policy = AggregateNullPolicy::UNSPECIFIED;
};

struct TrimSyntaxSpec {
  enum class FunctionType : uint8_t { TRIM = 0, LTRIM = 1, RTRIM = 2 };
  enum class Direction : uint8_t { DEFAULT = 0, LEADING = 1, TRAILING = 2, BOTH = 3 };

  FunctionType function_type = FunctionType::TRIM;
  Direction direction = Direction::DEFAULT;
  Expression* trim_arg = nullptr;
  Expression* source = nullptr;
};

struct PositionSyntaxSpec {
  Expression* needle = nullptr;
  Expression* haystack = nullptr;
};

struct SubstringSyntaxSpec {
  Expression* source = nullptr;
  Expression* start = nullptr;
  Expression* length = nullptr;
  bool used_from_keyword = false;
  bool used_for_keyword = false;
};

struct WeightStringSpec {
  enum class AsKind : uint8_t { NONE = 0, CHAR = 1, BINARY = 2 };

  Expression* source = nullptr;
  AsKind as_kind = AsKind::NONE;
  Expression* codepoints = nullptr;
  Expression* level_1 = nullptr;
  Expression* level_2 = nullptr;
  Expression* level_3 = nullptr;
};
```

`FunctionCallExpr` is extended as follows:

```cpp
class SqlJsonFunctionSpec;
class SqlXmlFunctionSpec;
class LambdaExpr;

class FunctionCallExpr : public Expression {
public:
  SchemaPath function_path;
  std::vector<FunctionArgumentItem> argument_items;
  std::vector<FunctionArgumentItem> parameter_items;
  bool distinct = false;
  Expression* filter = nullptr;
  std::vector<OrderByItem*> order_by;
  OrderedSetAggregateSpec ordered_set;
  AggregateDialectOptions aggregate_dialect;
  bool is_window = false;
  WindowSpec* window = nullptr;
  WindowBehaviorSpec window_behavior;
  bool preserve_function_identity = false;
  StringPool::StringId donor_surface_name = StringPool::INVALID_ID;
  FunctionSurfaceSyntaxKind surface_syntax = FunctionSurfaceSyntaxKind::ORDINARY;
  TrimSyntaxSpec* trim_syntax = nullptr;
  PositionSyntaxSpec* position_syntax = nullptr;
  SubstringSyntaxSpec* substring_syntax = nullptr;
  WeightStringSpec* weight_string = nullptr;
  SqlJsonFunctionSpec* sql_json = nullptr;
  SqlXmlFunctionSpec* sql_xml = nullptr;
};
```

Rules:

1. `parameter_items` are distinct from `argument_items` and exist only for
   parametric functions such as `quantile(0.9)(x)`.
2. Aggregate-local options such as `SEPARATOR`, local `LIMIT`, and
   constructor-local null policy shall live in `aggregate_dialect`, not in the
   normal argument vector.
3. MySQL-family alternate syntaxes shall normalize into canonical function
   symbols while preserving syntax-specific structure in the matching `*_syntax`
   carrier.
4. A function shall not carry both `surface_syntax = ORDINARY` and any
   non-null special syntax payload.

### 2. SQL/JSON function carrier

```cpp
enum class SqlJsonFunctionKind : uint8_t {
  VALUE = 0,
  QUERY = 1,
  EXISTS = 2,
  SERIALIZE = 3,
  OBJECT = 4,
  ARRAY = 5,
  OBJECT_AGG = 6,
  ARRAY_AGG = 7,
};

enum class SqlJsonBehaviorKind : uint8_t {
  UNSPECIFIED = 0,
  NULL_VALUE,
  DEFAULT_VALUE,
  ERROR_VALUE,
  EMPTY_ARRAY,
  EMPTY_OBJECT,
  TRUE_VALUE,
  FALSE_VALUE,
  UNKNOWN_VALUE,
};

enum class SqlJsonWrapperMode : uint8_t {
  UNSPECIFIED = 0,
  WITHOUT_WRAPPER,
  WITH_CONDITIONAL_WRAPPER,
  WITH_UNCONDITIONAL_WRAPPER,
};

enum class SqlJsonQuotesMode : uint8_t {
  UNSPECIFIED = 0,
  KEEP_QUOTES,
  OMIT_QUOTES,
};

enum class SqlJsonUniqueKeysMode : uint8_t {
  UNSPECIFIED = 0,
  WITH_UNIQUE_KEYS,
  WITHOUT_UNIQUE_KEYS,
};

struct SqlJsonBehaviorSpec {
  SqlJsonBehaviorKind kind = SqlJsonBehaviorKind::UNSPECIFIED;
  Expression* default_expr = nullptr;
};

class SqlJsonFunctionSpec {
public:
  SqlJsonFunctionKind kind = SqlJsonFunctionKind::VALUE;
  Expression* context_item = nullptr;
  Expression* path_expr = nullptr;
  std::vector<NamedExpressionItem> passing_args;
  TypeName returning_type;
  SqlJsonBehaviorSpec on_empty;
  SqlJsonBehaviorSpec on_error;
  SqlJsonWrapperMode wrapper_mode = SqlJsonWrapperMode::UNSPECIFIED;
  SqlJsonQuotesMode quotes_mode = SqlJsonQuotesMode::UNSPECIFIED;
  SqlJsonUniqueKeysMode unique_keys = SqlJsonUniqueKeysMode::UNSPECIFIED;
};
```

Rules:

1. `JSON_VALUE`, `JSON_QUERY`, `JSON_EXISTS`, `JSON_SERIALIZE`,
   `JSON_OBJECTAGG`, and `JSON_ARRAYAGG` shall use `SqlJsonFunctionSpec`.
2. `JSON_TABLE` remains owned by the table-source model from the earlier Beta 2
   spec and is not carried here.
3. `RETURNING`, `ON EMPTY`, `ON ERROR`, wrapper mode, quote mode, and unique
   keys mode shall survive parsing as structured fields.
4. Donor spellings may differ, but canonical semantics shall map to the same
   `SqlJsonFunctionKind`.

### 3. SQL/XML function carrier

```cpp
enum class SqlXmlFunctionKind : uint8_t {
  XMLCONCAT = 0,
  XMLELEMENT = 1,
  XMLEXISTS = 2,
  XMLFOREST = 3,
  XMLPARSE = 4,
  XMLPI = 5,
  XMLROOT = 6,
  XMLSERIALIZE = 7,
};

enum class SqlXmlDocumentMode : uint8_t {
  UNSPECIFIED = 0,
  DOCUMENT,
  CONTENT,
};

enum class SqlXmlWhitespaceMode : uint8_t {
  UNSPECIFIED = 0,
  PRESERVE,
  STRIP,
};

struct SqlXmlAttributeItem {
  StringPool::StringId name = StringPool::INVALID_ID;
  Expression* value = nullptr;
};

class SqlXmlFunctionSpec {
public:
  SqlXmlFunctionKind kind = SqlXmlFunctionKind::XMLCONCAT;
  SqlXmlDocumentMode document_mode = SqlXmlDocumentMode::UNSPECIFIED;
  SqlXmlWhitespaceMode whitespace_mode = SqlXmlWhitespaceMode::UNSPECIFIED;
  StringPool::StringId element_name = StringPool::INVALID_ID;
  std::vector<SqlXmlAttributeItem> attributes;
  Expression* expr = nullptr;
  Expression* version_expr = nullptr;
  Expression* standalone_expr = nullptr;
  TypeName serialize_target_type;
};
```

Rules:

1. PostgreSQL-family SQL/XML functions shall lower through
   `SqlXmlFunctionSpec`, not donor-private XML expression nodes.
2. XML attributes, document/content mode, whitespace mode, root version, and
   serialize target type shall remain explicit in the AST.
3. `XMLTABLE` is not part of `SqlXmlFunctionSpec`; it is a table-source carrier.

### 4. Insert-source values expression

```cpp
class InsertSourceValueExpr : public Expression {
public:
  ASTKind kind() const override { return ASTKind::InsertSourceValueExpr; }
  SchemaPath column_path;
  StringPool::StringId donor_surface_name = StringPool::INVALID_ID;
};
```

Rules:

1. MySQL-family `VALUES(col)` used in duplicate-key update expressions shall
   lower to `InsertSourceValueExpr`.
2. The expression is legal only inside insert conflict-update scope.
3. The expression shall not be rewritten into a plain column reference.

### 5. Lambda expression

```cpp
enum class LambdaSyntaxKind : uint8_t {
  SINGLE_ARROW = 0,
  KEYWORD_COLON = 1,
  FUNCTION_FORM = 2,
};

struct LambdaParameterItem {
  StringPool::StringId name = StringPool::INVALID_ID;
};

class LambdaExpr : public Expression {
public:
  ASTKind kind() const override { return ASTKind::LambdaExpr; }
  LambdaSyntaxKind syntax = LambdaSyntaxKind::SINGLE_ARROW;
  std::vector<LambdaParameterItem> parameters;
  Expression* body = nullptr;
};
```

Rules:

1. ClickHouse and DuckDB lambda forms shall lower to `LambdaExpr`.
2. `->` shall not be treated as JSON extraction when the parse context proves a
   lambda form.
3. `LambdaExpr` may appear only where a donor surface permits higher-order
   function arguments.

### 6. Table-source completion

```cpp
struct TableFunctionColumnDef {
  StringPool::StringId column_name = StringPool::INVALID_ID;
  TypeName target_type;
  bool has_default = false;
  Expression* default_expr = nullptr;
  bool is_not_null = false;
};

struct RowsFromItem {
  FunctionCallExpr* function = nullptr;
  std::vector<StringPool::StringId> column_aliases;
  std::vector<TableFunctionColumnDef> column_defs;
};

struct XmlTableNamespaceItem {
  StringPool::StringId prefix = StringPool::INVALID_ID;
  Expression* uri_expr = nullptr;
  bool default_namespace = false;
};

struct XmlTableColumnSpec {
  StringPool::StringId column_name = StringPool::INVALID_ID;
  TypeName target_type;
  bool for_ordinality = false;
  Expression* path_expr = nullptr;
  Expression* default_expr = nullptr;
  bool is_not_null = false;
};

struct XmlTableSpec {
  std::vector<XmlTableNamespaceItem> namespaces;
  Expression* row_expr = nullptr;
  Expression* document_expr = nullptr;
  std::vector<XmlTableColumnSpec> columns;
};
```

`TableRefNode::Type` is extended with `XML_TABLE`, and `TableRefNode` gains:

```cpp
XmlTableSpec* xml_table = nullptr;
```

Rules:

1. `ROWS FROM` item-level typed column definitions shall survive parsing.
2. `XMLTABLE` shall be a first-class table-source kind, not a generic function
   call.
3. `XMLTABLE` namespace items, ordinality columns, paths, defaults, and
   nullability shall remain structured.

## Native-v3 grammar obligations

Native v3 shall admit the following additional Beta 2 surfaces:

1. PostgreSQL-family SQL/XML functions
2. `XMLTABLE`
3. clause-rich `JSON_VALUE`, `JSON_QUERY`, `JSON_EXISTS`, `JSON_SERIALIZE`,
   `JSON_OBJECTAGG`, and `JSON_ARRAYAGG`
4. MySQL-family `TRIM`, `POSITION(... IN ...)`, `SUBSTRING(... FROM ... FOR ...)`,
   and `WEIGHT_STRING`
5. insert-source `VALUES(col)`
6. ClickHouse parametric function syntax `f(p1, ...)(arg1, ...)`
7. ClickHouse and DuckDB lambda syntax:
   `x -> expr`, `(x, y) -> expr`, and `lambda x, y: expr`

## Canonical desugar rules

1. MySQL-family special syntaxes shall lower into canonical function symbols
   with the matching syntax payload populated.
2. `VALUES(col)` shall lower to `InsertSourceValueExpr`.
3. Parametric functions shall lower to `FunctionCallExpr` with
   `parameter_items` carrying the first argument list and `argument_items`
   carrying the execution argument list.
4. Lambda syntax shall lower to `LambdaExpr` and never to a string or opaque
   function argument.
5. PostgreSQL-family `XMLTABLE` shall lower to `TableRefNode::Type::XML_TABLE`.
6. PostgreSQL-family `ROWS FROM` typed column definitions shall populate
   `RowsFromItem.column_defs`.

## Sample lowering snippets

```cpp
FunctionCallExpr* lowerMysqlWeightString(const MysqlWeightStringNode& donor) {
  auto* fn = arena.make<FunctionCallExpr>();
  fn->function_path = resolveBuiltin("weight_string");
  fn->surface_syntax = FunctionSurfaceSyntaxKind::MYSQL_WEIGHT_STRING;
  fn->weight_string = arena.make<WeightStringSpec>();
  fn->weight_string->source = lowerExpr(donor.source);
  fn->weight_string->as_kind = donor.as_binary
    ? WeightStringSpec::AsKind::BINARY
    : WeightStringSpec::AsKind::CHAR;
  fn->weight_string->codepoints = lowerOptionalExpr(donor.codepoints);
  fn->preserve_function_identity = true;
  fn->donor_surface_name = pool.intern("WEIGHT_STRING");
  return fn;
}

Expression* lowerMysqlValuesExpr(const MysqlValuesNode& donor) {
  auto* expr = arena.make<InsertSourceValueExpr>();
  expr->column_path = lowerColumnPath(donor.column_name);
  expr->donor_surface_name = pool.intern("VALUES");
  return expr;
}

TableRefNode* lowerPostgresXmlTable(const PgXmlTableNode& donor) {
  auto* ref = arena.make<TableRefNode>();
  ref->ref_type = TableRefNode::Type::XML_TABLE;
  ref->xml_table = lowerXmlTableSpec(donor);
  return ref;
}
```

## Required proof

1. every new function surface listed here parses in native v3 and in the owning
   donor parser
2. AST round-trip preserves syntax payloads, clause metadata, and donor-facing
   render identity
3. `VALUES(col)` is rejected outside insert conflict-update scope
4. lambda parsing disambiguates JSON extraction from lambda syntax
5. `XMLTABLE` and `ROWS FROM` typed column definitions survive round-trip
6. SQLite remains fail-closed for SQLite-unique backlog until local donor
   parser evidence exists
