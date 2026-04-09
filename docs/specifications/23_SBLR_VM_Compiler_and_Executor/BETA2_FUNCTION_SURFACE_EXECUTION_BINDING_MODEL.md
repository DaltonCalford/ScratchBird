Status: current_authority_beta2

# Beta 2 Function Surface Execution Binding Model

## Purpose

Define compiler, planner, runtime, and explain bindings for the remaining Beta 2
function-surface backlog.

This file extends:

- `BETA2_DONOR_DIALECT_EXECUTION_AND_PLANNER_BINDING_MODEL.md`

## Hard invariants

1. Parser-owned function backlog shall execute through shared runtime bindings,
   not donor-private evaluators.
2. Explain and reverse-render fidelity shall preserve donor-visible function
   semantics when this file marks them as identity-bearing.
3. Insert-source `VALUES(col)` semantics shall bind to the candidate insert row,
   not the pre-existing target row.

## Execution bindings

### 1. SQL/XML function family

1. `XMLCONCAT`, `XMLELEMENT`, `XMLEXISTS`, `XMLFOREST`, `XMLPARSE`, `XMLPI`,
   `XMLROOT`, and `XMLSERIALIZE` shall bind through a shared SQL/XML runtime
   library.
2. The runtime library shall consume the structured `SqlXmlFunctionSpec`
   payload directly.
3. `XMLSERIALIZE` shall cast only through the declared serialize target type.
4. Explain output shall preserve the canonical function symbol and donor-facing
   spelling for PostgreSQL-family rendering.

### 2. SQL/JSON function family

1. `JSON_VALUE`, `JSON_QUERY`, `JSON_EXISTS`, `JSON_SERIALIZE`,
   `JSON_OBJECTAGG`, and `JSON_ARRAYAGG` shall bind through the shared SQL/JSON
   runtime library.
2. `RETURNING`, `ON EMPTY`, `ON ERROR`, wrapper mode, quote mode, and unique
   keys mode shall be honored by runtime evaluation, not reconstructed from
   string rendering.
3. Aggregates that also carry `AggregateDialectOptions` shall respect both the
   SQL/JSON spec and aggregate-local option payloads.

### 3. `XMLTABLE` and richer `ROWS FROM`

1. `XMLTABLE` shall compile into a dedicated `XML_TABLE_SCAN` plan operator.
2. `ROWS FROM` with typed item-level column definitions shall compile into
   `ROWS_FROM_FANOUT_V2`.
3. The runtime shall materialize the declared output row shape exactly in the
   order and types declared by the parser payload.

### 4. Aggregate-local options

1. `GROUP_CONCAT`-style aggregates shall bind through normal aggregate
   dispatch, but the runtime shall consume `AggregateDialectOptions` for
   separator, local limit, local offset, and null policy.
2. Aggregate-local limit shall apply inside the aggregate implementation, not
   as an outer query limit.
3. Explain output shall preserve the donor-facing modifier ordering.

### 5. MySQL-family special syntax

1. `TRIM`, `POSITION`, `SUBSTRING`, and `WEIGHT_STRING` alternate forms shall
   compile into the canonical function symbol recorded by the AST.
2. The special syntax payload is execution-relevant only where semantics differ
   from a plain argument list; otherwise it is render-identity metadata.
3. `WEIGHT_STRING` shall bind through a dedicated collation-weight runtime path.

### 6. Insert-source `VALUES(col)`

1. `InsertSourceValueExpr` shall compile into a candidate-row slot read.
2. Candidate-row reads are resolved after INSERT source projection and before
   duplicate-key update expression execution.
3. The runtime shall reject `InsertSourceValueExpr` outside duplicate-key
   conflict-update scope.

### 7. Parametric functions

1. The function binder shall resolve parametric functions using both the
   canonical symbol and the parameter list.
2. Parameter items shall participate in function overload resolution,
   determinism keys, and explain identity.
3. Parameter items shall not be treated as ordinary row expressions at bind
   time.

### 8. Lambda expressions

1. `LambdaExpr` shall compile into a closure frame with explicit parameter
   symbols and a body expression ref.
2. Lambda parameters shall shadow outer names inside the lambda scope only.
3. Higher-order functions shall consume lambda payloads directly; they shall
   not parse or reinterpret stringified lambda bodies.

## Plan node additions

The planner shall admit the following first-class nodes:

1. `XML_TABLE_SCAN`
2. `ROWS_FROM_FANOUT_V2`
3. `SQL_JSON_FUNCTION_EVAL`
4. `SQL_XML_FUNCTION_EVAL`
5. `INSERT_SOURCE_VALUE_READ`
6. `LAMBDA_BIND`
7. `PARAMETRIC_FUNCTION_BIND`

## Determinism and cache rules

1. Plan keys shall include:
   - parametric-function parameters
   - aggregate-local options
   - SQL/JSON clause metadata
   - SQL/XML mode metadata
   - lambda syntax kind and body hash
2. `VALUES(col)` candidate-row slot bindings shall not leak target-row values
   into plan-key derivation.

## Sample binding snippets

```cpp
PlanExpr bindInsertSourceValue(const InsertSourceValueExpr& expr, BindCtx& ctx) {
  require(ctx.in_duplicate_key_update);
  return makePlanExpr(Op::INSERT_SOURCE_VALUE_READ,
                      ctx.resolveCandidateColumn(expr.column_path));
}

PlanExpr bindParametricFunction(const FunctionCallExpr& expr, BindCtx& ctx) {
  auto param_vector = bindExprVector(expr.parameter_items, ctx);
  auto arg_vector = bindExprVector(expr.argument_items, ctx);
  auto fn = resolveParametricBuiltin(expr.function_path, param_vector, arg_vector);
  return makePlanExpr(Op::PARAMETRIC_FUNCTION_BIND, fn, param_vector, arg_vector);
}
```

## Required proof

1. explain plans preserve donor-visible identity for every surface added here
2. `XML_TABLE_SCAN`, `ROWS_FROM_FANOUT_V2`, `INSERT_SOURCE_VALUE_READ`,
   `LAMBDA_BIND`, and `PARAMETRIC_FUNCTION_BIND` appear in runtime plan output
3. runtime rejects illegal `VALUES(col)` scope and invalid lambda binds
4. aggregate-local limit and separator semantics match donor-facing behavior
