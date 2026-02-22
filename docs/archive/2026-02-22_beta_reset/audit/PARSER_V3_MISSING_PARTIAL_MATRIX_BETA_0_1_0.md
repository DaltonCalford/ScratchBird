# Parser V3 Missing/Partial Matrix (Beta 0.1.0)

- Audit date: `2026-02-19`
- Scope: native parser v3 pipeline closure check before beta publication

## 1. Method

This matrix is implementation-backed only, using:

- `src/parser/parser_v3.cpp`
- `include/scratchbird/parser/ast_v3.h`
- `src/parser/v3_emitter.cpp`
- `src/sblr/executor.cpp`

Status labels:

- `Implemented`: parser + emitter + executor all present.
- `Partial`: parser/emitter present, but runtime behavior is incomplete or narrowed.
- `Missing`: parser accepts syntax but emitted opcode/surface has no executor handler.
- `Rejected`: parser deterministically rejects the form (`PRS_0505`/`PRS_0504`).

## 2. Statement-Level Partial/Missing

### 2.1 DDL/DCL Gaps

- `CREATE TYPE`: parser is detailed, emitter payload is minimal, no V3 create-type execution path.
  - Parser: `src/parser/parser_v3.cpp:5546`
  - Emitter: `src/parser/v3_emitter.cpp:1990`
  - Executor: no `SBLR3_CREATE_TYPE` dispatch in V3 mutation switch (`src/sblr/executor.cpp`)
- `CREATE DATABASE EMULATED`: parser captures full contract, emitter emits minimal `SBLR3_CREATE_DATABASE` payload, executor v3 mutation switch routes `SBLR3_CREATE_DATABASE_EMULATED` instead.
  - Parser: `src/parser/parser_v3.cpp:3925`
  - Emitter: `src/parser/v3_emitter.cpp:1517`
  - Executor v3 dispatch: `src/sblr/executor.cpp:58592`, `src/sblr/executor.cpp:60419`
- `ALTER SEARCH INDEX` and `ALTER VECTOR INDEX`: only `REBUILD` accepted.
  - Parser rejections: `src/parser/parser_v3.cpp:5840`, `src/parser/parser_v3.cpp:5863`
- `REVOKE` privilege coverage is narrower than `GRANT` (`TRUNCATE`, `REFERENCES`, `TRIGGER`, `USAGE` accepted in `GRANT`, not in `REVOKE`).
  - Grant: `src/parser/parser_v3.cpp:15591`
  - Revoke: `src/parser/parser_v3.cpp:15676`
- `SET PARSER VERSION` is parsed then rejected.
  - `src/parser/parser_v3.cpp:13479`
- `SET TERM` is parser+emitter accepted in `0.1.0`, but delimiter-switch script splitting remains caller/client workflow (native `parseStatements()` stays semicolon-delimited).
  - AST: `include/scratchbird/parser/ast_v3.h:3994`
  - Parser branch: `src/parser/parser_v3.cpp`
  - Emitter switch: `src/parser/v3_emitter.cpp:3408`
- Lifecycle-closure partials (object families without full `CREATE+ALTER+SHOW+DROP` surfaces):
  - `ACCESS METHOD`, `STATISTICS`, `TRANSFORM` are create-only in parser dispatch.
    - Create dispatch: `src/parser/parser_v3.cpp:1173`, `src/parser/parser_v3.cpp:1179`, `src/parser/parser_v3.cpp:1185`
    - No corresponding alter/drop dispatch entries in `parseAlter()`/`parseDrop()` (`src/parser/parser_v3.cpp:6728`, `src/parser/parser_v3.cpp:8156`)
  - `FOREIGN DATA WRAPPER` has create path but no alter/drop command family.
    - Create: `src/parser/parser_v3.cpp:5071`
  - `MEASUREMENT` supports create/alter but has no drop family.
    - Create: `src/parser/parser_v3.cpp:1892`
    - Alter: `src/parser/parser_v3.cpp:6102`
  - `PUBLICATION`/`SUBSCRIPTION` support create/drop but no alter family.
    - Create: `src/parser/parser_v3.cpp:2456`, `src/parser/parser_v3.cpp:2477`
    - Drop: `src/parser/parser_v3.cpp:8441`, `src/parser/parser_v3.cpp:8474`
  - `EXCEPTION` supports create/drop but no alter family.
    - Create: `src/parser/parser_v3.cpp:5755`
    - Drop: `src/parser/parser_v3.cpp:8902`
