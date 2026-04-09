# Emulation Function Surface Gap Analysis

## Purpose

Identify donor function or function-like surfaces that still do not have a
shared landing zone in the current Beta 2 parser or AST or SBLR specification
set.

This package is intentionally narrower than the earlier dialect audit. It does
not repeat already-covered `APPLY`, `PIVOT`, `UNPIVOT`, `QUALIFY`,
`MATCH_RECOGNIZE`, ordered-set aggregates, named arguments, `VARIADIC`, window
null-treatment, or the existing multi-model command families. It only records
the function backlog that still lacks explicit Beta 2 authority.

## Reviewed authority

Canonical Beta 2 files:

- `/home/dcalford/CliWork/ScratchBird/docs/specifications/21_V3_Dialect_Surface/BETA2_DONOR_DIALECT_AST_GRAMMAR_AND_DESUGAR_EXPANSION_MODEL.md`
- `/home/dcalford/CliWork/ScratchBird/docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/BETA2_DONOR_DIALECT_SBLR_PAYLOAD_AND_OPCODE_EXPANSION_MODEL.md`
- `/home/dcalford/CliWork/ScratchBird/docs/specifications/23_SBLR_VM_Compiler_and_Executor/BETA2_DONOR_DIALECT_EXECUTION_AND_PLANNER_BINDING_MODEL.md`
- `/home/dcalford/CliWork/ScratchBird/docs/specifications/28_Parser_Implementations/BETA2_EMULATED_DONOR_MAPPING_AND_SHARED_LOWERING_MODEL.md`

Current ScratchBird parser or AST evidence:

- `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h`
- `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp`
- `/home/dcalford/CliWork/ScratchBird/src/parser/v3_emitter.cpp`

Donor authority comes from the per-engine `parser_ast` paths recorded in the
April 2 reference packet set.

## Executive result

The current Beta 2 spec set already closes most of the previously obvious SQL
dialect backlog. The remaining function backlog is smaller and more specific:

1. PostgreSQL-family SQL/XML function surfaces are still missing.
2. PostgreSQL-family `XMLTABLE` and richer `ROWS FROM` item metadata are still
   missing.
3. Clause-rich SQL/JSON function metadata is still missing.
4. MySQL-family aggregate-local options are still missing.
5. MySQL-family special function desugars are still missing.
6. MySQL-family `VALUES(col)` insert-source semantics are still missing.
7. ClickHouse parametric-function parameter lists are still missing.
8. ClickHouse and DuckDB lambda expressions are still missing.
9. SQLite remains blocked by a local donor-source evidence gap.

## Detailed findings

### 1. PostgreSQL-family SQL/XML functions are still unspecced

PostgreSQL exposes a full SQL/XML function family in
`src/backend/parser/gram.y`, including `XMLCONCAT`, `XMLELEMENT`,
`XMLEXISTS`, `XMLFOREST`, `XMLPARSE`, `XMLPI`, `XMLROOT`, and
`XMLSERIALIZE`. YugabyteDB carries the same PostgreSQL-derived family in
`src/postgres/src/backend/parser/gram.y`.

The current Beta 2 function carrier in section `21` is still limited to:

- structured argument items
- ordered-set metadata
- aggregate-local `ORDER BY`
- window metadata

It does not provide any structured landing zone for XML-specific clause data
such as:

- document vs content mode
- XML attributes lists
- whitespace handling
- root version and standalone controls
- XML serialize target type

This is not a parser-only omission. The missing information must survive AST,
SBLR, executor, explain, and donor reverse rendering.

### 2. `XMLTABLE` still has no shared table-source carrier

PostgreSQL and YugabyteDB both expose `XMLTABLE` with:

- optional `XMLNAMESPACES`
- a row expression plus a document expression
- typed column definitions
- `PATH`
- `DEFAULT`
- nullability
- `FOR ORDINALITY`

The current Beta 2 table-source expansion only defines carriers for:

- `JSON_TABLE`
- `ROWS FROM`
- `PIVOT`
- `UNPIVOT`
- `MATCH_RECOGNIZE`

There is no `XMLTABLE` carrier in section `21`, no SBLR payload in section
`22`, and no executor binding in section `23`.

This also affects the PostgreSQL-family emulation lanes that inherit the same
grammar surface, including the Citus lane where PostgreSQL semantics remain the
baseline for parser-facing SQL.

### 3. `ROWS FROM` is still underspecified for record-returning functions

The current Beta 2 `RowsFromItem` stores only:

- a function pointer
- column aliases

That is not enough for PostgreSQL-family `ROWS FROM` semantics. The donor
grammar explicitly documents per-function typed column definition lists, for
example:

`ROWS FROM (foo() AS (foo_res_a text, foo_res_b text), bar() AS (...))`

Without typed per-item column definition carriers, record-returning table
functions still require parser-private state or lossy lowering.

### 4. Clause-rich SQL/JSON functions are still missing

The Beta 2 quartet admits `JSON_TABLE`, but it does not yet define a general
function-clause carrier for SQL/JSON constructor and query functions.

Source-backed donor evidence shows the missing clause families include:

- PostgreSQL:
  `JSON_QUERY`, `JSON_EXISTS`, `JSON_VALUE`, `JSON_SERIALIZE`,
  `JSON_OBJECTAGG`, `JSON_ARRAYAGG`, `WITH UNIQUE KEYS`,
  `ABSENT ON NULL`, `ON EMPTY`, and `ON ERROR`
- MySQL:
  `JSON_VALUE ... RETURNING ... ON EMPTY ... ON ERROR`,
  `JSON_ARRAYAGG(... opt_json_constructor_null_clause)`,
  `JSON_OBJECTAGG(...)`
