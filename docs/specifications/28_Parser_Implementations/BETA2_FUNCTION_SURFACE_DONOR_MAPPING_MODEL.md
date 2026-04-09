Status: current_authority_beta2

# Beta 2 Function Surface Donor Mapping Model

## Purpose

Define how the remaining donor function gaps map into the shared AST, SBLR, and
runtime structures owned by sections `21`, `22`, and `23`.

This file extends:

- `BETA2_EMULATED_DONOR_MAPPING_AND_SHARED_LOWERING_MODEL.md`

## Hard invariants

1. Donor parsers shall lower every surface owned by this file into the shared
   carriers from sections `21` through `23`.
2. Donor parsers shall not retain donor-private expression or table-function
   nodes for any gap closed here.
3. Native v3 shall accept the shared syntax forms owned by this file once the
   relevant parser work lands.
4. SQLite remains fail-closed for SQLite-unique function backlog until local
   donor parser evidence exists.

## Mapping buckets

### 1. PostgreSQL-family SQL/XML and table-function completion

Mapped donors:

- `PostgreSQL`
- `Citus`
- `YugabyteDB`

Canonical target:

- section `21` `SqlXmlFunctionSpec`
- section `21` `XmlTableSpec`
- section `21` `RowsFromItem.column_defs`
- section `22` `SblrSqlXmlFunctionPayload`
- section `22` `SblrXmlTablePayload`
- section `22` `SblrRowsFromItemPayload`
- section `23` `SQL_XML_FUNCTION_EVAL`
- section `23` `XML_TABLE_SCAN`
- section `23` `ROWS_FROM_FANOUT_V2`

Rules:

1. PostgreSQL-family SQL/XML functions shall lower through `FunctionCallExpr`
   with `sql_xml != nullptr`.
2. `XMLTABLE` shall lower to `TableRefNode::Type::XML_TABLE`.
3. `ROWS FROM` item-level typed column definitions shall populate
   `RowsFromItem.column_defs`.

### 2. Clause-rich SQL/JSON function completion

Mapped donors:

- `PostgreSQL`
- `MySQL`
- `Vitess`

Canonical target:

- section `21` `SqlJsonFunctionSpec`
- section `22` `SblrSqlJsonFunctionPayload`
- section `23` `SQL_JSON_FUNCTION_EVAL`

Rules:

1. Donor `RETURNING`, `ON EMPTY`, `ON ERROR`, wrapper, quote, and unique-key
   clauses shall lower to `SqlJsonFunctionSpec`.
2. Donor parsers may keep donor spellings for reverse-render only via
   `donor_surface_name`.

### 3. MySQL-family aggregate and special-function completion

Mapped donors:

- `MySQL`
- `MariaDB`
- `Vitess`
- `TiDB`

Canonical target:

- section `21` `AggregateDialectOptions`
- section `21` `TrimSyntaxSpec`
- section `21` `PositionSyntaxSpec`
- section `21` `SubstringSyntaxSpec`
- section `21` `WeightStringSpec`
- section `22` `SblrAggregateDialectOptionsPayload`
- section `22` syntax payload refs in `SblrFunctionCallPayloadV3`
- section `23` shared aggregate and function runtime bindings

Rules:

1. `GROUP_CONCAT` separator and local limit semantics shall populate
   `AggregateDialectOptions`.
2. `TRIM`, `POSITION`, `SUBSTRING`, and `WEIGHT_STRING` shall lower to
   canonical function symbols plus the matching syntax payload.
3. TiDB uses the same aggregate option bucket even where its AST is less
   specialized than Vitess.

### 4. Insert-source values completion

Mapped donors:

- `MySQL`
- `MariaDB`
- `TiDB`
- `Vitess`
- `Dolt`

Canonical target:

- section `21` `InsertSourceValueExpr`
- section `22` `SblrInsertSourceValuePayload`
- section `23` `INSERT_SOURCE_VALUE_READ`

Rules:

1. Every donor `VALUES(col)` expression in duplicate-key update scope shall
   lower to `InsertSourceValueExpr`.
2. Donor parsers shall reject or normalize any donor-private legacy spelling to
   the same shared expression.

### 5. ClickHouse and DuckDB higher-order completion

Mapped donors:

- `ClickHouse`
- `DuckDB`

Canonical target:

- section `21` `LambdaExpr`
- section `21` `FunctionCallExpr.parameter_items`
- section `22` `SblrLambdaPayload`
- section `22` `SblrFunctionCallPayloadV3.parameter_items`
- section `23` `LAMBDA_BIND`
- section `23` `PARAMETRIC_FUNCTION_BIND`

Rules:

1. ClickHouse parametric functions shall split the first argument list into
   `parameter_items` and the second into `argument_items`.
2. ClickHouse and DuckDB lambda forms shall lower to `LambdaExpr`.
3. Donor parser ambiguity between JSON extraction and lambda `->` must be
   resolved before AST emission.

## Donor families with no new mapping bucket from this file

The following donors inherit only the already-admitted Beta 2 function
structures from the earlier mapping model and add no new shared bucket here:

- `Apache Ignite`
- `Cassandra`
- `CockroachDB`
- `FirebirdSQL`
- `FoundationDB`
- `immudb`
- `InfluxDB`
- `Milvus`
- `MongoDB`
- `Neo4j`
- `OpenSearch`
- `Redis`
- `XTDB`

## Sample lowering snippets

```cpp
Expression* lowerVitessValuesFunc(const VitessValuesFuncExpr& donor) {
  auto* expr = arena.make<InsertSourceValueExpr>();
  expr->column_path = lowerColumnPath(donor.Name);
  expr->donor_surface_name = pool.intern("VALUES");
  return expr;
}

FunctionCallExpr* lowerClickHouseParametricFunction(const ChFunctionNode& donor) {
  auto* fn = arena.make<FunctionCallExpr>();
  fn->function_path = lowerFunctionPath(donor.name);
  fn->parameter_items = lowerFunctionArgs(donor.parameters);
  fn->argument_items = lowerFunctionArgs(donor.arguments);
  fn->preserve_function_identity = true;
  fn->donor_surface_name = pool.intern(donor.name);
  return fn;
}

Expression* lowerDuckDbLambda(const DuckLambdaNode& donor) {
  auto* expr = arena.make<LambdaExpr>();
  expr->syntax = LambdaSyntaxKind::KEYWORD_COLON;
  expr->parameters = lowerLambdaParams(donor.params);
  expr->body = lowerExpr(donor.body);
  return expr;
}
```

## Required proof

1. every donor listed here emits the shared carriers named in its bucket
2. equivalent donor and native-v3 inputs produce equivalent canonical payloads
3. no donor parser retains donor-private AST or SBLR structures for these
   surfaces
4. SQLite remains fail-closed until local donor parser evidence exists
