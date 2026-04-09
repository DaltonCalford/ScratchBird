# ScratchBird Emulation AST/V3/SBLR Gap Analysis

## Scope

This audit answers one question: for each donor database or database platform in the current emulation set, which parser, dialect, function, command, and statement surfaces already have a usable ScratchBird AST/SBLR equivalent, and which surfaces still need new AST or SBLR structures before a donor parser can map to them 1:1.

The source of truth for the ScratchBird side is:

- `include/scratchbird/parser/ast_v3.h`
- `src/parser/parser_v3.cpp`
- `src/parser/v3_emitter.cpp`

The source of truth for the donor side is the local reference clone tree under `docs/reference/project_clones`.

The audit is intentionally local-source-only. If a donor surface is not backed by local source, it is not promoted beyond `LOCAL_EVIDENCE_GAP`.

## Current ScratchBird Baseline

The current v3 model is broader than the native parser surface suggests. The important existing carriers are:

- `FunctionCallExpr` already carries the function path, positional arguments, aggregate `DISTINCT`, aggregate `FILTER`, aggregate inner `ORDER BY`, and an attached `WindowSpec` (`ast_v3.h:3319-3335`).
- `TableRefNode` already carries table, subquery, and function-table sources, plus `LATERAL`, `WITH ORDINALITY`, and PostgreSQL-style `TABLESAMPLE` (`ast_v3.h:3407-3438`, `parser_v3.cpp:10523-10612`).
- `SelectStmt` already carries `DISTINCT ON`, `GROUP BY`, `ROLLUP`, `CUBE`, `GROUPING SETS`, named windows, limit or offset or fetch, and lock strengths (`ast_v3.h:3581-3635`, `parser_v3.cpp:9982-9988`, `parser_v3.cpp:10770-10812`).
- Recursive CTE `SEARCH` and `CYCLE` are already represented in the AST and parsed in the v3 grammar (`ast_v3.h:3659-3679`, `parser_v3.cpp:9880-9939`).
- `CreateViewStmt` already has a `materialized` flag and `with_data` support even though the native parser still rejects the donor prefix spelling `CREATE MATERIALIZED VIEW` (`ast_v3.h:877-900`, `parser_v3.cpp:1136-1142`).
- `CALL`, `MERGE`, `COPY`, `RETURNING`, and `EXECUTE PROCEDURE` already have first-class AST coverage (`ast_v3.h:2530-2608`, `parser_v3.cpp:833`, `parser_v3.cpp:915-916`, `parser_v3.cpp:953-954`, `parser_v3.cpp:20020-20118`).
- The native parser already supports canonical Redis stream and Lua surfaces, and the emitter already understands internal key-text lowering for CQL, Mongo, Cypher, Redis PubSub, and Milvus (`parser_v3.cpp:17832-18072`, `v3_emitter.cpp:3091-3170`).

The limiting factor is not lack of all generic structure. The main issues are:

- Native v3 still hard-refuses some donor spellings that could be desugared into existing structures.
- `SelectStmt` and `TableRefNode` are still missing several donor clause families.
- `FunctionCallExpr` lacks metadata for ordered-set aggregates, variadic markers, named arguments, null-treatment modifiers, and `FROM FIRST` or `FROM LAST`.
- The emitter erases the identity of any window function outside the current eight-name whitelist by lowering unknown window calls to `SBLR3_WIN_ROW_NUMBER` (`parser_v3.cpp:13761-13816`, `v3_emitter.cpp:5938-6004`).
- The native parser still exposes only `REDIS` inside `parseNoSqlSurface()`, even though the emitter already carries more multi-model opcodes (`parser_v3.cpp:17464-17520`, `v3_emitter.cpp:3091-3170`).

## Shared Findings

### 1. Native v3 hard-refusals that should become desugars

These donor surfaces already have a usable structural landing zone and should stop terminating in `PRS_0505`:

