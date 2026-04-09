# ScratchBird v3 EBNF vs Parser Parity Report

Timestamp: `20260310T120313Z`  
Status: in-progress evidence snapshot preserved before EBNF rewrite

## Scope

This report records confirmed mismatches between:

- EBNF source: `/home/dcalford/CliWork/ScratchBird-v3-Complete-EBNF.md`
- Live parser source of truth: `src/parser/parser_v3.cpp`
- Live schema-path parser source of truth: `src/parser/schema_path_v3.cpp`

The goal of this report is preservation of evidence only. Every item below is based on direct parser code inspection. Nothing here is inferred from design intent.

## Primary parser truth points

- Top-level statement dispatch: `parser_v3.cpp:657`
- `CREATE` dispatch: `parser_v3.cpp:1069`
- `ALTER` dispatch: `parser_v3.cpp:6938`
- `DROP` dispatch: `parser_v3.cpp:8711`
- `SHOW`: `parser_v3.cpp:15063`
- `DESCRIBE`: `parser_v3.cpp:15575`
- `VALIDATE INDEX`: `parser_v3.cpp:15970`
- Additional statement surfaces begin around:
  - `DOC PATH FILTER`: `parser_v3.cpp:16069`
  - `TS BUCKET AGG`: `parser_v3.cpp:16160`
  - `SEARCH DSL`: `parser_v3.cpp:16218`
  - `CLUSTER/CUBE/SERVICE control`: `parser_v3.cpp:16505`
  - `NoSQL/vector/graph/redis/UDR compile`: `parser_v3.cpp:17222`
- `CALL`: `parser_v3.cpp:19855`
- Schema path implementation: `schema_path_v3.cpp:71`

## Confirmed hallucinated or non-parser structures in the current EBNF

### 1. Schema-path grammar in the EBNF does not match the live parser

Current EBNF path section starts around:

- `ScratchBird-v3-Complete-EBNF.md:2891`

Current EBNF defines path families such as:

- `absolute_env_path ::= "!:" environment_path`
- `relative_env_path ::= ".:" environment_path`
- `parent_env_path ::= "..:" environment_path`
- `current_env_path ::= "." [ "." environment_path ]`

The live parser does not implement that grammar.

Live parser behavior in `schema_path_v3.cpp` is:

- optional `!:` prefix sets `no_search_path`
- then exactly one of:
  - `.` followed by identifier path components
  - `..` followed by identifier path components
  - identifier, optionally followed by `.` identifier repeats

The live parser does not implement:

- `.:` relative prefix
- `..:` parent prefix
- `absolute_env_path`
- `relative_env_path`
- `parent_env_path`
- `current_env_path` as documented in the EBNF
- the environment/database hierarchy model currently encoded in section 20

### 2. Top-level statements documented in the EBNF but rejected by the live parser

These appear in the EBNF as accepted grammar, but the parser explicitly rejects them:

- `CREATE SCHEDULE`
  - EBNF section: `ScratchBird-v3-Complete-EBNF.md:1506`
  - Parser rejection: `parser_v3.cpp:1129-1132`
- `ALTER SCHEDULE`
  - EBNF section: `ScratchBird-v3-Complete-EBNF.md:1922`
  - Parser rejection: `parser_v3.cpp:6941-6944`
- `DROP SCHEDULE`
  - EBNF section: `ScratchBird-v3-Complete-EBNF.md:2217`
  - Parser rejection: `parser_v3.cpp:8714-8717`
- `VACUUM`
  - EBNF section: `ScratchBird-v3-Complete-EBNF.md:2498-2502`
  - Parser rejection: `parser_v3.cpp:786-788`
- `REINDEX`
  - EBNF section: `ScratchBird-v3-Complete-EBNF.md:2691`
  - No top-level parser route exists; `ALTER INDEX ... REBUILD/REBALANCE/...` is the live route.
- `REFRESH MATERIALIZED VIEW`
  - EBNF section: `ScratchBird-v3-Complete-EBNF.md:2703`
  - No top-level parser route exists; top-level `REFRESH` dispatch is `REFRESH CUBE ...` via `parser_v3.cpp:798-799`
- `LISTEN`, `NOTIFY`, `UNLISTEN`
  - EBNF section: `ScratchBird-v3-Complete-EBNF.md:2709-2713`
  - No top-level parser route exists