- External database connection command family is parser+emitter present (`CREATE/ALTER/DROP DATABASE CONNECTION`)
  but still normalized through `AlterSystemStmt` key contracts in `0.1.0` instead of a dedicated catalog-object
  lifecycle opcode family.
  - Parser: `src/parser/parser_v3.cpp`
  - Emitter: `src/parser/v3_emitter.cpp`
  - Runtime: interpreted through generic admin/config pathways.
- FDW connection-building primitives are parser+emitter present but not closed in v3 executor opcode dispatch:
  - `CREATE SERVER` (`CreateForeignServerStmt`) parser: `src/parser/parser_v3.cpp:4760`
  - `CREATE USER MAPPING` parser: `src/parser/parser_v3.cpp:5039`
  - emitter opcodes: `SBLR3_CREATE_FOREIGN_SERVER`, `SBLR3_CREATE_USER_MAPPING` (`src/parser/v3_emitter.cpp:1711`, `src/parser/v3_emitter.cpp:1783`)
  - executor v3 dispatch does not route these opcodes through `executeDdlMutationOpcode`/session/admin switches (`src/sblr/executor.cpp:60360`)
  - Result: connection-definition flow is split and not runtime-closed as a canonical external connection object in `0.1.0`.

### 2.2 Deterministic Rejected Forms (Not Closed in 0.1.0)

Parser explicitly rejects/limits these surfaces:

- `CREATE OR ALTER` on search/vector/measurement/schedule/connection rule/token/quota profile/extension/publication/subscription/access method/statistics/transform.
  - `src/parser/parser_v3.cpp:1025`, `src/parser/parser_v3.cpp:1036`, `src/parser/parser_v3.cpp:1043`, `src/parser/parser_v3.cpp:1050`, `src/parser/parser_v3.cpp:1064`, `src/parser/parser_v3.cpp:1074`, `src/parser/parser_v3.cpp:1088`, `src/parser/parser_v3.cpp:1097`, `src/parser/parser_v3.cpp:1103`, `src/parser/parser_v3.cpp:1109`, `src/parser/parser_v3.cpp:1119`, `src/parser/parser_v3.cpp:1125`, `src/parser/parser_v3.cpp:1131`
- Native extension clause forms with explicit `Unsupported ... surface` rejections (doc-path filter, ts bucket, search DSL, vector ANN, hybrid bridge, graph path, redis lua/stream, UDR compile variants).
  - `src/parser/parser_v3.cpp:14297`, `src/parser/parser_v3.cpp:14373`, `src/parser/parser_v3.cpp:14549`, `src/parser/parser_v3.cpp:14619`, `src/parser/parser_v3.cpp:14699`, `src/parser/parser_v3.cpp:14764`, `src/parser/parser_v3.cpp:14885`, `src/parser/parser_v3.cpp:14991`, `src/parser/parser_v3.cpp:15185`, `src/parser/parser_v3.cpp:15366`

### 2.3 vNext Command Families: Parser Coverage vs Runtime Closure

Method:

- compared opcode symbols referenced in `src/sblr/executor.cpp` vs symbols emitted in `src/parser/v3_emitter.cpp`
- filtered to command/control families (`ADMIN_`, `CLUSTER_`, `CONFIG_`, `SECURITY_`, etc.)
- result:
  - Native v3 parser/emitter now maps the NoSQL bridge families (`SBLR3_CQL_*`, `SBLR3_MONGO_*`,
    `SBLR3_CYPHER_*`, `SBLR3_REDIS_*`, `SBLR3_MILVUS_*`) and admin/cluster/service-channel families
    (`SBLR3_ADMIN_*`, `SBLR3_CLUSTER_*`, `SBLR3_SERVICE_CHANNEL_*`) to concrete command surfaces.
  - These families remain **runtime-partial** in `0.1.0`: executor dispatch routes them through
    `executeVNextOpcode` and currently returns deterministic semantic-bridge rejection (`IRX_0406`) until
    explicit semantic handlers are implemented.

Additional executor-only command families:

- Config/session control:
  - `SBLR3_SESSION_RESET`
  - `SBLR3_CONFIG_RESET`, `SBLR3_CONFIG_HISTORY`, `SBLR3_CONFIG_RELOAD`
  - `SBLR3_CONFIG_RESOURCE_BUNDLES_SHOW`, `SBLR3_CONFIG_RESOURCE_BUNDLE_VALIDATE`, `SBLR3_CONFIG_RESOURCE_BUNDLE_ACTIVATE`
- Text-search admin:
  - `SBLR3_TEXTSEARCH_CREATE_DICTIONARY`, `SBLR3_TEXTSEARCH_ALTER_DICTIONARY`, `SBLR3_TEXTSEARCH_DROP_DICTIONARY`
  - `SBLR3_TEXTSEARCH_CREATE_CONFIGURATION`, `SBLR3_TEXTSEARCH_ALTER_CONFIGURATION`, `SBLR3_TEXTSEARCH_DROP_CONFIGURATION`
  - `SBLR3_TEXTSEARCH_LOAD_DICTIONARY_DATA`
- Security/control-plane:
  - `SBLR3_SECURITY_ENCRYPTION_PROFILE`, `SBLR3_SECURITY_ENCRYPTION_KEY`
  - `SBLR3_SECURITY_KEY_SHARD_SUBMIT`, `SBLR3_SECURITY_UNLOCK_DATABASE`
  - `SBLR3_SECURITY_CERT_DDL`, `SBLR3_SECURITY_PRIVATE_KEY_ROTATE`, `SBLR3_SECURITY_SHOW_STATUS`
- Alert/healing:
  - `SBLR3_ALERT_RULE_DDL`, `SBLR3_ALERT_TARGET_DDL`, `SBLR3_ALERT_ROUTE_DDL`, `SBLR3_ALERT_SILENCE_DDL`, `SBLR3_ALERT_ACK`, `SBLR3_ALERT_SHOW`
  - `SBLR3_HEALING_POLICY_DDL`, `SBLR3_HEALING_ACTION_DDL`, `SBLR3_HEALING_RUN`, `SBLR3_HEALING_SHOW_RUNS`
- Shard/cube/job-type:
  - `SBLR3_SHARD_POLICY_DDL`, `SBLR3_SHARD_DDL`, `SBLR3_SHARD_REPLICA_DDL`, `SBLR3_SHARD_MIGRATE`, `SBLR3_SHARD_SHOW`
  - `SBLR3_CUBE_DDL`, `SBLR3_CUBE_REFRESH`, `SBLR3_CUBE_SHOW_STATS`
  - `SBLR3_JOB_TYPE_DDL`, `SBLR3_JOB_TYPE_PARAM_SET`
Cube-specific status:

- Parser+emitter now expose both cube query grouping (`GROUP BY CUBE(...)`) and cube-object command families
  (`CREATE|ALTER|DROP CUBE`, `REFRESH CUBE`, `SHOW CUBE STATS`/`CUBE SHOW STATS`) mapped to
  `SBLR3_CUBE_DDL`, `SBLR3_CUBE_REFRESH`, and `SBLR3_CUBE_SHOW_STATS`.
  - Parser object-surface dispatch: `src/parser/parser_v3.cpp`
  - Emitter opcode mapping: `src/parser/v3_emitter.cpp`