- `APPLY` should desugar to `JOIN LATERAL` or `LEFT JOIN LATERAL`. ScratchBird already carries `TableRefNode.lateral` and standard join nodes, but the parser hard-refuses `APPLY` today (`parser_v3.cpp:10663-10669`, `parser_v3.cpp:10710-10729`).
- `CREATE MATERIALIZED VIEW` prefix spelling should lower into `CreateViewStmt.materialized = true` instead of failing (`ast_v3.h:877-900`, `parser_v3.cpp:1136-1142`).
- MySQL-family `REPLACE INTO`, `INSERT IGNORE`, and `ON DUPLICATE KEY UPDATE` should lower into existing insert/conflict machinery or an explicit insert conflict alias layer instead of failing at the syntax boundary (`parser_v3.cpp:670`, `parser_v3.cpp:11050`, `parser_v3.cpp:11107`, `ast_v3.h:3685-3759`).
- Donor procedure families that already arrive as generic `CALL` or generic function invocation do not need new AST nodes. Citus administrative UDFs and Dolt `DOLT_*` procedures fit current `FunctionCallExpr` and `ExecuteProcedureStmt` structures.

### 2. Shared `SelectStmt` or `TableRefNode` gaps

The following donor surfaces do not have a direct current field or node:

- `QUALIFY`
- `JSON_TABLE`
- `PIVOT`
- `UNPIVOT`
- `MATCH_RECOGNIZE`
- `ROWS FROM (...)` with multiple table functions
- temporal table ornaments such as `AS OF`, `AS OF SYSTEM TIME`, `AS OF TIMESTAMP`, `FOR VALID_TIME`, and `FOR SYSTEM_TIME`
- ClickHouse table-query ornaments such as `PREWHERE`, `ARRAY JOIN`, `LIMIT BY`, `FINAL`, and query-local `SETTINGS`

The current proof is negative as well as positive:

- `SelectStmt` has no `qualify` field in `ast_v3.h:3581-3635`.
- `TableRefNode` currently has only `TABLE`, `SUBQUERY`, `FUNCTION`, and `JOIN` variants in `ast_v3.h:3407-3418`.
- There are no hits for `QUALIFY`, `JSON_TABLE`, `ROWS FROM`, `MATCH_RECOGNIZE`, `PREWHERE`, `LIMIT BY`, or `ARRAY JOIN` in `parser_v3.cpp` or `ast_v3.h`.
- `PIVOT` and `UNPIVOT` are explicitly refused in `parser_v3.cpp:10663-10665`.

### 3. Shared `FunctionCallExpr` metadata gaps

Current `FunctionCallExpr` is still positional-only. That is sufficient for generic scalar calls, but not for several donor families:

- PostgreSQL `VARIADIC` arguments and named-argument call syntax (`gram.y:8744`, `gram.y:15856-15866`, `src/test/regress/expected/polymorphism.out:1479`)
- ordered-set and inverse-distribution aggregates using `WITHIN GROUP` (`postgresql/gram.y:15935-15957`, `postgresql/gram.y:16567`, `firebird/parse.y:8723`, `mariadb/sql_yacc.yy:11888`)
- MySQL-family and Vitess `RESPECT NULLS`, `IGNORE NULLS`, and `FROM FIRST` or `FROM LAST` on window functions (`mysql/sql_yacc.yy:11498-11528`, `mysql/sql_yacc.yy:11599-11603`, `vitess/sql.y:6414-6438`, `tidb/parser.y:10269-10341`)

`FunctionCallExpr.arguments` is just `std::vector<Expression*>`, and there is no argument-mode or argument-name carrier in `ast_v3.h:3324-3334`. The lexer does have `COLON_EQUALS`, `ARROW`, and `DOUBLE_ARROW` tokens, but current parser use is assignment and JSON extraction, not function argument metadata (`lexer_v3.cpp:737-854`, `parser_v3.cpp:12861-12887`, `parser_v3.cpp:19517-19529`).

### 4. Shared window-function execution gap

Even when the parser can already read a donor window invocation generically, ScratchBird still loses semantics for any name outside:

- `ROW_NUMBER`
- `RANK`
- `DENSE_RANK`
- `LAG`
- `LEAD`
- `FIRST_VALUE`
- `LAST_VALUE`
- `NTH_VALUE`

The problem is structural:

- `parser_v3.cpp:13761-13816` only validates those eight names as explicit window families.
- `v3_emitter.cpp:5938-5958` emits dedicated opcodes for those eight only.
- `v3_emitter.cpp:5955-5958` collapses every other window function to `SBLR3_WIN_ROW_NUMBER`.