- Vitess:
  dedicated `JSONValueExpr` with `ReturningType`,
  `EmptyOnResponse`, and `ErrorOnResponse`

Current ScratchBird parser code does already have JSON existence and extraction
operators, but those are much narrower. The current code path only proves:

- binary JSON existence operators
- arrow and hash-arrow extract operators
- a few function-name special cases such as `JSON_EXTRACT`

That is not equivalent to clause-rich SQL/JSON function support.

### 5. Aggregate-local options are still missing

The current Beta 2 `FunctionCallExpr` supports:

- `distinct`
- `filter`
- aggregate-local `ORDER BY`
- ordered-set metadata

It still does not provide:

- aggregate-local `SEPARATOR`
- aggregate-local `LIMIT`
- constructor-local null policy options

That is enough to block exact lowering for:

- MySQL `GROUP_CONCAT(... SEPARATOR ...)`
- MariaDB `GROUP_CONCAT(... SEPARATOR ... LIMIT ...)`
- MariaDB `JSON_ARRAYAGG(... ORDER BY ... LIMIT ...)`
- Vitess `GroupConcatExpr` with `Separator` and `Limit`
- TiDB `GROUP_CONCAT` with `OptGConcatSeparator`

This needs a reusable aggregate-option carrier, not another donor-private AST.

### 6. MySQL-family special function syntaxes are still missing a desugar contract

MySQL, MariaDB, and Vitess all expose function families that are not just
plain `name(args...)` calls. Source-backed examples include:

- `TRIM(LEADING expr FROM expr)`
- `TRIM(expr FROM expr)`
- `POSITION(expr IN expr)`
- `SUBSTRING(expr FROM expr FOR expr)`
- `WEIGHT_STRING(expr AS CHAR(...))`

Vitess goes further and materializes several of these as dedicated AST nodes:

- `TrimFuncExpr`
- `WeightStringFuncExpr`
- `SubstrExpr`

The current Beta 2 grammar obligations do not define a native-v3 admission or
desugar contract for these alternate syntaxes. That omission matters even if
some of them eventually lower to ordinary scalar calls, because the parser
still needs a canonical route from donor syntax into shared AST.

### 7. MySQL-family `VALUES(col)` still has no canonical expression

The new Beta 2 insert work admits `ON DUPLICATE KEY UPDATE`, but it still does
not define the insert-source value expression used by MySQL-family donors:

- MySQL `Item_insert_value`
- MariaDB `Item_insert_value`
- TiDB `ValuesExpr`
- Vitess `ValuesFuncExpr`
- Dolt testcases using `VALUES(col_c)` in duplicate-key updates

This is not a normal scalar function. It is a context-sensitive insert-source
expression whose meaning depends on the candidate inserted row.

Without a shared `InsertSourceValueExpr`, MySQL-family emulation still needs
donor-private expression classes or ad hoc rewrite state.

### 8. ClickHouse still needs a parametric-function carrier

ClickHouse `ASTFunction` explicitly distinguishes:

- `arguments`
- `parameters`

The donor comments use `quantile(0.9)(x)` as the canonical example.

The current Beta 2 function carrier has only one argument list, so it cannot
losslessly represent parametric aggregate/function calls that separate the
configuration phase from the execution-argument phase.

### 9. ClickHouse and DuckDB still need a shared lambda expression

DuckDB has an explicit `LambdaExpression` node and documents the ambiguity
between lambda syntax and the JSON `->` operator.

ClickHouse also marks lambda functions explicitly:

- `ASTFunction.is_lambda_function`
- operator table entry `{"->", Operator("lambda", ...)}`

The current ScratchBird parser still uses `TokenType::ARROW` as JSON extract
only. The Beta 2 function model likewise does not define:

- lambda parameters
- lambda body
- donor syntax flavor
- lambda scope rules

This gap blocks higher-order function parity for ClickHouse and DuckDB.

## Engines with no additional gap isolated in this audit

The following engines were reviewed against the new Beta 2 function-expansion
set, and this audit did not isolate an additional shared function carrier gap
from the reviewed source slice:

- Apache Ignite
- Cassandra
- CockroachDB
- FirebirdSQL
- FoundationDB
- immudb
- InfluxDB
- Milvus
- MongoDB
- Neo4j
- OpenSearch
- Redis
- XTDB

This statement is intentionally narrow. It means only that this audit did not
find an uncovered shared function structure in the reviewed local source slice.
It does not mean those donors are fully implemented already.

## Not counted as gaps here

The following surfaces were checked and intentionally not counted as missing
from the new specifications because the Beta 2 quartet already creates a shared
landing zone for them:

- `APPLY`
- `QUALIFY`
- `PIVOT`
- `UNPIVOT`
- `MATCH_RECOGNIZE` as a table-source carrier
- ordered-set aggregates with `WITHIN GROUP`
- named arguments
- `VARIADIC`
- window-function identity preservation
- `IGNORE NULLS`
- `RESPECT NULLS`
- `FROM FIRST`
- `FROM LAST`
- shared multi-model command families

## Immediate implication

If the goal is to make every new emulation parser map to already-existing
shared AST or SBLR structures, then the next Beta 2 follow-on spec set should
focus on only six implementation buckets:

1. SQL/XML function carriers
2. `XMLTABLE` plus richer table-function column-definition carriers
3. SQL/JSON function-clause carriers
4. aggregate-local option carriers
5. MySQL-family special-function and `VALUES(col)` carriers
6. ClickHouse or DuckDB lambda and parametric-function carriers

Once those are formalized, the remaining donor function backlog becomes mostly
parser mapping and executor library work instead of structural AST or SBLR
design work.