- Remaining gap is runtime semantic closure: these opcodes currently route through vNext bridge handling
  (`IRX_0406`) until dedicated semantic handlers are implemented.

Important related partial:

- `SBLR3_CREATE_DATABASE_EMULATED` exists in executor/vNext dispatch, but `CREATE DATABASE EMULATED` parser path currently emits `SBLR3_CREATE_DATABASE` payload shape instead of directly emitting this vNext opcode.
  - Parser: `src/parser/parser_v3.cpp:3925`
  - Emitter: `src/parser/v3_emitter.cpp:1517`
  - Executor dispatch includes vNext opcode: `src/sblr/executor.cpp:58592`, `src/sblr/executor.cpp:60419`

## 3. Operator Coverage Matrix

### 3.1 Implemented

- Arithmetic/comparison/logical core: `+ - * / DIV %`, `= <> < <= > >=`, `AND OR NOT`, `IS [NOT] NULL`, `IS [NOT] DISTINCT FROM`.
- Bitwise core: `& | ^ << >>`, unary `~`.
- Predicate forms: `STARTING WITH`, `CONTAINING`.
- JSON binary extract: `->`, `->>`.
- Pattern operators: `LIKE`, `ILIKE`, `~`, `~*`, `!~`, `!~*`.
- Membership/value-list operators: `IN (...)`, `NOT IN (...)`.
- JSON hash extract operators: `#>`, `#>>`.
- Array containment operators: `@>`, `<@`, `&&`.
- Concat operator `||` (runtime `SBLR3_FUNC_CONCAT`).

Evidence:

- Parser expression chain: `src/parser/parser_v3.cpp:10637`
- Emitter opcode mapping: `src/parser/v3_emitter.cpp:4556`
- Executor expression eval switch: `src/sblr/executor.cpp:41936`

### 3.2 Remaining Partial (After V3 Evaluator Closure)

- Value-list membership is now closed (`IN (...)`, `NOT IN (...)`), but subquery-membership (`IN (subquery)`, `NOT IN (subquery)`) remains partial in `0.1.0`.
  - Emitter: `SBLR3_IN_LIST`, `SBLR3_SUBQUERY_IN`, `SBLR3_SUBQUERY_NOT_IN` (`src/parser/v3_emitter.cpp`)
  - Executor: `SBLR3_IN_LIST` is evaluated; subquery-membership opcodes still require dedicated subquery membership routing in V3 expression execution.
- Array containment operators (`@>`, `<@`, `&&`) are now evaluated through structural JSON-array semantics in V3 evaluator path; full engine-specific operator-class parity remains pending.
  - Emitter: `SBLR3_ARRAY_CONTAINS`, `SBLR3_ARRAY_CONTAINED_BY`, `SBLR3_ARRAY_OVERLAP`
  - Executor: handlers present in V3 expression evaluator switch (`src/sblr/executor.cpp`).

### 3.3 Partial

- `?`, `?|`, `?&` are parsed distinctly (`JSON_EXISTS`, `JSON_EXISTS_ANY`, `JSON_EXISTS_ALL`) but emitter collapses all three to `SBLR3_FUNC_JSON_EXISTS`.
  - Parser: `src/parser/parser_v3.cpp:10934`, `src/parser/parser_v3.cpp:10941`, `src/parser/parser_v3.cpp:10948`
  - Emitter collapse: `src/parser/v3_emitter.cpp:4594`

## 4. Function Coverage Matrix

### 4.1 Parser Surface

- Generic function calls are accepted by identifier path + argument list.
- Special validation exists for:
  - `COUNT(*)`, aggregate `ORDER BY`, `FILTER`, `OVER`
  - window arity checks (`ROW_NUMBER`, `RANK`, `DENSE_RANK`, `LAG`, `LEAD`, `FIRST_VALUE`, `LAST_VALUE`, `NTH_VALUE`)
  - feature-gated builtins (`DOC_PATH_*`, `TS_*`, `SEARCH_*`, `VECTOR_*`)