That means PostgreSQL, MySQL, MariaDB, TiDB, Vitess, Firebird, and DuckDB window functions like `CUME_DIST`, `PERCENT_RANK`, and `NTILE` do not have a faithful current SBLR lowering path.

### 5. Shared multi-model command-envelope gap

The emitter already has internal key-text lowering for:

- `nosql.cql.*`
- `nosql.mongo.*`
- `nosql.cypher.*`
- `nosql.redis.*`
- `nosql.milvus.*`

But the native parser still rejects engine-prefixed `CQL`, `MONGO`, `CYPHER`, and `MILVUS` at entry (`parser_v3.cpp:697-700`) and `parseNoSqlSurface()` only accepts `REDIS` (`parser_v3.cpp:17488-17520`).

This means several donor parsers could map to existing AST-plus-emitter paths today if they construct AST directly, but the native v3 grammar does not expose those carriers yet.

### 6. Donor tokens that are not current emulation requirements

Not every donor token found in source should turn into a ScratchBird backlog item.

- MariaDB `WITH CUBE` is reserved but intentionally aborts as unsupported in donor source (`mariadb/sql_yacc.yy:12930-12942`). It is not a shipped donor requirement.
- DuckDB has grammar support for `ROWS FROM (...)`, but the transformer still throws `NotImplementedException("ROWS FROM() not implemented")` (`duckdb/transform_table_function.cpp:9`). That is not a donor-shipped requirement either.
- PostgreSQL regression output explicitly shows `RESPECT NULLS` and `IGNORE NULLS` failing for `percent_rank`, `cume_dist`, and `ntile` (`src/test/regress/sql/window.sql:2088-2096`). Null-treatment is not a PostgreSQL requirement, even though it is a MySQL-family requirement.

## Engine-By-Engine Findings

## Apache Ignite

- Donor source confirms `APPLY`, `QUALIFY`, `PIVOT`, `UNPIVOT`, and `MATCH_RECOGNIZE` as first-class grammar elements (`IgniteSqlParserImplConstants.java:25`, `:335`, `:435`, `:454`, `:679`; `IgniteSqlParserImpl.java:3132-3147`, `:3939-3942`, `:4062-4162`, `:4218-4234`).
- `APPLY` can target existing `JoinNode` plus `TableRefNode.lateral`; this is a native-v3 desugar gap, not an AST gap.
- `QUALIFY`, `PIVOT`, `UNPIVOT`, and `MATCH_RECOGNIZE` have no current `SelectStmt` or `TableRefNode` carrier and therefore require shared AST expansion buckets.
- Ordered-set handling also matters because Ignite’s parser carries `WITHIN GROUP` decisions (`IgniteSqlParserImpl.java:5454`, `:9307`).

## Cassandra

- Donor source confirms `CREATE/ALTER/DROP KEYSPACE`, `APPLY BATCH`, `USING TIMESTAMP`, selector functions `TTL()` and `WRITETIME()`, and `CREATE/ALTER/DROP MATERIALIZED VIEW` (`Parser.g:563-732`, `:951`, `:1124`, `:1190`, `:1283`, `:1399`; `CQL.textile:172-243`, `:592-643`, `:1026-1151`).
- `CREATE MATERIALIZED VIEW` is structurally close to current `CreateViewStmt.materialized`, but native v3 still refuses the donor prefix spelling.
- The CQL batch/keyspace/ttl/writetime runtime hooks already exist in `v3_emitter.cpp:3091-3101`; the missing piece is canonical parser exposure plus a stable AST carrier for those command families.
- There are no current `USE DATABASE`, `KEYSPACE`, or CQL-specific batch or timestamp statement carriers in `parser_v3.cpp` or `ast_v3.h`.

## Citus

- Citus administrative entry points are exposed as PostgreSQL functions and upgrade SQL, not as new core grammar families (`citus--8.0-1.sql:581`, `:1318`; `citus--9.5-1--10.0-4.sql:19`, `:28`; `citus--11.0-4--11.1-1.sql:182-185`).
- Current `FunctionCallExpr` and `CALL` support are already sufficient for the parser shape. The main work is extension function catalog or routing, not new AST nodes.
- Citus still inherits PostgreSQL-family gaps such as `JSON_TABLE`, `ROWS FROM`, `VARIADIC`, ordered-set aggregate metadata, and the window-function identity gap.