- generic `LOAD` statement
  - EBNF section: `ScratchBird-v3-Complete-EBNF.md:2716`
  - Top-level `LOAD` in the parser routes to extension load only via `parseInstallExtensionSurface(true)` at `parser_v3.cpp:773-775`

### 3. Missing parser routes that are not represented as accepted grammar in the current EBNF

These are implemented in the live parser and need explicit grammar coverage:

- `CALL ...`
  - Parser route: `parseStatementInternal` at `parser_v3.cpp:898`
  - Implementation: `parseCall` at `parser_v3.cpp:19855`
- `DROP COMMENT ON ...`
  - `DROP` dispatch: `parser_v3.cpp:8792-8794`
  - Implementation: `parseDropComment` at `parser_v3.cpp:18733`
- `VALIDATE INDEX ...`
  - Top-level dispatch: `parser_v3.cpp:878-880`
  - Implementation: `parser_v3.cpp:15970`
- top-level `VALIDATE [DATABASE ...]`
  - Top-level dispatch: `parser_v3.cpp:878-880`
  - Implemented by `parseAdminControlSurface("VALIDATE")`
- `CREATE CLUSTER ...`
  - `CREATE` dispatch: `parser_v3.cpp:1241-1245`
  - Implementation: `parser_v3.cpp:16505`
- `ALTER CLUSTER ...`
  - `ALTER` dispatch: `parser_v3.cpp:7013-7015`
  - Implementation: `parser_v3.cpp:16570`
- `DROP CLUSTER ...`
  - `DROP` dispatch: `parser_v3.cpp:8786-8788`
  - Implementation: `parser_v3.cpp:16709`
- `CREATE CUBE ...`
  - `CREATE` dispatch: `parser_v3.cpp:1248-1252`
  - Implementation: `parser_v3.cpp:16886`
- `ALTER CUBE ...`
  - `ALTER` dispatch: `parser_v3.cpp:7016-7018`
  - Implementation: `parser_v3.cpp:16924`
- `DROP CUBE ...`
  - `DROP` dispatch: `parser_v3.cpp:8789-8791`
  - Implementation: `parser_v3.cpp:17008`

### 4. SHOW grammar in the EBNF is materially narrower than the live parser

Current EBNF `SHOW` section:

- `ScratchBird-v3-Complete-EBNF.md:2367`

Current EBNF covers a small configuration-oriented subset only.

The live parser supports many additional `SHOW` forms, including:

- scoped metadata forms:
  - `SHOW IN|FROM <path> <object_type> ...`
  - `SHOW ALL [IN|FROM <path>] [name|LIKE pattern] [WITH RECURSIVE ...]`
- object-family forms:
  - `SHOW TABLES`
  - `SHOW DATABASES`
  - `SHOW COLUMNS FROM <path>`
  - `SHOW INDEXES`
  - `SHOW INDEX HEALTH|USAGE|STORAGE|CONTENTION|OPTIONS <path>`
  - `SHOW TABLE`
  - `SHOW TRIGGER`
  - `SHOW VIEW`
  - `SHOW PROCEDURE`
  - `SHOW FUNCTION`
  - `SHOW DOMAIN`
  - `SHOW GENERATOR` / `SHOW SEQUENCE`
  - `SHOW SCHEMA`
  - `SHOW ROLE`
  - `SHOW JOBS`
  - `SHOW JOB`
  - `SHOW JOB RUNS`
  - `SHOW CHECKS`
  - `SHOW COLLATIONS`
  - `SHOW COMMENTS`
  - `SHOW DEPENDENCIES`
  - `SHOW PACKAGE`
- variable/system forms:
  - `SHOW CURRENT_SCHEMA`
  - `SHOW SEARCH_PATH`
  - `SHOW SCHEMA PATH`
  - `SHOW TIME ZONE`
  - `SHOW SQL DIALECT`
  - `SHOW VERSION`
  - `SHOW PARSER VERSION`
  - `SHOW DATABASE`
  - `SHOW SYSTEM`
  - `SHOW METRICS`
- control surfaces:
  - `SHOW CLUSTER ...`
  - `SHOW CUBE ...`

All of those need grammar treatment based on `parser_v3.cpp:15063`.

### 5. DESCRIBE grammar in the EBNF does not match the live parser

Current EBNF `DESCRIBE` section:

- `ScratchBird-v3-Complete-EBNF.md:2381`

The live parser supports two forms only:

- canonical:
  - `DESCRIBE <object_name> OF <object_type> IN|FROM <path> [COMMENT ONLY|FULL|DDL ONLY]`
- compatibility:
  - `DESCRIBE <table> [column]`

That behavior is implemented at `parser_v3.cpp:15575`.

The current EBNF instead lists many direct keyword-led forms such as:

- `DESCRIBE INDEX ...`
- `DESCRIBE DATABASE ...`
- `DESCRIBE SCHEMA ...`

Those are not how the live parser dispatches this statement.

### 6. COMMENT grammar is incomplete and structurally wrong relative to the live parser

Current EBNF `COMMENT` section:

- `ScratchBird-v3-Complete-EBNF.md:2505`

The live parser supports:

- compatibility form:
  - `COMMENT ON <object_type> <path> IS <string|NULL>`
- canonical form:
  - `COMMENT ON <object_name> OF <object_type> IN|FROM <path> IS <string|NULL>`
- drop form:
  - `DROP COMMENT ON ...`

Implemented in:

- `parseComment`: `parser_v3.cpp:18438`
- `parseDropComment`: `parser_v3.cpp:18733`

The current EBNF does not model the canonical `OF <type> IN|FROM <path>` form and does not model `DROP COMMENT ON`.

### 7. Additional-surface sections use placeholder nonterminals that are not defined anywhere

The current EBNF contains placeholder structures instead of real grammar for several live parser surfaces.

Undefined placeholders found by direct production scan:

- `search_specification`
- `vector_specification`
- `graph_specification`
- `stream_command`
- `nosql_operation`
- `bridge_specification`
- `cluster_operation`
- `show_specification`
- `cube_operation`
- `service_operation`

These appear around:

- `ScratchBird-v3-Complete-EBNF.md:2756-2822`

The live parser does not consume those placeholder nonterminals. It consumes concrete token sequences in:

- `parseSearchDslSurface`: `parser_v3.cpp:16218`
- `parseVectorAnnSurface`: `parser_v3.cpp:17280`
- `parseGraphPathSurface`: `parser_v3.cpp:17407`
- `parseRedisStreamGroupSurface`: `parser_v3.cpp:17634`
- `parseHybridBridgeSurface`: `parser_v3.cpp:17342`
- `parseShowClusterControlSurface`: `parser_v3.cpp:16853`
- `parseCubeControlSurface`: `parser_v3.cpp:17101`
- `parseShowCubeControlSurface`: `parser_v3.cpp:17155`
- `parseServiceChannelSurface`: `parser_v3.cpp:17192`

## Undefined production names currently present in the EBNF

The following names are referenced by productions in the current EBNF but are not themselves defined as productions in the document as currently written:

- `cte_name`
- `column_alias`
- `cursor_name`
- `data_type`
- `opclass_options`
- `field_name`
- `attribute_name`
- `procedure_statement`
- `job_identifier`
- `schedule_identifier`
- `qualified_rule_name`
- `qualified_token_name`
- `qualified_profile_name`
- `qualified_channel_name`
- `new_index_name`
- `new_schema_name`
- `new_owner`
- `new_tablespace_name`
- `new_version`
- `qualified_udr_name`
- `savepoint_name`
- `transaction_id`
- `configuration_parameter`
- `value`
- `provider`
- `default_value`
- `variable_name`
- `gds_code`
- `connection_name`
- `search_specification`
- `vector_specification`
- `graph_specification`
- `stream_command`
- `nosql_operation`
- `bridge_specification`
- `source`
- `cluster_operation`
- `show_specification`
- `cube_operation`
- `cube_name`
- `service_operation`
- `job_run_uuid`
- `table_definition`
- `any_printable_ascii_except_quote`
- `catalog_name`

This list is evidence of internal EBNF incompleteness, not parser capability.

## Working correction direction

The next EBNF revision should:

- treat `ScratchBird/src/parser/parser_v3.cpp` and `ScratchBird/src/parser/schema_path_v3.cpp` as authoritative
- remove top-level statements the live parser explicitly rejects
- add missing live parser routes that are currently absent
- replace placeholder grammar with concrete grammar derived from parser functions
- rewrite section 20 schema paths to match `schema_path_v3.cpp`
- rewrite section 22 complete-statement coverage so it references only real, defined productions

## Preservation note

This file exists to prevent loss of the current mismatch inventory while the root EBNF file is being corrected.