Evidence:

- `src/parser/parser_v3.cpp:12156`
- `src/parser/parser_v3.cpp:12293`
- `src/parser/parser_v3.cpp:12332`
- `src/parser/parser_v3.cpp:12390`

### 4.2 Implemented Runtime Function Closures

- `ABS`, `SIN`, `COS`, `TAN`, `POWER`, `CONCAT` are now closed in the V3 expression evaluator switch.
  - Emitter map: `src/parser/v3_emitter.cpp`
  - Executor handlers: `src/sblr/executor.cpp` (V3 eval switch).

### 4.3 Partial

- Window function emission supports only `ROW_NUMBER`, `RANK`, `DENSE_RANK`; other parsed window names are emitted as fallback `SBLR3_WIN_ROW_NUMBER`.
  - Parser accepts full set: `src/parser/parser_v3.cpp:12293`
  - Emitter fallback: `src/parser/v3_emitter.cpp:4755`
- `FunctionCallExpr::distinct` is emitted for aggregate payloads, but parser does not set `distinct` in `parseFunctionCall` (so `COUNT(DISTINCT ...)` path is not closed in parser).
  - AST field: `include/scratchbird/parser/ast_v3.h:3181`
  - Emitter usage: `src/parser/v3_emitter.cpp:4785`
  - Parser function-call implementation: `src/parser/parser_v3.cpp:12156`

## 5. Cast/Conversion Coverage Matrix

### 5.1 Explicit Casts (Implemented)

- `CAST(expr AS type [USING format])`
- `expr::type`
- `USING` formats recognized in executor: `HEX`, `BASE64`, `ESCAPE`

Evidence:

- Parser: `src/parser/parser_v3.cpp:12514`, `src/parser/parser_v3.cpp:11525`
- Emitter: `src/parser/v3_emitter.cpp:4802`
- Executor: `src/sblr/executor.cpp:42431`

### 5.2 Implicit Conversion Rules (Implemented, Policy-Controlled)

- Implicit text->numeric/temporal/comparison coercions are applied for eligible operator families.
- `operator.strict_mode` disables implicit casts for these operator paths.
- Temporal arithmetic supports date/time/timestamp with interval (+/-), and temporal-temporal subtraction to interval.

Evidence:

- Implicit normalization: `src/sblr/executor.cpp:1849`
- Temporal arithmetic: `src/sblr/executor.cpp:1491`
- Strict-mode setting read/write: `src/sblr/executor.cpp:59305`, `src/sblr/executor.cpp:67757`

### 5.3 Partial/Unsupported

- Implicit comparison coercion remains unsupported for certain operand type combinations.
  - `src/sblr/executor.cpp:2003`, `src/sblr/executor.cpp:2165`
- `DIV_INT`, modulo, and bitwise families reject wide numeric operands.
  - `src/sblr/executor.cpp:42356`, `src/sblr/executor.cpp:42368`, `src/sblr/executor.cpp:42380`

## 6. Beta Readiness Conclusion

- Parser v3 surface is broad and major expression/runtime gaps for scalar function and operator families were closed in this audit cycle (`LIKE/ILIKE`, regex operators, JSON hash extract, array containment, concat-op, and `ABS/SIN/COS/TAN/POWER/CONCAT`).
- `0.1.0` remains a feature-surface beta because partials still include subquery-membership (`IN (subquery)`), collapsed `JSON_EXISTS_ANY`/`JSON_EXISTS_ALL` opcode mapping, known window/aggregate distinct parser-emitter closure items, and incomplete lifecycle closure for several object families (`ACCESS METHOD`, `STATISTICS`, `TRANSFORM`, `FOREIGN DATA WRAPPER`, `MEASUREMENT`, `PUBLICATION`/`SUBSCRIPTION`, `EXCEPTION`).