## ClickHouse

- Donor source confirms `PREWHERE`, `QUALIFY`, `LIMIT BY`, `ARRAY JOIN`, `FINAL`, and per-query `SETTINGS` as real select-query surfaces (`Analyzer/ValidationUtils.cpp:71-87`, `Analyzer/QueryNode.cpp:261-339`, `Analyzer/TableExpressionModifiers.h:10`, `Parsers/ExpressionElementParsers.cpp:1621-1650`).
- None of those surfaces exist in current `SelectStmt` or `TableRefNode`.
- This is not a parser-only issue. ClickHouse requires new AST carriers for table modifiers, select post-filtering, and query-local settings. `TABLESAMPLE` is not sufficient because ClickHouse `SAMPLE/OFFSET/FINAL` and `ARRAY JOIN` are structurally different.

## CockroachDB

- Donor grammar confirms `AS OF SYSTEM TIME`, `ALTER RANGE`, `LOCALITY`, `PLACEMENT`, `CREATE/ALTER CHANGEFEED`, `EXPERIMENTAL SCRUB`, and `UPSERT` (`sql.y:2415-2703`, `:3480-3525`, `:3651-3672`, `:6239`, `:6336-6382`, `:7862-7911`, `:13117-13167`, `:14105-14109`, `:14253-14261`).
- `UPSERT` is a native-v3 syntax gap that can probably target existing insert or conflict machinery, but the exact conflict-target semantics still need explicit lowering rules.
- `AS OF SYSTEM TIME`, `ALTER RANGE`, `LOCALITY`, `PLACEMENT`, `CHANGEFEED`, and `SCRUB` have no current AST carrier.
- Cockroach therefore needs both a temporal-clause bucket and a distribution-or-admin statement bucket.

## Dolt

- Donor source confirms `AS OF` on `SHOW`, `SELECT`, and `CALL`, plus many `DOLT_*` procedures (`diff.go:501`, `:534`, `:1022`; `merge.go:256`; `commit.go:198`; `dolt_queries.go:450-458`, `:7785-8050`).
- `CALL DOLT_*` and `DOLT_LOG()` already fit current `ExecuteProcedureStmt` and `FunctionCallExpr`.
- The gap is the `AS OF` clause. Current `parser_v3.cpp` has no `AS OF` hits, so current AST and parser need a temporal-table or temporal-call carrier.

## DuckDB

- Donor source confirms shipped `QUALIFY` and `PIVOT` or `UNPIVOT` surfaces (`select.y:966`, `:1165-1176`; `select_node.cpp:93`, `:146`; `pivotref.cpp:312`, `:324`).
- Donor grammar also carries `WITHIN GROUP`, `VARIADIC`, and null-treatment metadata for functions (`select.y:2966-3039`, `:3220`; `transform_function.cpp:236`, `:317`, `:328-334`).
- `ROWS FROM (...)` exists in grammar but donor transform code still throws `NotImplementedException` (`transform_table_function.cpp:9`), so it is not a donor-shipped requirement today.
- ScratchBird therefore needs `QUALIFY`, `PIVOT/UNPIVOT`, ordered-set metadata, and window null-treatment expansion for DuckDB; it does not need `ROWS FROM` specifically for DuckDB until donor implementation changes.

## FirebirdSQL

- Donor grammar confirms `WITHIN GROUP`, `FROM FIRST`, `FROM LAST`, and extra window functions such as `CUME_DIST`, `PERCENT_RANK`, and `NTILE` (`parse.y:8723`, `:8756`, `:8763-8764`, `:10022`, `:10046`, `:10051`).
- `FROM FIRST` and `FROM LAST` have no current ScratchBird function metadata field.
- `WITHIN GROUP` ordered-set aggregates need new function-call metadata and SBLR lowering.
- `CUME_DIST`, `PERCENT_RANK`, and `NTILE` hit the shared window-function identity gap in the emitter.

## FoundationDB

- FoundationDB is not a SQL grammar target in the local clone. The local reference surface is `fdbcli` command actors such as `coordinators`, `datadistribution`, `maintenance`, `throttle`, and `versionepoch` (`fdbcli.h:98-115`, `:256-334`).
- ScratchBird has no current canonical command envelope or emitter opcodes for these command families.
- This is a command-envelope and executor-extension problem, not a `SelectStmt` grammar problem.

## immudb

- Donor grammar and tests confirm `USE DATABASE`, `USE SNAPSHOT SINCE/AFTER/BEFORE/UNTIL/TX`, `HISTORY OF`, and `UPSERT INTO` (`sql_grammar.y:81-85`, `:237`, `:242`, `:424`, `:990`, `:1012-1038`; `parser_test.go:73`, `:101-158`, `:478-698`).
- There are no `USE` or `SNAPSHOT` statement carriers in current ScratchBird `parser_v3.cpp`.
- `HISTORY OF` has no current table-source equivalent.
- `UPSERT` is a syntax or lowering gap; whether it becomes a direct alias to current conflict machinery must be specified explicitly during implementation.

## InfluxDB

- Donor rewrite and test code confirm `SHOW MEASUREMENTS`, `SHOW FIELD KEYS`, `SHOW TAG KEYS`, `SHOW TAG VALUES`, and `DELETE FROM` InfluxQL surfaces (`iox_query_influxql_rewrite/src/lib.rs:251-394`, `:527-528`; `tests/server/query.rs:267-343`, `:632-780`).
- `DELETE FROM` already exists generically in ScratchBird SQL, but the InfluxQL `SHOW` family has no current AST or parser equivalent.
- This is a select-or-show statement bucket, not a function-call bucket.

## MariaDB

- Donor source confirms `JSON_TABLE`, `PERCENTILE_CONT`, `PERCENTILE_DISC`, `WITHIN GROUP`, and extra window functions `CUME_DIST`, `PERCENT_RANK`, and `NTILE` (`sql_yacc.yy:572`, `:11888`, `:11911-11917`, `:12378`; `:11766-11801`).
- `JSON_TABLE` needs the shared table-source AST bucket.
- `PERCENTILE_CONT/DISC ... WITHIN GROUP` need the shared ordered-set aggregate bucket.
- `CUME_DIST`, `PERCENT_RANK`, and `NTILE` need the shared window-identity fix in the emitter.
- MariaDB `WITH CUBE` is explicitly not donor-shipped and should not become an emulation requirement (`sql_yacc.yy:12930-12942`).

## Milvus

- The current emitter already carries `create_collection`, `drop_collection`, `create_index`, `drop_index`, `insert`, `delete`, `search`, and `query` opcodes (`v3_emitter.cpp:3148-3170`).
- Donor proxy code confirms those command families plus `HybridSearch` (`impl.go:504-624`, `:2113-2376`, `:2968`, `:3250-3324`, `:3915`; `task_search.go:564-1066`; `task_query.go:387-824`).
- Native v3 still rejects `MILVUS` at entry (`parser_v3.cpp:697-700`), so existing command families need parser exposure.
- `HybridSearch` has no current ScratchBird emitter opcode and needs expansion beyond exposure-only work.

## MongoDB

- The current emitter already knows `find`, `aggregate`, `findAndModify`, and `bulkWrite` (`v3_emitter.cpp:3103-3113`).
- Donor source additionally confirms `distinct`, `count`, `createIndexes`, and `listCollections` (`distinct_access.cpp:412-665`, `count_request.h:38-44`, `create_indexes.idl:194-196`, `list_collections.idl:109-113`, `aggregate_command.idl:88-91`, `find_and_modify.cpp:212-214`, `bulk_write.cpp:377-445`).
- Native v3 still rejects `MONGO` at entry (`parser_v3.cpp:697-700`), so the four existing Mongo opcodes need parser exposure.
- `distinct`, `count`, `createIndexes`, and `listCollections` need new command-envelope items and new lowering or executor handling.

## MySQL

- Donor grammar confirms `REPLACE`, `IGNORE`, `ON DUPLICATE KEY UPDATE`, `JSON_TABLE`, `QUALIFY`, and the window modifier family `CUME_DIST`, `PERCENT_RANK`, `NTILE`, `IGNORE NULLS`, `FROM FIRST`, and `FROM LAST` (`sql_yacc.yy:1077`, `:9308`, `:12404`, `:12714`, `:11474-11528`, `:11599-11603`, `:15029-15030`).
- ScratchBird native v3 hard-refuses `REPLACE INTO`, `INSERT IGNORE`, and `ON DUPLICATE KEY UPDATE` (`parser_v3.cpp:670`, `:11050`, `:11107`).
- `JSON_TABLE` and `QUALIFY` require new AST fields or nodes.
- Window null-treatment, `FROM FIRST/LAST`, and the extra window functions require both AST metadata and emitter fixes.

## Neo4j

- The current emitter already knows `MATCH`, `MERGE`, `UNWIND`, and `CALL` for Cypher lowering (`v3_emitter.cpp:3115-3125`).
- Donor parser tokens and rules additionally confirm `FOREACH` and `USE` graph clauses (`Cypher5Parser.java:54`, `:82`, `:3642-3673` and many clause entry points).
- Native v3 still rejects `CYPHER` at entry (`parser_v3.cpp:697-700`), so the existing four surfaces need parser exposure.
- `FOREACH` and `USE` need command-envelope or graph-AST expansion.

## OpenSearch

- ScratchBird already has a single-search JSON carrier through `SearchQueryDslStmt` (`ast_v3.h:4257-4264`) and the parser exposes `SEARCH QUERY DSL`, `SEARCH JOIN FIELD MAPPING`, and `SEARCH PERCOLATOR FIELD` (`parser_v3.cpp:16408-16566`).
- Donor source also confirms first-class `CreateIndexRequest`, `PutMappingRequest`, `BulkRequest`, and `MultiSearchRequest` request families (`CreateIndexRequest.java:90`, `PutMappingRequest.java:84`, `BulkRequest.java:80`, `MultiSearchRequest.java:78`, `:334`).
- Those request families have no current ScratchBird AST or parser carrier.
- Search aggregation bodies can remain inside the existing JSON payload model, but bulk or multi-search and index-admin envelopes need new carriers.

## PostgreSQL

- Donor source confirms `JSON_TABLE`, `ROWS FROM (...)`, `VARIADIC`, ordered-set aggregates with `WITHIN GROUP`, named-argument calls, and shipped `CUME_DIST`, `PERCENT_RANK`, and `NTILE` window functions (`gram.y:14205-14226`, `:14487-14502`, `:15856-15957`, `:16567`, `src/test/regress/expected/polymorphism.out:1479`, `src/test/regress/sql/window.sql:53-59`, `:2088-2096`, `src/test/regress/sql/aggregates.sql:1058-1060`).
- `CREATE MATERIALIZED VIEW` is a native-v3 syntax refusal even though `CreateViewStmt` already has the right structural flag (`parser_v3.cpp:1136-1142`, `ast_v3.h:877-900`).
- `ROWS FROM` and `JSON_TABLE` need new table-source AST carriers.
- `VARIADIC`, named arguments, and ordered-set aggregates need function metadata expansion.
- `CUME_DIST`, `PERCENT_RANK`, and `NTILE` are currently parseable only as generic window names but lose identity in the emitter.

## Redis

- ScratchBird already parses canonical Redis string/hash/list/set/zset, Lua, and stream-group command surfaces (`parser_v3.cpp:17495-17509`, `:17832-18072`).
- The emitter already carries `nosql.redis.pubsub` (`v3_emitter.cpp:3143-3146`).
- Donor command tables additionally confirm PubSub subscribe-family commands and transaction commands `MULTI`, `EXEC`, `WATCH`, and `UNWATCH` (`commands.def:11814-11827`, `:11967-11970`).
- `PUBSUB` needs parser exposure or a donor-parser-to-canonical mapping rule. Transaction commands have no current opcode or AST carrier and need new command-envelope work.

## SQLite

- No standalone SQLite donor clone was found in the local reference tree during packet generation.
- SQLite remains a `LOCAL_EVIDENCE_GAP` for parser or function surface analysis.
- No structural claim should be promoted beyond that boundary until the donor clone is present locally.

## TiDB

- Donor parser and AST code confirm `AS OF TIMESTAMP`, `FLASHBACK`, `PLACEMENT POLICY`, TTL table options, `SPLIT TABLE ... INDEX ...`, `SPLIT REGION`, and MySQL-family window extensions (`ast/dml.go:4274`, `ast/ddl.go:273`, `:3150-3175`, `:4182-4183`, `sem.go:23-24`, `:41-42`, `:59-60`, `:71-76`, `:116-117`, `parser.y:2290-2293`, `:3326-3475`, `:13306-13334`, `:10245-10341`).
- `AS OF TIMESTAMP` and `FLASHBACK` need temporal statement or table ornaments that do not exist today.
- `PLACEMENT POLICY`, TTL options, and split-region statements need new DDL or admin statement carriers; current `CreateTableStmt` has no generic table-option bag (`ast_v3.h:705-744`).
- TiDB also inherits the shared window metadata and extra-window-function issues.

## Vitess

- Donor grammar confirms `JSON_TABLE`, `QUALIFY`, `IGNORE NULLS`, `RESPECT NULLS`, `FROM FIRST`, `FROM LAST`, `REPLACE`, and `ON DUPLICATE KEY UPDATE` (`sql.y:3953`, `:9300`, `:6414-6438`, `:8589`, `:8732-8734`, `:7776`).
- `REPLACE` and `ON DUPLICATE KEY UPDATE` are native-v3 desugar candidates, not new AST families.
- `JSON_TABLE` and `QUALIFY` need shared AST expansion.
- Vitess also requires the shared window metadata and window-identity fixes.

## XTDB

- Donor source confirms `FOR VALID_TIME` and `FOR SYSTEM_TIME` table or query context in the planner and SQL lowering code (`xtql/plan.clj:415-430`, `sql.clj:313-348`, `:2848-3142`, `node/impl.clj:64`).
- Current ScratchBird AST has no temporal table-reference ornament for this family.
- This is a temporal-clause AST gap rather than a generic function gap.

## YugabyteDB

- YugabyteDB inherits PostgreSQL surfaces and adds `_YB_TABLEGROUP_P`, `_YB_TABLETS_P`, `_YB_COLOCATED_P`, and `_YB_HASH_P` grammar-level distribution options (`gram.y:827-839`, `:3197-3203`, `:3694-3871`, `:4807`, `:4845-4848`, `:5365-5372`).
- All PostgreSQL-family gaps still apply.
- The Yugabyte-specific additions need new DDL storage and distribution-option carriers. Current `CreateTableStmt` only knows generic partitioning and tablespace fields (`ast_v3.h:726-740`), not tablegroup, tablet count, or co-location directives.

## Implementation Order

The cheapest high-value sequence is:

1. Remove native hard-refusals where the current AST already has a safe target: `APPLY`, `CREATE MATERIALIZED VIEW`, and the MySQL-family insert aliases.
2. Add `SelectStmt::qualify`.
3. Add a generalized table-source extension layer for `JSON_TABLE`, `PIVOT`, `UNPIVOT`, `MATCH_RECOGNIZE`, temporal-table ornaments, and ClickHouse select modifiers.
4. Extend `FunctionCallExpr` to carry argument mode, argument name, ordered-set metadata, null-treatment metadata, and `FROM FIRST/LAST`.
5. Replace the eight-name window whitelist in the emitter with a generic window-call identity carrier so `NTILE`, `CUME_DIST`, `PERCENT_RANK`, and future donor window functions survive lowering intact.
6. Expose the already-existing CQL, Mongo, Cypher, Milvus, and Redis PubSub lowering paths through canonical parser entry points.
7. Add new command envelopes and executor opcodes for the remaining donor-only command families: Mongo `count/distinct/createIndexes/listCollections`, Milvus `HybridSearch`, Redis transactions, OpenSearch bulk or multi-search and index-admin, and FoundationDB CLI commands.
8. Fill the SQLite local evidence gap before creating any canonical implementation spec for SQLite emulation.

## Bottom Line

The current ScratchBird parser and AST are not “too small” for the emulation set. They already cover a large amount of the needed structural ground. The real blockers are concentrated in a limited set of missing clause families and metadata carriers:

- `QUALIFY`
- `JSON_TABLE`
- `PIVOT/UNPIVOT`
- `MATCH_RECOGNIZE`
- temporal table or query ornaments
- ClickHouse query ornaments
- ordered-set or variadic or named-argument function metadata
- faithful generic window-function lowering
- native exposure of already-existing multi-model command families

Once those shared buckets are implemented, most donor-specific parsers can target common ScratchBird AST or SBLR structures instead of requiring engine-by-engine bespoke runtime paths.
